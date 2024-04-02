#pragma once

#include <memory>
#include <string>

#include "motis/nigiri/eval/poly.h"

namespace motis::nigiri::eval {

std::unique_ptr<poly> parse_poly(std::string const& filename);

}  // namespace motis::nigiri::eval
