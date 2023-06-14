#pragma once

#include <string>
#include <vector>

#include "motis/module/message.h"

namespace nigiri {
struct timetable;
}

namespace nigiri::routing::tripbased {
struct transfer_set;
}

namespace motis::nigiri {

motis::module::msg_ptr route(
    std::vector<std::string> const& tags, ::nigiri::timetable& tt,
    motis::module::msg_ptr const& msg,
    ::nigiri::routing::tripbased::transfer_set const* ts = nullptr);

}  // namespace motis::nigiri