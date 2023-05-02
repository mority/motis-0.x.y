#pragma once

#include "motis/module/message.h"
#include "geo/point_rtree.h"

namespace nigiri {
struct timetable;
}

namespace motis::nigiri {

motis::module::msg_ptr geo_station_lookup(std::vector<std::string> const& tags,
                                          ::nigiri::timetable const&,
                                          geo::point_rtree const&,
                                          motis::module::msg_ptr const& msg);

}  // namespace motis::nigiri