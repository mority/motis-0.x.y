#include "motis/nigiri/query_generator.h"

namespace motis::nigiri {

motis::module::msg_ptr generate(tag_lookup const&, ::nigiri::timetable const&,
                                motis::module::msg_ptr const&) {
  std::cout << "query_generator.cc::generate called\n";
  return motis::module::make_success_msg();
}

}