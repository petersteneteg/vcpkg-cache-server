#pragma once

#include <vcpkg-cache-server/functional.hpp>

#include <spdlog/spdlog.h>

#include <filesystem>

namespace vcache {

struct Authorization {
    fp::UnorderedStringMap<std::string> write;
};

struct Settings {
    std::filesystem::path cacheDir{};
    spdlog::level::level_enum logLevel = spdlog::level::info;
    Authorization auth{};
    std::optional<std::pair<std::filesystem::path,std::filesystem::path>> certAndKey = std::nullopt;
    int port = -1;
    std::string host = "0.0.0.0";
    std::optional<std::filesystem::path> logFile = std::nullopt;
};

Settings parseArgs(int argc, char* argv[]);

}  // namespace vcache
