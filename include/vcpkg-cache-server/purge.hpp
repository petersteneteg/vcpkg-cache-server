#pragma once

#include <vcpkg-cache-server/store.hpp>
#include <vcpkg-cache-server/database.hpp>

#include <spdlog/spdlog.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace vcache {

// Each set field is a glob pattern ('*' / '?'); unset fields match everything.
// A cache entry must match all set fields to be selected.
struct PurgePattern {
    std::optional<std::string> sha = std::nullopt;
    std::optional<std::string> package = std::nullopt;
    std::optional<std::string> version = std::nullopt;
    std::optional<std::string> arch = std::nullopt;
};

bool empty(const PurgePattern& pattern);
bool matches(const PurgePattern& pattern, const Info& info);
std::vector<Info> findMatches(const Store& store, const PurgePattern& pattern);

struct PurgeResult {
    std::vector<Info> removed;
    size_t totalSize = 0;
    bool dryrun = false;
};

// Refuses to run (throws std::invalid_argument) when `pattern` is empty, to
// guard against an accidental full-cache wipe.
PurgeResult purge(Store& store, db::Database& db, const PurgePattern& pattern, bool dryrun,
                  std::shared_ptr<spdlog::logger> log);

}  // namespace vcache
