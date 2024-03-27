#include "motis/nigiri/eval/commands.h"

#include <regex>

#include "utl/parser/cstr.h"

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

int generate(int argc, char const** argv) {
  generator_settings generator_opt;

  motis::bootstrap::module_settings module_opt;
  motis::bootstrap::dataset_settings dataset_opt;
  motis::bootstrap::import_settings import_opt;

  // parse commandline arguments
  try {
    conf::options_parser parser(
        {&generator_opt, &module_opt, &dataset_opt, &import_opt});
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

  auto const start_modes = read_modes(generator_opt.start_modes_);
  auto const dest_modes = read_modes(generator_opt.dest_modes_);
  auto const start_type = generator_opt.get_start_type();
  auto const dest_type = generator_opt.get_dest_type();
  auto const message_type = generator_opt.get_message_type();

  // need a motis instance to load nigiri module and timetable
  motis::bootstrap::motis_instance instance;
  instance.import(module_opt, dataset_opt, import_opt);

  // get the nigiri timetable from shared data
  ::nigiri::timetable const& tt = *instance.get<::nigiri::timetable*>(
      to_res_id(motis::module::global_res_id::NIGIRI_TIMETABLE));

  // instantiate nigiri query generator
  ::nigiri::query_generation::query_generator qg{tt};
  qg.interval_size_ = ::nigiri::duration_t{generator_opt.interval_size_};

  for (std::uint32_t q_id = 1U; q_id <= generator_opt.query_count_; ++q_id) {
    if ((q_id % 100) == 0) {
      std::cout << q_id << "/" << generator_opt.query_count_ << "\n";
    }

    // unit of time designations of nigiri query time is [unixtime in minutes]
    unixtime const random_time =
        qg.random_time().time_since_epoch().count() * 60;  // [unixtime in s]
    Interval random_interval{random_time,
                             random_time + generator_opt.interval_size_ * 60};
  }

  return 0;
}

}  // namespace motis::nigiri::eval
