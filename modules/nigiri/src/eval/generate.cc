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
#include "motis/nigiri/extern_trip.h"
#include "motis/nigiri/location.h"
#include "motis/nigiri/tag_lookup.h"
#include "motis/nigiri/unixtime_conv.h"

#include "nigiri/query_generator/generator.h"
#include "nigiri/timetable.h"

#include "version.h"

namespace motis::nigiri::eval {

constexpr auto const kTargetEscape = std::string_view{"TARGET"};

struct generator_settings : public conf::configuration {
  generator_settings() : configuration("Generator Options", "") {
    param(query_count_, "query_count", "number of queries to generate");
    param(out_, "out", "file to write generated queries to");
    param(bbox_, "bbox",
          "limit randomized locations to a bounding box, format: "
          "lat_min,lon_min,lat_max,lon_max\ne.g., "
          "36.0,-11.0,72.0,32.0\n(available via \"-b europe\")");
    param(start_modes_, "start_modes", "start modes ppr-15|osrm_car-15|...");
    param(dest_modes_, "dest_modes", "destination modes (see start modes)");
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
  std::string start_modes_;
  std::string dest_modes_;
  std::string start_type_{"intermodal_pretrip"};
  std::string dest_type_{"coordinate"};
  std::vector<std::string> routers_{"/nigiri"};
  std::string search_dir_{"forward"};
  bool extend_earlier_{false};
  bool extend_later_{false};
  unsigned min_connection_count_{0U};
  unsigned interval_size_{120U};  // [minutes]
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

std::vector<std::string> tokenize(std::string const& str, char delimiter,
                                  std::uint32_t n_tokens) {
  auto tokens = std::vector<std::string>{};
  tokens.reserve(n_tokens);
  auto start = 0U;
  for (auto i = 0U; i != n_tokens; ++i) {
    auto end = str.find(delimiter, start);
    if (end == std::string::npos && i != n_tokens - 1U) {
      break;
    }
    tokens.emplace_back(str.substr(start, end - start));
    start = end + 1U;
  }
  return tokens;
}

std::optional<geo::box> parse_bbox(std::string const& str) {
  using namespace geo;
  if (str == "europe") {
    return box{latlng{36.0, -11.0}, latlng{72.0, 32.0}};
  }

  auto const bbox_regex = std::regex{
      "^[-+]?[0-9]*\\.?[0-9]+,[-+]?[0-9]*\\.?[0-9]+,[-+]?[0-9]*\\.?[0-9]+,[-+]?"
      "[0-9]*\\.?[0-9]+$"};
  if (!std::regex_match(begin(str), end(str), bbox_regex)) {
    return std::nullopt;
  }

  auto const tokens = tokenize(str, ',', 4U);
  return box{latlng{std::stod(tokens[0]), std::stod(tokens[1])},
             latlng{std::stod(tokens[2]), std::stod(tokens[3])}};
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
  auto mode_id = 0;
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

void write_query(::nigiri::query_generation::generator& qg,
                 nigiri::tag_lookup const& tags, std::int32_t query_id,
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

  std::optional<::nigiri::routing::query> random_query;
  while (!random_query.has_value()) {
    random_query = qg.random_pretrip_query();
  }

  auto const nigiri_start_interval = std::visit(
      utl::overloaded{
          [](::nigiri::unixtime_t const& start_time) {
            return ::nigiri::interval<::nigiri::unixtime_t>{start_time,
                                                            start_time};
          },
          [](::nigiri::interval<::nigiri::unixtime_t> const& start_interval) {
            return start_interval;
          }},
      random_query.value().start_time_);

  Interval const start_interval{to_motis_unixtime(nigiri_start_interval.from_),
                                to_motis_unixtime(nigiri_start_interval.to_)};

  // randomize start and destination location indices
  auto const start_location_idx = random_query.value().start_[0].target();
  auto const dest_location_idx = random_query.value().destination_[0].target();

  // resolve start and destination indices
  auto const start_location_id =
      get_station_id(tags, qg.tt_, start_location_idx);
  auto const start_pos = to_motis_pos(qg.pos_near_start(start_location_idx));
  auto const dest_location_id = get_station_id(tags, qg.tt_, dest_location_idx);
  auto const dest_pos = to_motis_pos(qg.pos_near_dest(dest_location_idx));

  auto const get_destination = [&](flatbuffers::FlatBufferBuilder& fbb) {
    if (dest_type == intermodal::IntermodalDestination_InputPosition) {
      return intermodal::CreateInputPosition(fbb, dest_pos.lat(),
                                             dest_pos.lng())
          .Union();
    } else {
      return routing::CreateInputStation(
                 fbb, fbb.CreateString(dest_location_id), fbb.CreateString(""))
          .Union();
    }
  };

  auto const ontrip_train = [&]() {
    auto const [tr, stop_idx] = qg.random_transport_active_stop();
    auto const merged_trips_idx =
        qg.tt_.transport_to_trip_section_[tr.t_idx_].size() == 1
            ? qg.tt_.transport_to_trip_section_[tr.t_idx_]
                                               [0]  // all sections belong
                                                    // to the same trip
            : qg.tt_.transport_to_trip_section_[tr.t_idx_][stop_idx];
    auto const trip_idx =
        qg.tt_
            .merged_trips_[merged_trips_idx][0];  // 0 until more than one trip
                                                  // is merged in the transport
    auto const trip_stop =
        ::nigiri::stop{
            qg.tt_.route_location_seq_[qg.tt_.transport_route_[tr.t_idx_]]
                                      [stop_idx]}
            .location_idx();
    auto const unixtime_arr_stop = to_motis_unixtime(
        qg.tt_.event_time(tr, stop_idx, ::nigiri::event_type::kArr));
    auto const extern_trip =
        nigiri_trip_to_extern_trip(tags, qg.tt_, trip_idx, tr);
    auto const trip_stop_id = get_station_id(tags, qg.tt_, trip_stop);

    return std::tuple{extern_trip, trip_stop_id, unixtime_arr_stop};
  };

  if (message_type == MsgContent_IntermodalRoutingRequest) {
    switch (start_type) {
      case intermodal::IntermodalStart_IntermodalPretripStart: {
        for (auto const& [fbbp, router] : utl::zip(fbbs, routers)) {
          auto& fbb = *fbbp;
          fbb.create_and_finish(
              MsgContent_IntermodalRoutingRequest,
              intermodal::CreateIntermodalRoutingRequest(
                  fbb, start_type,
                  intermodal::CreateIntermodalPretripStart(
                      fbb, &start_pos, &start_interval,
                      qg.s_.min_connection_count_,
                      qg.s_.extend_interval_earlier_,
                      qg.s_.extend_interval_later_)
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
        for (auto const& [fbbp, router] : utl::zip(fbbs, routers)) {
          auto& fbb = *fbbp;
          fbb.create_and_finish(
              MsgContent_IntermodalRoutingRequest,
              CreateIntermodalRoutingRequest(
                  fbb, start_type,
                  intermodal::CreateIntermodalOntripStart(
                      fbb, &start_pos, start_interval.begin())
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
        auto const [extern_trip, trip_stop_id, unixtime_arr_stop] =
            ontrip_train();

        for (auto const& [fbbp, router] : utl::zip(fbbs, routers)) {
          auto& fbb = *fbbp;
          fbb.create_and_finish(
              MsgContent_IntermodalRoutingRequest,
              CreateIntermodalRoutingRequest(
                  fbb, start_type,
                  CreateOntripTrainStart(
                      fbb,
                      CreateTripId(
                          fbb, fbb.CreateString(extern_trip.id_),
                          fbb.CreateString(extern_trip.station_id_),
                          extern_trip.train_nr_, extern_trip.time_,
                          fbb.CreateString(extern_trip.target_station_id_),
                          extern_trip.target_time_,
                          fbb.CreateString(extern_trip.line_id_)),
                      routing::CreateInputStation(
                          fbb, fbb.CreateString(trip_stop_id),
                          fbb.CreateString("")),
                      unixtime_arr_stop)
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
                          fbb, fbb.CreateString(start_location_id),
                          fbb.CreateString("")),
                      start_interval.begin())
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
                          fbb, fbb.CreateString(start_location_id),
                          fbb.CreateString("")),
                      &start_interval, qg.s_.min_connection_count_,
                      qg.s_.extend_interval_earlier_,
                      qg.s_.extend_interval_later_)
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
        auto const [extern_trip, trip_stop_id, unixtime_arr_stop] =
            ontrip_train();

        for (auto const& [fbbp, router] : utl::zip(fbbs, routers)) {
          auto& fbb = *fbbp;
          fbb.create_and_finish(
              MsgContent_RoutingRequest,
              motis::routing::CreateRoutingRequest(
                  fbb, routing::Start_OntripTrainStart,
                  routing::CreateOntripTrainStart(
                      fbb,
                      CreateTripId(
                          fbb, fbb.CreateString(extern_trip.id_),
                          fbb.CreateString(extern_trip.station_id_),
                          extern_trip.train_nr_, extern_trip.time_,
                          fbb.CreateString(extern_trip.target_station_id_),
                          extern_trip.target_time_,
                          fbb.CreateString(extern_trip.line_id_)),
                      routing::CreateInputStation(
                          fbb, fbb.CreateString(trip_stop_id),
                          fbb.CreateString("")),
                      unixtime_arr_stop)
                      .Union(),
                  routing::CreateInputStation(
                      fbb, fbb.CreateString(dest_location_id),
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
                          fbb, fbb.CreateString(start_location_id),
                          fbb.CreateString("")),
                      start_interval.begin())
                      .Union(),
                  routing::CreateInputStation(
                      fbb, fbb.CreateString(dest_location_id),
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
                          fbb, fbb.CreateString(start_location_id),
                          fbb.CreateString("")),
                      &start_interval, qg.s_.min_connection_count_,
                      qg.s_.extend_interval_earlier_,
                      qg.s_.extend_interval_later_)
                      .Union(),
                  routing::CreateInputStation(
                      fbb, fbb.CreateString(dest_location_id),
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
  ::nigiri::query_generation::generator_settings gs;
  gs.interval_size_ = ::nigiri::duration_t{generator_opt.interval_size_};
  gs.extend_interval_earlier_ = generator_opt.extend_earlier_;
  gs.extend_interval_later_ = generator_opt.extend_later_;
  gs.min_connection_count_ = generator_opt.min_connection_count_;
  gs.start_match_mode_ = ::nigiri::routing::location_match_mode::kEquivalent;
  gs.dest_match_mode_ = ::nigiri::routing::location_match_mode::kEquivalent;
  gs.start_mode_ = get_transport_mode(start_modes);
  gs.dest_mode_ = get_transport_mode(dest_modes);
  if (!generator_opt.bbox_.empty()) {
    gs.bbox_ = parse_bbox(generator_opt.bbox_);
    if (!gs.bbox_.has_value()) {
      std::cout << "Error: bbox input malformed\n";
      return 1;
    }
  }
  ::nigiri::query_generation::generator qg{tt, gs};

  // get nigiri tags from shared data
  nigiri::tag_lookup const& tags = *instance.get<nigiri::tag_lookup*>(
      to_res_id(motis::module::global_res_id::NIGIRI_TAGS));

  for (std::int32_t query_id = 1; query_id <= generator_opt.query_count_;
       ++query_id) {
    if ((query_id % 100) == 0) {
      std::cout << query_id << "/" << generator_opt.query_count_ << "\n";
    }
    write_query(qg, tags, query_id, start_modes, dest_modes, message_type,
                start_type, dest_type, generator_opt.get_search_dir(),
                generator_opt.routers_, of_streams);
  }

  return 0;
}

}  // namespace motis::nigiri::eval
