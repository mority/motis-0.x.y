#include "motis/nigiri/eval/commands.h"


#include "conf/configuration.h"
#include "conf/options_parser.h"

#include "motis/module/message.h"
#include "motis/bootstrap/dataset_settings.h"
#include "motis/bootstrap/motis_instance.h"

#include "version.h"

namespace motis::nigiri::eval {

struct generator_settings : public conf::configuration {
  generator_settings() : configuration("Generator Options", "") {
    param(query_count_, "query_count", "number of queries to generate");
    param(out_, "out", "file to write generated queries to");
    param(bbox_, "bbox", "bounding box for locations");
    param(poly_file_, "poly", "bounding polygon for locations");
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
};

int generate(int argc, char const** argv) {
  motis::bootstrap::dataset_settings dataset_opt;
  generator_settings generator_opt;
  motis::bootstrap::import_settings import_opt;

  // parse commandline arguments
  try {
    conf::options_parser parser({&dataset_opt, &generator_opt, &import_opt});
    parser.read_command_line_args(argc, argv, false);

    if (parser.help()) {
      std::cout << "\n\tnigiri-query-generator (MOTIS v" << motis::short_version()
                << ")\n\n";
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

  // need a motis instance to load nigiri module and timetable
  motis::bootstrap::motis_instance instance;
  instance.import(motis::bootstrap::module_settings{}, dataset_opt, import_opt);

  // pass commandline arguments to nigiri
  // construct a query generation message and send it to nigiri
  motis::module::message_creator fbb;

  instance.call("/nigiri_query_generator")

  return 0;
}

} // namespace motis::nigiri::eval

