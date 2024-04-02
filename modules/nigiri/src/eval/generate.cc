#include "motis/nigiri/eval/commands.h"

#include <regex>

#include "utl/erase_if.h"
#include "utl/parser/cstr.h"
#include "utl/to_vec.h"

#include "conf/configuration.h"
#include "conf/options_parser.h"

#include "motis/module/message.h"
#include "motis/bootstrap/dataset_settings.h"
#include "motis/bootstrap/motis_instance.h"

#include "nigiri/query_generator/query_generator.h"
#include "nigiri/timetable.h"

#include "version.h"

namespace motis::nigiri::eval {

constexpr auto const kTargetEscape = std::string_view{"TARGET"};

struct generator_settings : public conf::configuration {
  generator_settings() : configuration("Generator Options", "") {
    param(query_count_, "query_count", "number of queries to generate");
    param(out_, "out", "file to write generated queries to");
    // param(bbox_, "bbox", "bounding box for locations");
    // param(poly_file_, "poly", "bounding polygon for locations");
    param(start_modes_, "start_modes", "start modes ppr-15|osrm_car-15|...");
    param(dest_modes_, "dest_modes", "destination modes (see start modes)");
    param(large_stations_, "large_stations", "use only large stations");
    param(message_type_, "message_type", "intermodal|routing");
    param(start_type_, "start_type",
          "query type:\n"
          "  pretrip = interval at station\n"
          "  ontrip_station = start time at station\n"
          "  ontrip_train = start in train\n"
          "  intermodal_pretrip = interval at coordinate\n"
          "  intermodal_ontrip = start time at station");
    param(dest_type_, "dest_type", "destination type: coordinate|station");
    param(routers_, "routers", "routing targets");
    param(search_dir_, "search_dir", "search direction forward/backward");
    param(extend_earlier_, "extend_earlier", "extend search interval earlier");
    param(extend_later_, "extend_later", "extend search interval later");
    param(min_connection_count_, "min_connection_count",
          "min. number of connections (otherwise interval will be extended)");
    param(interval_size_, "interval_size",
          "size of the search interval in minutes");
  }

  MsgContent get_message_type() const {
    using cista::hash;
    switch (hash(message_type_)) {
      case hash("routing"): return MsgContent_RoutingRequest;
      case hash("intermodal"): return MsgContent_IntermodalRoutingRequest;
    }
    throw std::runtime_error{"query type not "};
  }

  intermodal::IntermodalStart get_start_type() const {
    using cista::hash;
    switch (hash(start_type_)) {
      case hash("intermodal_pretrip"):
        return intermodal::IntermodalStart_IntermodalPretripStart;
      case hash("intermodal_ontrip"):
        return intermodal::IntermodalStart_IntermodalOntripStart;
      case hash("ontrip_train"):
        return intermodal::IntermodalStart_OntripTrainStart;
      case hash("ontrip_station"):
        return intermodal::IntermodalStart_OntripStationStart;
      case hash("pretrip"): return intermodal::IntermodalStart_PretripStart;
    }
    throw std::runtime_error{"start type not supported"};
  }

  intermodal::IntermodalDestination get_dest_type() const {
    using cista::hash;
    switch (hash(dest_type_)) {
      case hash("coordinate"):
        return intermodal::IntermodalDestination_InputPosition;
      case hash("station"):
        return intermodal::IntermodalDestination_InputStation;
    }
    throw std::runtime_error{"start type not supported"};
  }

  SearchDir get_search_dir() const {
    using cista::hash;
    switch (hash(search_dir_)) {
      case hash("forward"): return SearchDir_Forward;
      case hash("backward"): return SearchDir_Backward;
    }
    throw std::runtime_error{"search dir not supported"};
  }

