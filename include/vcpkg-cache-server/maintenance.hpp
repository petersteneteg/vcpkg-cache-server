#pragma once

#include <vcpkg-cache-server/store.hpp>
#include <vcpkg-cache-server/settings.hpp>
#include <vcpkg-cache-server/database.hpp>

namespace vcache {

void maintain(Store& store, db::Database& db, const Maintenance& maintenance,
              std::shared_ptr<spdlog::logger> log, Time now);
}
