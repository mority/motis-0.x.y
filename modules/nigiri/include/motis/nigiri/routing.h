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

motis::module::msg_ptr route(tag_lookup const&, ::nigiri::timetable const&,
                             ::nigiri::rt_timetable const*,
                             motis::module::msg_ptr const&,
                             ::nigiri::routing::tripbased::transfer_set const* ts = nullptr);

}  // namespace motis::nigiri