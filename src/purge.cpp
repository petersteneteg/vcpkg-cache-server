#include <vcpkg-cache-server/purge.hpp>
#include <vcpkg-cache-server/maintenance.hpp>
#include <vcpkg-cache-server/functional.hpp>

#include <sqlite_orm/sqlite_orm.h>

#include <ranges>
#include <stdexcept>

namespace vcache {

bool empty(const PurgePattern& pattern) {
    return !pattern.sha && !pattern.package && !pattern.version && !pattern.arch;
}

bool matches(const PurgePattern& pattern, const Info& info) {
    if (pattern.sha && !fp::globMatch(*pattern.sha, info.sha)) return false;
    if (pattern.package && !fp::globMatch(*pattern.package, info.package)) return false;
    if (pattern.version && !fp::globMatch(*pattern.version, info.version)) return false;
    if (pattern.arch && !fp::globMatch(*pattern.arch, info.arch)) return false;
    return true;
}

std::vector<Info> findMatches(const Store& store, const PurgePattern& pattern) {
    return store.allInfos() |
           std::views::filter([&](const Info& info) { return matches(pattern, info); }) |
           std::ranges::to<std::vector>();
}

PurgeResult purge(Store& store, db::Database& db, const PurgePattern& pattern, bool dryrun,
                  std::shared_ptr<spdlog::logger> logger) {
    if (empty(pattern)) {
        throw std::invalid_argument("Refusing to purge: no pattern fields specified");
    }

    auto matched = findMatches(store, pattern);

    std::vector<std::string> toDelete;
    size_t totalSize = 0;

    db.begin_transaction();
    for (const auto& info : matched) {
        if (auto cid = db::getCacheId(db, info.sha)) {
            auto cache = db.get<db::Cache>(*cid);
            if (!cache.deleted) {
                totalSize += detail::removeCache(cache, db, toDelete, logger);
            }
        }
    }

    if (dryrun) {
        db.rollback();
    } else {
        db.commit();
        for (const auto& sha : toDelete) {
            store.remove(sha);
        }
    }

    return PurgeResult{.removed = std::move(matched), .totalSize = totalSize, .dryrun = dryrun};
}

}  // namespace vcache
