#include <vcpkg-cache-server/maintenance.hpp>
#include <vcpkg-cache-server/logging.hpp>

namespace vcache {

namespace detail {

size_t removeCache(const db::Cache& cache, db::Database& db, std::vector<std::string>& toDelete,
                   std::shared_ptr<spdlog::logger> logger) {
    using namespace sqlite_orm;

    log::info(*logger,
              "[Maintain]    Removing cache: {} used: {:%Y-%m-%d %H:%M} "
              "created: {:%Y-%m-%d %H:%M} size: {:>10}",
              cache.sha, Time{Duration{cache.lastUsed}}, Time{Duration{cache.created}},
              ByteSize{cache.size});

    db.update_all(set(c(&db::Cache::deleted) = true), where(c(&db::Cache::id) == cache.id));

    toDelete.push_back(cache.sha);

    return cache.size;
}

// Effective last activity of a cache: the latest of lastUsed and created. The lastUsed
// sentinel -1 must be handled explicitly since real file_clock timestamps can be negative
// (on libstdc++ the epoch is in the future), making -1 "newer" than any real timestamp.
inline auto effectiveLastUse() {
    using namespace sqlite_orm;
    return case_<db::Rep>()
        .when(c(&db::Cache::lastUsed) == -1, then(&db::Cache::created))
        .else_(max(&db::Cache::lastUsed, &db::Cache::created))
        .end();
}

}  // namespace detail

void maintain(Store& store, db::Database& db, const Maintenance& maintenance,
              std::shared_ptr<spdlog::logger> logger, Time now) {

    using detail::removeCache;

    std::vector<std::string> toDelete;

    db.begin_transaction();

    size_t allRemoved{};

    log::info(*logger, "[Maintain] Running Maintenance");

    {
        using namespace sqlite_orm;
        log::info(*logger, "[Maintain] Running sanity checks");
        for (auto& cache : db.iterate<db::Cache>(where(c(&db::Cache::deleted) == true))) {
            if (store.exists(cache.sha)) {
                allRemoved += removeCache(cache, db, toDelete, logger);
            }
        }
        for (auto& cache : db.iterate<db::Cache>(
                 where(and_(c(&db::Cache::deleted) == false, c(&db::Cache::size) == 0)))) {

            allRemoved += removeCache(cache, db, toDelete, logger);
        }
        for (auto& cache : db.iterate<db::Cache>(where(c(&db::Cache::deleted) == false))) {
            if (!store.exists(cache.sha)) {
                allRemoved += removeCache(cache, db, toDelete, logger);
            }
        }
    }

    allRemoved +=
        maintenance.maxAge
            .and_then([&](Duration maxAge) -> std::optional<size_t> {
                using namespace sqlite_orm;
                const auto cutoff = now - maxAge;

                log::info(*logger, "[Maintain] Looking for packages created before: {} ({})",
                          cutoff, FormatDuration{maxAge});
                size_t removed{};
                for (auto& cache : db.iterate<db::Cache>(
                         where(and_(c(&db::Cache::deleted) == false,
                                    c(&db::Cache::created) < cutoff.time_since_epoch().count())))) {

                    removed += removeCache(cache, db, toDelete, logger);
                }
                return removed;
            })
            .value_or(size_t{0});

    allRemoved +=
        maintenance.maxUnused
            .and_then([&](Duration maxUnused) -> std::optional<size_t> {
                using namespace sqlite_orm;
                const auto cutoff = now - maxUnused;

                log::info(*logger, "[Maintain] Looking for packages not used after: {} ({})",
                          cutoff, FormatDuration{maxUnused});
                size_t removed{};
                // Never-used caches have lastUsed == -1, fall back to their creation time.
                for (auto& cache : db.iterate<db::Cache>(
                         where(and_(c(&db::Cache::deleted) == false,
                                    less_than(detail::effectiveLastUse(),
                                              cutoff.time_since_epoch().count()))))) {

                    removed += removeCache(cache, db, toDelete, logger);
                }
                return removed;
            })
            .value_or(size_t{0});

    allRemoved +=
        maintenance.maxPackageSize
            .and_then([&](ByteSize max) -> std::optional<size_t> {
                using namespace sqlite_orm;
                const auto sizes = db.select(
                    columns(as<colalias_a>(total(&db::Cache::size)), &db::Package::name,
                            &db::Package::id),
                    inner_join<db::Package>(on(and_(c(&db::Cache::package) == &db::Package::id,
                                                    c(&db::Cache::deleted) == false))),
                    group_by(&db::Cache::package)
                        .having(greater_than(get<colalias_a>(), std::to_underlying(max))));

                size_t totalRemoved{};
                for (const auto& [size, name, pid] : sizes) {
                    log::info(*logger,
                              "[Maintain] Package: {:20} size {:>10} exceeds given max size {:>10}",
                              name, ByteSize{static_cast<size_t>(size)}, max);

                    const auto overflow = size - std::to_underlying(max);
                    size_t removed{};
                    // Evict by effective last activity; never-used caches (lastUsed == -1)
                    // fall back to their creation time.
                    for (auto& cache : db.iterate<db::Cache>(
                             where(and_(c(&db::Cache::package) == pid,
                                        c(&db::Cache::deleted) == false)),
                             order_by(detail::effectiveLastUse()))) {
                        removed += removeCache(cache, db, toDelete, logger);
                        if (removed > overflow) break;
                    }
                    totalRemoved += removed;
                }
                return totalRemoved;
            })
            .value_or(size_t{0});

    allRemoved +=
        maintenance.maxTotalSize
            .and_then([&](ByteSize max) -> std::optional<size_t> {
                using namespace sqlite_orm;
                const auto totalSize =
                    db.total(&db::Cache::size, where(c(&db::Cache::deleted) == false));

                if (totalSize > std::to_underlying(max)) {
                    const auto overflow = totalSize - std::to_underlying(max);
                    log::info(*logger, "[Maintain] Total Cache size {} exceeds given max {} by {}",
                              ByteSize{static_cast<size_t>(totalSize)}, ByteSize{max},
                              ByteSize(overflow));
                    return overflow;
                } else {
                    log::info(*logger, "[Maintain] Total Cache size {} with given max {}",
                              ByteSize{static_cast<size_t>(totalSize)}, ByteSize{max});
                    return std::nullopt;
                }
            })
            .and_then([&](size_t overflow) -> std::optional<size_t> {
                using namespace sqlite_orm;

                size_t removed{};
                // Evict by effective last activity; never-used caches (lastUsed == -1)
                // fall back to their creation time.
                for (auto& cache : db.iterate<db::Cache>(
                         where(c(&db::Cache::deleted) == false),
                         order_by(detail::effectiveLastUse()))) {
                    removed += removeCache(cache, db, toDelete, logger);
                    if (removed > overflow) break;
                }
                return removed;
            })
            .value_or(size_t{0});

    if (allRemoved > 0) {
        log::info(*logger, "[Maintain] Remove a total of {}", ByteSize{allRemoved});
    }

    if (maintenance.dryrun) {
        db.rollback();
        log::info(*logger, "[Maintain] changes discarded, dry run mode");
    } else {
        db.commit();
        for (const auto& sha : toDelete) {
            store.remove(sha);
        }
    }
    log::info(*logger, "[Maintain] Maintenance finished");
}

}  // namespace vcache
