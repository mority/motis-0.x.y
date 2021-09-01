#pragma once

#include "motis/core/schedule/event.h"

namespace motis::railviz {

struct train {
  CISTA_COMPARABLE();

  ev_key key_;
  float route_distance_{0};
};

}  // namespace motis::railviz