  int query_count_{1000};
  std::string message_type_{"intermodal"};
  std::string out_{"q_TARGET.txt"};
  std::string bbox_;
  std::string poly_file_;
  std::string start_modes_;
  std::string dest_modes_;
  std::string start_type_{"intermodal_pretrip"};
  std::string dest_type_{"coordinate"};
  bool large_stations_{false};
  std::vector<std::string> routers_{"/routing"};
  std::string search_dir_{"forward"};
  bool extend_earlier_{false};
  bool extend_later_{false};
  unsigned min_connection_count_{0U};
  unsigned interval_size_{60U};  // [minutes]
};

std::string replace_target_escape(std::string const& str,
                                  std::string const& target) {
  auto const esc_pos = str.find(kTargetEscape);
  utl::verify(esc_pos != std::string::npos, "target escape {} not found in {}",
              kTargetEscape, str);

  auto clean_target = target;
  if (clean_target[0] == '/') {
    clean_target.erase(clean_target.begin());
  }
  std::replace(clean_target.begin(), clean_target.end(), '/', '_');

  auto target_str = str;
  target_str.replace(esc_pos, kTargetEscape.size(), clean_target);

  return target_str;
}

struct mode {
  CISTA_PRINTABLE(mode);

  int get_param(std::size_t const index, int const default_value) const {
    return parameters_.size() > index ? parameters_[index] : default_value;
  }

