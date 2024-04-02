#pragma once

#include <memory>
#include <string>

#include "motis/nigiri/eval/bbox.h"

namespace motis::nigiri::eval {

std::unique_ptr<bbox> parse_bbox(std::string const& input);

}  // namespace motis::nigiri::eval
