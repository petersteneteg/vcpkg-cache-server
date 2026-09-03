#pragma once

#include <vcpkg-cache-server/store.hpp>
#include <vcpkg-cache-server/settings.hpp>
#include <vcpkg-cache-server/database.hpp>

#include <memory>
#include <string>
#include <vector>

namespace vcache {

namespace detail {
// Marks a cache as deleted in the database and queues its sha for filesystem removal.
size_t removeCache(const db::Cache& cache, db::Database& db, std::vector<std::string>& toDelete,
                   std::shared_ptr<spdlog::logger> logger);
}  // namespace detail

void maintain(Store& store, db::Database& db, const Maintenance& maintenance,
              std::shared_ptr<spdlog::logger> log, Time now);
}  // namespace vcache