  std::string name_;
  std::vector<int> parameters_;
};

std::vector<mode> read_modes(std::string const& in) {
  std::regex word_regex("([_a-z]+)(-\\d+)?(-\\d+)?");
  std::smatch match;
  std::vector<mode> modes;
  utl::for_each_token(utl::cstr{in}, '|', [&](auto&& s) {
    auto const x = std::string{s.view()};
    auto const matches = std::regex_search(x, match, word_regex);
    if (!matches) {
      throw utl::fail("invalid mode in \"{}\": {}", in, s.view());
    }

    auto m = mode{};
    m.name_ = match[1].str();
    if (match.size() > 2) {
      for (auto i = 2; i != match.size(); ++i) {
        auto const& group = match[i];
        if (group.str().size() > 1) {
          m.parameters_.emplace_back(std::stoi(group.str().substr(1)));
        }
      }
    }
    modes.emplace_back(m);
  });
  return modes;
}

::nigiri::query_generation::transport_mode get_transport_mode(
    std::vector<mode> const& modes) {
  constexpr auto const max_walk_speed = 50U;  // [m/minute] --> 3 km/h
  constexpr auto const max_bike_speed = 200U;  // [m/minute] --> 12 km/h
  constexpr auto const max_car_speed = 800U;  // [m/minute] --> 48 km/h

  auto r = std::numeric_limits<double>::min();
  auto mode_id = 0U;
  ::nigiri::query_generation::transport_mode nigiri_mode{0, 0, 0};
  for (auto const& m : modes) {
    std::uint16_t cur_speed;
    std::uint16_t cur_duration;
    std::uint16_t cur_range;

    if (m.name_ == "ppr" || m.name_ == "osrm_foot") {
      cur_speed = max_walk_speed;
      cur_duration = m.get_param(0, 15);
      cur_range = cur_duration * max_walk_speed;
    } else if (m.name_ == "osrm_bike") {
      cur_speed = max_bike_speed;
      cur_duration = m.get_param(0, 15);
      cur_range = cur_duration * max_bike_speed;
    } else if (m.name_ == "osrm_car" || m.name_ == "osrm_car_parking") {
      cur_speed = max_car_speed;
      cur_duration = m.get_param(0, 15);
      cur_range = cur_duration * max_car_speed;
    } else if (m.name_ == "gbfs") {
      std::uint16_t const walk_duration = m.get_param(0, 15);
      std::uint16_t const bike_duration = m.get_param(1, 15);
      cur_duration = walk_duration + bike_duration;
      cur_speed =
          (max_walk_speed * walk_duration + max_bike_speed * bike_duration) /
          cur_duration;
      cur_range = cur_duration * cur_speed;
    } else {
      throw utl::fail("unknown mode \"{}\"", m.name_);
    }

    if (r < cur_range) {
      r = cur_range;
      nigiri_mode.mode_id_ = mode_id;
      nigiri_mode.speed_ = cur_speed;
      nigiri_mode.max_duration_ = cur_duration;
    }
    ++mode_id;
  }
  return nigiri_mode;
}

std::vector<flatbuffers::Offset<intermodal::ModeWrapper>> create_modes(
    flatbuffers::FlatBufferBuilder& fbb, std::vector<mode> const& modes) {
  auto v = std::vector<flatbuffers::Offset<intermodal::ModeWrapper>>{};
  for (auto const& m : modes) {
    if (m.name_ == "ppr") {
      v.emplace_back(CreateModeWrapper(
          fbb, intermodal::Mode_FootPPR,
          intermodal::CreateFootPPR(
              fbb, ppr::CreateSearchOptions(fbb, fbb.CreateString("default"),
                                            60 * m.get_param(0, 15)))
              .Union()));
    } else if (m.name_ == "osrm_foot") {
      v.emplace_back(CreateModeWrapper(
          fbb, intermodal::Mode_Foot,
          intermodal::CreateFoot(fbb, 60 * m.get_param(0, 15)).Union()));
    } else if (m.name_ == "osrm_bike") {
      v.emplace_back(CreateModeWrapper(
          fbb, intermodal::Mode_Bike,
          intermodal::CreateBike(fbb, 60 * m.get_param(0, 15)).Union()));
    } else if (m.name_ == "osrm_car") {
      v.emplace_back(CreateModeWrapper(
          fbb, intermodal::Mode_Car,
          intermodal::CreateCar(fbb, 60 * m.get_param(0, 15)).Union()));
    } else if (m.name_ == "osrm_car_parking") {
      v.emplace_back(CreateModeWrapper(
          fbb, intermodal::Mode_CarParking,
          intermodal::CreateCarParking(
              fbb, 60 * m.get_param(0, 15),
              ppr::CreateSearchOptions(fbb, fbb.CreateString("default"),
                                       60 * m.get_param(1, 10)))
              .Union()));
    } else if (m.name_ == "gbfs") {
      v.emplace_back(CreateModeWrapper(
          fbb, intermodal::Mode_GBFS,
          intermodal::CreateGBFS(fbb, fbb.CreateString("default"),
                                 60 * m.get_param(0, 15),
                                 60 * m.get_param(1, 15))
              .Union()));
    } else {
      throw utl::fail("unknown mode \"{}\"", m.name_);
    }
  }
  return v;
}

Position to_motis_pos(geo::latlng const& nigiri_pos) {
  return {nigiri_pos.lat(), nigiri_pos.lng()};
}

void write_query(::nigiri::query_generation::query_generator& qg,
                 std::uint32_t query_id, Interval const interval,
                 std::vector<mode> const& start_modes,
                 std::vector<mode> const& dest_modes,
                 MsgContent const message_type,
                 intermodal::IntermodalStart const start_type,
                 intermodal::IntermodalDestination const dest_type,
                 SearchDir dir, std::vector<std::string> const& routers,
                 std::vector<std::ofstream>& out_files) {
  auto fbbs = utl::to_vec(routers, [](auto&&) {
    return std::make_unique<module::message_creator>();
  });

  auto const get_destination = [&](flatbuffers::FlatBufferBuilder& fbb) {
    if (dest_type == intermodal::IntermodalDestination_InputPosition) {
      auto const dest_pos = qg.random_dest_pos();
      return intermodal::CreateInputPosition(fbb, dest_pos.lat(),
                                             dest_pos.lng())
          .Union();
    } else {
      return routing::CreateInputStation(fbb,
                                         fbb.CreateString(qg.random_stop_id()),
                                         fbb.CreateString(""))
          .Union();
    }
  };

  if (message_type == MsgContent_IntermodalRoutingRequest) {
    switch (start_type) {
      case intermodal::IntermodalStart_IntermodalPretripStart: {
        auto const start_pos = to_motis_pos(qg.random_start_pos());

        for (auto const& [fbbp, router] : utl::zip(fbbs, routers)) {
          auto& fbb = *fbbp;
          fbb.create_and_finish(
              MsgContent_IntermodalRoutingRequest,
              intermodal::CreateIntermodalRoutingRequest(
                  fbb, start_type,
                  intermodal::CreateIntermodalPretripStart(
                      fbb, &start_pos, &interval, qg.min_connection_count_,
                      qg.extend_interval_earlier_, qg.extend_interval_later_)
                      .Union(),
                  fbb.CreateVector(create_modes(fbb, start_modes)), dest_type,
                  get_destination(fbb),
                  fbb.CreateVector(create_modes(fbb, dest_modes)),
                  routing::SearchType_Default, dir, fbb.CreateString(router))
                  .Union(),
              "/intermodal", DestinationType_Module, query_id);
        }

        break;
      }

      case intermodal::IntermodalStart_IntermodalOntripStart: {
        auto const start_pos = to_motis_pos(qg.random_start_pos());

        for (auto const& [fbbp, router] : utl::zip(fbbs, routers)) {
          auto& fbb = *fbbp;
          fbb.create_and_finish(
              MsgContent_IntermodalRoutingRequest,
              CreateIntermodalRoutingRequest(
                  fbb, start_type,
                  intermodal::CreateIntermodalOntripStart(fbb, &start_pos,
                                                          interval.begin())
                      .Union(),
                  fbb.CreateVector(create_modes(fbb, start_modes)), dest_type,
                  get_destination(fbb),
                  fbb.CreateVector(create_modes(fbb, dest_modes)),
                  routing::SearchType_Default, dir, fbb.CreateString(router))
                  .Union(),
              "/intermodal", DestinationType_Module, query_id);
        }

        break;
      }

      case intermodal::IntermodalStart_OntripTrainStart: {
        // TODO
        break;
      }

      case intermodal::IntermodalStart_OntripStationStart: {
        for (auto const& [fbbp, router] : utl::zip(fbbs, routers)) {
          auto& fbb = *fbbp;
          fbb.create_and_finish(
              MsgContent_IntermodalRoutingRequest,
              intermodal::CreateIntermodalRoutingRequest(
                  fbb, start_type,
                  routing::CreateOntripStationStart(
                      fbb,
                      routing::CreateInputStation(
                          fbb, fbb.CreateString(qg.random_stop_id()),
                          fbb.CreateString("")),
                      interval.begin())
                      .Union(),
                  fbb.CreateVector(create_modes(fbb, start_modes)), dest_type,
                  get_destination(fbb),
                  fbb.CreateVector(create_modes(fbb, dest_modes)),
                  routing::SearchType_Default, dir, fbb.CreateString(router))
                  .Union(),
              "/intermodal", DestinationType_Module, query_id);
        }

        break;
      }

      case intermodal::IntermodalStart_PretripStart: {
        for (auto const& [fbbp, router] : utl::zip(fbbs, routers)) {
          auto& fbb = *fbbp;
          fbb.create_and_finish(
              MsgContent_IntermodalRoutingRequest,
              CreateIntermodalRoutingRequest(
                  fbb, start_type,
                  CreatePretripStart(
                      fbb,
                      routing::CreateInputStation(
                          fbb, fbb.CreateString(qg.random_stop_id()),
                          fbb.CreateString("")),
                      &interval, qg.min_connection_count_,
                      qg.extend_interval_earlier_, qg.extend_interval_later_)
                      .Union(),
                  fbb.CreateVector(create_modes(fbb, start_modes)), dest_type,
                  get_destination(fbb),
                  fbb.CreateVector(create_modes(fbb, dest_modes)),
                  routing::SearchType_Default, dir, fbb.CreateString(router))
                  .Union(),
              "/intermodal", DestinationType_Module, query_id);
        }

        break;
      }

      default:
        throw utl::fail(
            "start type {} not supported for message type intermodal",
            EnumNameIntermodalStart(start_type));
    }
  } else if (message_type == MsgContent_RoutingRequest) {

    switch (start_type) {

      case intermodal::IntermodalStart_OntripTrainStart: {
        // TODO
        break;
      }

      case intermodal::IntermodalStart_OntripStationStart: {
        for (auto const& [fbbp, router] : utl::zip(fbbs, routers)) {
          auto& fbb = *fbbp;
          fbb.create_and_finish(
              MsgContent_RoutingRequest,
              CreateRoutingRequest(
                  fbb, routing::Start_OntripStationStart,
                  CreateOntripStationStart(
                      fbb,
                      routing::CreateInputStation(
                          fbb, fbb.CreateString(qg.random_stop_id()),
                          fbb.CreateString("")),
                      interval.begin())
                      .Union(),
                  routing::CreateInputStation(
                      fbb, fbb.CreateString(qg.random_stop_id()),
                      fbb.CreateString("")),
                  routing::SearchType_Default, dir,
                  fbb.CreateVector(
                      std::vector<flatbuffers::Offset<routing::Via>>()),
                  fbb.CreateVector(std::vector<flatbuffers::Offset<
                                       routing::AdditionalEdgeWrapper>>()))
                  .Union(),
              router, DestinationType_Module, query_id);
        }
        break;
      }

      case intermodal::IntermodalStart_PretripStart: {
        for (auto const& [fbbp, router] : utl::zip(fbbs, routers)) {
          auto& fbb = *fbbp;
          fbb.create_and_finish(
              MsgContent_RoutingRequest,
              CreateRoutingRequest(
                  fbb, routing::Start_PretripStart,
                  CreatePretripStart(
                      fbb,
                      routing::CreateInputStation(
                          fbb, fbb.CreateString(qg.random_stop_id()),
                          fbb.CreateString("")),
                      &interval, qg.min_connection_count_,
                      qg.extend_interval_earlier_, qg.extend_interval_later_)
                      .Union(),
                  routing::CreateInputStation(
                      fbb, fbb.CreateString(qg.random_stop_id()),
                      fbb.CreateString("")),
                  routing::SearchType_Default, dir,
                  fbb.CreateVector(
                      std::vector<flatbuffers::Offset<routing::Via>>()),
                  fbb.CreateVector(std::vector<flatbuffers::Offset<
                                       routing::AdditionalEdgeWrapper>>()))
                  .Union(),
              router, DestinationType_Module, query_id);
        }
        break;
      }

      default:
        throw utl::fail("start type {} not supported for message type routing",
                        EnumNameIntermodalStart(start_type));
    }
  }

  for (auto const& [out_file, fbbp] : utl::zip(out_files, fbbs)) {
    auto& fbb = *fbbp;
    out_file << make_msg(fbb)->to_json(module::json_format::SINGLE_LINE)
             << "\n";
  }
}

int generate(int argc, char const** argv) {
  // need a motis instance to load nigiri module and timetable
  motis::bootstrap::motis_instance instance;
  motis::bootstrap::dataset_settings dataset_opt;
  motis::bootstrap::import_settings import_opt;
  motis::bootstrap::module_settings module_opt({"Next Generation Routing"});

  generator_settings generator_opt;

  std::vector<conf::configuration*> confs = {&import_opt, &dataset_opt,
                                             &module_opt, &generator_opt};
  for (auto const& module : instance.modules()) {
    if (module->name() == "Next Generation Routing") {
      confs.push_back(module);
    }
  }

  // parse commandline arguments
  try {
    conf::options_parser parser(confs);
    parser.read_environment("MOTIS_");
    parser.read_command_line_args(argc, argv, false);

    if (parser.help()) {
      std::cout << "\n\tnigiri-query-generator (MOTIS v"
                << motis::short_version() << ")\n\n";
      parser.print_help(std::cout);
      return 0;
    } else if (parser.version()) {
      std::cout << "nigiri-query-generator (MOTIS v" << motis::long_version()
                << ")\n";
      return 0;
    }

    parser.read_configuration_file(true);
    parser.print_used(std::cout);
    parser.print_unrecognized(std::cout);
  } catch (std::exception const& e) {
    std::cout << "options error: " << e.what() << "\n";
    return 1;
  }

  instance.import(module_opt, dataset_opt, import_opt);
  // instance.init_modules(module_opt);

  auto const start_modes = read_modes(generator_opt.start_modes_);
  auto const dest_modes = read_modes(generator_opt.dest_modes_);
  auto const start_type = generator_opt.get_start_type();
  auto const dest_type = generator_opt.get_dest_type();
  auto const message_type = generator_opt.get_message_type();

  utl::verify(generator_opt.dest_type_ == "coordinate" ||
                  generator_opt.dest_type_ == "station",
              "unknown destination type {}, supported: coordinate, station",
              generator_opt.dest_type_);
  utl::verify(
      !start_modes.empty() ||
          (start_type != intermodal::IntermodalStart_IntermodalOntripStart &&
           start_type != intermodal::IntermodalStart_IntermodalPretripStart),
      "no start modes given: {} (start_type={})", generator_opt.start_modes_,
      EnumNameIntermodalStart(start_type));
  utl::verify(!dest_modes.empty() ||
                  dest_type != intermodal::IntermodalDestination_InputPosition,
              "no destination modes given: {}, dest_type={}",
              generator_opt.dest_modes_,
              EnumNameIntermodalDestination(dest_type));

  auto of_streams =
      utl::to_vec(generator_opt.routers_, [&](std::string const& router) {
        return std::ofstream{replace_target_escape(generator_opt.out_, router)};
      });

  // get the nigiri timetable from shared data
  ::nigiri::timetable const& tt = *instance.get<::nigiri::timetable*>(
      to_res_id(motis::module::global_res_id::NIGIRI_TIMETABLE));

  // nigiri query generator setup
  ::nigiri::query_generation::query_generator qg{tt};
  qg.interval_size_ = ::nigiri::duration_t{generator_opt.interval_size_};
  qg.extend_interval_earlier_ = generator_opt.extend_earlier_;
  qg.extend_interval_later_ = generator_opt.extend_later_;
  qg.min_connection_count_ = generator_opt.min_connection_count_;
  qg.start_match_mode_ =
      start_type == intermodal::IntermodalStart_IntermodalPretripStart ||
              start_type == intermodal::IntermodalStart_IntermodalOntripStart
          ? ::nigiri::routing::location_match_mode::kIntermodal
          : ::nigiri::routing::location_match_mode::kEquivalent;
  qg.dest_match_mode_ =
      dest_type == intermodal::IntermodalDestination_InputPosition
          ? ::nigiri::routing::location_match_mode::kIntermodal
          : ::nigiri::routing::location_match_mode::kEquivalent;
  qg.start_mode_ = get_transport_mode(start_modes);
  qg.dest_mode_ = get_transport_mode(dest_modes);

  std::cout << "timetable spans " << tt.external_interval().from_ << " to "
            << tt.external_interval().to_ << "\n";

  for (std::uint32_t query_id = 1U; query_id <= generator_opt.query_count_;
       ++query_id) {
    if ((query_id % 100) == 0) {
      std::cout << query_id << "/" << generator_opt.query_count_ << "\n";
    }

    // unit of time designations of nigiri query time is [unixtime in minutes]
    unixtime const random_time =
        qg.random_time().time_since_epoch().count() * 60;  // [unixtime in s]
    Interval const random_interval{
        random_time, random_time + generator_opt.interval_size_ * 60};

    write_query(qg, query_id, random_interval, start_modes, dest_modes,
                message_type, start_type, dest_type,
                generator_opt.get_search_dir(), generator_opt.routers_,
                of_streams);
  }

  return 0;
}

}  // namespace motis::nigiri::eval
