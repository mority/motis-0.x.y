#pragma once

#include "motis/module/message.h"

namespace nigiri {
struct timetable;
struct rt_timetable;
}  // namespace nigiri

namespace nigiri::routing::tripbased {
struct transfer_set;
}

namespace motis::nigiri {

struct tag_lookup;

motis::module::msg_ptr route(tag_lookup const&, ::nigiri::timetable&,
                             ::nigiri::rt_timetable const*,
                             motis::module::msg_ptr const&);
motis::module::msg_ptr route(
    std::vector<std::string> const& tags, ::nigiri::timetable& tt,
    motis::module::msg_ptr const& msg,
    ::nigiri::routing::tripbased::transfer_set const* ts = nullptr);

}  // namespace motis::nigiri