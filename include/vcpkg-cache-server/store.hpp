#pragma once

#include <vcpkg-cache-server/functional.hpp>

#include <fstream>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <string_view>
#include <ranges>
#include <map>
#include <string>

namespace vcache {

struct Info {
    std::string package{};
    std::string version{};
    std::string arch{};
    std::string sha{};
    std::map<std::string, std::string> ctrl{};
    std::map<std::string, std::string> abi{};
    std::filesystem::file_time_type time{};
    std::size_t size{};
};

Info extractInfo(const std::filesystem::path& path);

fp::UnorderedStringMap<Info> scan(const std::filesystem::path& path,
                                  std::shared_ptr<spdlog::logger> log);

class Store {
public:
    Store(const std::filesystem::path& aRoot, std::shared_ptr<spdlog::logger> aLog);

    bool exists(std::string_view sha) const;

    const Info* info(std::string_view sha);
    const Info* info(std::string_view sha) const;

    std::shared_ptr<std::ifstream> read(std::string_view sha);

    std::shared_ptr<std::ofstream> write(std::string_view sha);

    auto allInfos() const { return infos | std::views::values; }

    std::string statistics() const;

protected:
    std::filesystem::path shaToPath(std::string_view sha) const;

    std::shared_ptr<spdlog::logger> log;
    std::filesystem::path root;
    fp::UnorderedStringMap<Info> infos;
};

}  // namespace vcache
