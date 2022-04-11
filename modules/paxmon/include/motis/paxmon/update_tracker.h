#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "motis/core/schedule/schedule.h"
#include "motis/module/message.h"

#include "motis/paxmon/passenger_group.h"
#include "motis/paxmon/statistics.h"

namespace motis::paxmon {

struct universe;

struct update_tracker {
  update_tracker();
  update_tracker(update_tracker const&);
  update_tracker(update_tracker&&) noexcept;
  ~update_tracker();

  update_tracker& operator=(update_tracker const&);
  update_tracker& operator=(update_tracker&&) noexcept;

  void start_tracking(universe const&, schedule const&,
                      bool include_before_trip_load_info,
                      bool include_after_trip_load_info,
                      bool include_trips_with_unchanged_load);
  std::pair<motis::module::message_creator&,
            flatbuffers::Offset<PaxMonTrackedUpdates>>
  finish_updates();
  void stop_tracking();
  bool is_tracking() const;
  std::vector<tick_statistics> get_tick_statistics() const;

  void before_group_added(passenger_group const*);
  void before_group_reused(passenger_group const*);
  void after_group_reused(passenger_group const*);
  void before_group_removed(passenger_group const*);
  void before_trip_rerouted(trip const*);
  void rt_updates_applied(tick_statistics const& tick_stats);

private:
  struct impl;
  std::unique_ptr<impl> impl_;
};

}  // namespace motis::paxmon
