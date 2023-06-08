#include "motis/nigiri/nigiri.h"

#include "cista/memory_holder.h"

#include "conf/date_time.h"

#include "utl/enumerate.h"
#include "utl/helpers/algorithm.h"
#include "utl/verify.h"

#include "geo/point_rtree.h"

#include "nigiri/loader/dir.h"
#include "nigiri/loader/gtfs/loader.h"
#include "nigiri/loader/hrd/loader.h"
#include "nigiri/loader/init_finish.h"
#include "nigiri/print_transport.h"
#include "nigiri/timetable.h"

#include "motis/core/common/logging.h"
#include "motis/module/event_collector.h"
#include "motis/nigiri/geo_station_lookup.h"
#include "motis/nigiri/routing.h"
#include "nigiri/routing/tripbased/tb_preprocessing.h"

namespace fs = std::filesystem;
namespace mm = motis::module;
namespace n = ::nigiri;

namespace motis::nigiri {

struct nigiri::impl {
  impl() {
    loaders_.emplace_back(std::make_unique<n::loader::gtfs::gtfs_loader>());
    loaders_.emplace_back(
        std::make_unique<n::loader::hrd::hrd_5_00_8_loader>());
    loaders_.emplace_back(
        std::make_unique<n::loader::hrd::hrd_5_20_26_loader>());
    loaders_.emplace_back(
        std::make_unique<n::loader::hrd::hrd_5_20_39_loader>());
    loaders_.emplace_back(
        std::make_unique<n::loader::hrd::hrd_5_20_avv_loader>());
  }
  std::vector<std::unique_ptr<n::loader::loader_interface>> loaders_;
  std::shared_ptr<cista::wrapped<n::timetable>> tt_;
  std::vector<std::string> tags_;
  geo::point_rtree station_geo_index_;
  std::shared_ptr<cista::wrapped<n::routing::tripbased::tb_preprocessing>> tbp_;
};

nigiri::nigiri() : module("Next Generation Routing", "nigiri") {
  param(no_cache_, "no_cache", "disable timetable caching");
  param(first_day_, "first_day",
        "YYYY-MM-DD, leave empty to use first day in source data");
  param(num_days_, "num_days", "number of days, ignored if first_day is empty");
  param(geo_lookup_, "geo_lookup", "provide geo station lookup");
  param(link_stop_distance_, "link_stop_distance",
        "GTFS only: radius to connect stations, 0=skip");
  param(algo_str_, "algorithm", "the routing algorithm used (default: raptor), possible options: raptor | tripbased");
}

nigiri::~nigiri() = default;

void nigiri::init(motis::module::registry& reg) {
  reg.register_op("/nigiri",
                  [&](mm::msg_ptr const& msg) {
                    return route(impl_->tags_, **impl_->tt_, msg);
                  },
                  {});

  if (geo_lookup_) {
    reg.register_op("/lookup/geo_station",
                    [&](mm::msg_ptr const& msg) {
                      return geo_station_lookup(impl_->tags_, **impl_->tt_,
                                                impl_->station_geo_index_, msg);
                    },
                    {});
  }
}

void nigiri::import(motis::module::import_dispatcher& reg) {
  // set the routing algorithm to user specified value
  if(algo_str_ == "raptor") {
    algo_ = algorithm::raptor;
    LOG(logging::info) << "nigiri routing algorithm: " << algo_str_ << "\n";
  } else if(algo_str_ == "tripbased") {
    algo_ = algorithm::tripbased;
    LOG(logging::info) << "nigiri routing algorithm: " << algo_str_ << "\n";
  } else {
    LOG(logging::warn) << "unknown routing algorithm option: " << algo_str_ << ", defaulting to raptor\n";
  }

  impl_ = std::make_unique<impl>();
  std::make_shared<mm::event_collector>(
      get_data_directory().generic_string(), "nigiri", reg,
      [this](mm::event_collector::dependencies_map_t const& dependencies,
             mm::event_collector::publish_fn_t const& publish) {
        using import::FileEvent;

        auto const& msg = dependencies.at("SCHEDULE");

        utl::verify(
            utl::all_of(*motis_content(FileEvent, msg)->paths(),
                        [](auto&& p) {
                          return p->tag()->str() != "schedule" ||
                                 !p->options()->str().empty();
                        }),
            "all schedules require a name tag, even with only one schedule");

        std::stringstream ss;
        ss << first_day_;
        date::sys_days begin;
        ss >> date::parse("%F", begin);

        auto const interval = n::interval<date::sys_days>{
            begin, begin + std::chrono::days{num_days_}};
        LOG(logging::info) << "interval: " << interval.from_ << " - "
                           << interval.to_;

        auto h =
            cista::hash_combine(cista::BASE_HASH,
                                interval.from_.time_since_epoch().count(),  //
                                interval.to_.time_since_epoch().count(),  //
                                link_stop_distance_);

        auto datasets =
            std::vector<std::tuple<n::source_idx_t,
                                   decltype(impl_->loaders_)::const_iterator,
                                   std::unique_ptr<n::loader::dir>>>{};
        for (auto const [i, p] :
             utl::enumerate(*motis_content(FileEvent, msg)->paths())) {
          if (p->tag()->str() != "schedule") {
            continue;
          }
          auto const path = fs::path{p->path()->str()};
          auto d = n::loader::make_dir(path);
          auto const c = utl::find_if(
              impl_->loaders_, [&](auto&& c) { return c->applicable(*d); });
          utl::verify(c != end(impl_->loaders_), "no loader applicable to {}",
                      path);
          h = cista::hash_combine(h, (*c)->hash(*d));

          datasets.emplace_back(n::source_idx_t{i}, c, std::move(d));

          auto const tag = p->options()->str();
          impl_->tags_.emplace_back(tag + (tag.empty() ? "default_" : "_"));
        }
        utl::verify(!datasets.empty(), "no schedule datasets found");

        auto const data_dir = get_data_directory() / "nigiri";
        auto const dump_file_path = data_dir / fmt::to_string(h);
        auto const tbp_file_path = data_dir / fmt::to_string(h) += ".tbp";

        auto loaded = false;
        for (auto i = 0U; i != 2; ++i) {
          // Parse from input files and write memory image.
          if (no_cache_ || !fs::is_regular_file(dump_file_path) || (algo_ == algorithm::tripbased && !fs::is_regular_file(tbp_file_path))) {
            impl_->tt_ = std::make_shared<cista::wrapped<n::timetable>>(
                cista::raw::make_unique<n::timetable>());

            (*impl_->tt_)->date_range_ = interval;
            n::loader::register_special_stations(**impl_->tt_);

            for (auto const& [src, loader, dir] : datasets) {
              auto progress_tracker = utl::activate_progress_tracker(
                  fmt::format("{}nigiri", impl_->tags_[to_idx(src)]));

              LOG(logging::info)
                  << "loading nigiri timetable with configuration "
                  << (*loader)->name();

              try {
                (*loader)->load({.link_stop_distance_ = link_stop_distance_},
                                src, *dir, **impl_->tt_);
                progress_tracker->status("FINISHED").show_progress(false);
              } catch (std::exception const& e) {
                progress_tracker->status(fmt::format("ERROR: {}", e.what()))
                    .show_progress(false);
                throw;
              } catch (...) {
                progress_tracker->status("ERROR: UNKNOWN EXCEPTION")
                    .show_progress(false);
                throw;
              }
            }

            n::loader::finalize(**impl_->tt_);

            if(algo_ == algorithm::tripbased) {
              impl_->tbp_ = std::make_shared<cista::wrapped<n::routing::tripbased::tb_preprocessing>>(cista::raw::make_unique<n::routing::tripbased::tb_preprocessing>(**impl_->tt_));
              LOG(logging::info) << "nigiri trip-based preprocessing: building transfer set" << "\n";
              auto progress_tracker = utl::activate_progress_tracker("trip-based preprocessing");
              progress_tracker->show_progress(true);
              (*impl_->tbp_)->build_transfer_set();
              progress_tracker->status("FINISHED").show_progress(false);
              utl::verify((*impl_->tbp_)->ts_ready_, "nigiri trip-based preprocessing failed");
              LOG(logging::info) << "nigiri trip-based preprocessing: found " << (*impl_->tbp_)->n_transfers_ << " transfers" << "\n";
            }

            if (no_cache_) {
              loaded = true;
              break;
            } else {
              // Write to disk, next step: read from disk.
              std::filesystem::create_directories(data_dir);
              (*impl_->tt_)->write(dump_file_path);
              if(algo_ == algorithm::tripbased) {
                (*impl_->tbp_)->write(tbp_file_path);
              }
            }
          }

          // Read memory image from disk.
          if (!no_cache_) {
            try {
              impl_->tt_ = std::make_shared<cista::wrapped<n::timetable>>(
                  n::timetable::read(cista::memory_holder{
                      cista::file{dump_file_path.string().c_str(), "r"}
                          .content()}));
              (**impl_->tt_).locations_.resolve_timezones();
              if(algo_ == algorithm::tripbased) {
                impl_->tbp_ = std::make_shared<cista::wrapped<n::routing::tripbased::tb_preprocessing>>(
                    n::routing::tripbased::tb_preprocessing::read(cista::memory_holder{
                    cista::file{tbp_file_path.string().c_str(), "r"}.content()}));
              }
              loaded = true;
              break;
            } catch (std::exception const& e) {
              LOG(logging::error)
                  << "cannot read cached timetable image: " << e.what();
              std::filesystem::remove(dump_file_path);
              if(algo_ == algorithm::tripbased) {
                std::filesystem::remove(tbp_file_path);
              }
              continue;
            }
          }
        }

        utl::verify(loaded, "loading failed");

        add_shared_data(to_res_id(mm::global_res_id::NIGIRI_TIMETABLE),
                        impl_->tt_->get());
        add_shared_data(to_res_id(mm::global_res_id::NIGIRI_TAGS),
                        &impl_->tags_);

        LOG(logging::info) << "nigiri timetable: stations="
                           << (*impl_->tt_)->locations_.names_.size()
                           << ", trips=" << (*impl_->tt_)->trip_debug_.size()
                           << "\n";

        if(algo_ == algorithm::tripbased) {
          LOG(logging::info) << "nigiri tripbased preprocessing: transfers=" << (*impl_->tbp_)->n_transfers_ << "\n";
        }

        if (geo_lookup_) {
          impl_->station_geo_index_ =
              geo::make_point_rtree((**impl_->tt_).locations_.coordinates_);
        }

        import_successful_ = true;

        mm::message_creator fbb;
        fbb.create_and_finish(MsgContent_NigiriEvent,
                              motis::import::CreateNigiriEvent(fbb).Union(),
                              "/import", DestinationType_Topic);
        publish(make_msg(fbb));
      })
      ->require("SCHEDULE", [this](mm::msg_ptr const& msg) {
        if (msg->get()->content_type() != MsgContent_FileEvent) {
          return false;
        }
        using import::FileEvent;
        return motis_content(FileEvent, msg)->paths()->size() != 0U &&
               utl::all_of(*motis_content(FileEvent, msg)->paths(),
                           [this](import::ImportPath const* p) {
                             if (p->tag()->str() != "schedule") {
                               return true;
                             }
                             auto const d = n::loader::make_dir(
                                 fs::path{p->path()->str()});
                             return utl::any_of(impl_->loaders_, [&](auto&& c) {
                               return c->applicable(*d);
                             });
                           });
      });
}

}  // namespace motis::nigiri
