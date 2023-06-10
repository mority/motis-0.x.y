#pragma once

#include <string>
#include <vector>

#include "motis/module/message.h"

namespace nigiri {
struct timetable;
}

namespace nigiri::routing::tripbased {
struct tb_preprocessing;
}

namespace motis::nigiri {

motis::module::msg_ptr route(
    std::vector<std::string> const& tags, ::nigiri::timetable& tt,
    ::nigiri::routing::tripbased::tb_preprocessing* tbp,
    motis::module::msg_ptr const& msg);

}  // namespace motis::nigiri