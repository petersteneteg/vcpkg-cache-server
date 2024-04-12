#pragma once

#include <vcpkg-cache-server/functional.hpp>

#include <spdlog/spdlog.h>

#include <filesystem>

namespace vcache {

struct Authorization {
    fp::UnorderedStringSet write;
};

struct Settings {
    int port = 443;
    std::filesystem::path base{};
    spdlog::level::level_enum logLevel = spdlog::level::debug;
    Authorization auth{};
    std::filesystem::path cert;
    std::filesystem::path key;
};

Settings parseArgs(int argc, char* argv[]);

}  // namespace vcache
