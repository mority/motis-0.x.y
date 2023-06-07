#pragma once

#include <memory>
#include <string>
#include <vector>

#include "motis/module/module.h"

namespace motis::nigiri {

enum class algorithm{
  raptor,
  tripbased
};

struct nigiri : public motis::module::module {
  nigiri();
  ~nigiri() override;

  nigiri(nigiri const&) = delete;
  nigiri& operator=(nigiri const&) = delete;

  nigiri(nigiri&&) = delete;
  nigiri& operator=(nigiri&&) = delete;

  void init(motis::module::registry&) override;
  void import(motis::module::import_dispatcher&) override;
  bool import_successful() const override { return import_successful_; }

private:
  bool import_successful_{false};

  struct impl;
  std::unique_ptr<impl> impl_;
  bool no_cache_{false};
  std::string first_day_;
  std::uint16_t num_days_{2U};
  bool geo_lookup_{false};
  unsigned link_stop_distance_{100U};
  std::string algo_str_;
  algorithm algo_{algorithm::raptor};
  std::string tbp_file_;
};

}  // namespace motis::nigiri
