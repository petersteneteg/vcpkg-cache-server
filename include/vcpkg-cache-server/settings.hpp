#pragma once

#include <vcpkg-cache-server/functional.hpp>

#include <spdlog/spdlog.h>

#include <filesystem>

namespace vcache {

using Time = std::filesystem::file_time_type;
using Duration = Time::duration;

struct Authorization {
    fp::UnorderedStringMap<std::string> write;
};

struct Maintenance {
    bool dryrun = false;
    std::optional<ByteSize> maxTotalSize = std::nullopt;
    std::optional<ByteSize> maxPackageSize = std::nullopt;
    std::optional<Duration> maxAge = std::nullopt;
    std::optional<Duration> maxUnused = std::nullopt;
};

struct Settings {
    std::filesystem::path cacheDir{};
    std::filesystem::path dbFile;
    spdlog::level::level_enum logLevel = spdlog::level::info;
    Authorization auth{};
    std::optional<std::pair<std::filesystem::path, std::filesystem::path>> certAndKey =
        std::nullopt;
    int port = -1;
    std::string host = "0.0.0.0";
    std::optional<std::filesystem::path> logFile = std::nullopt;

    Maintenance maintenance;
};

Settings parseArgs(int argc, char* argv[]);

}  // namespace vcache
