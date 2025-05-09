#pragma once

#include <vcpkg-cache-server/functional.hpp>

#include <fstream>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <string_view>
#include <ranges>
#include <map>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <memory>

namespace vcache {

using Time = std::filesystem::file_time_type;
using Duration = Time::duration;
using Clock = Time::clock;
using Rep = Time::rep;

struct Info {
    std::string package{};
    std::string version{};
    std::string arch{};
    std::string sha{};
    std::map<std::string, std::string> ctrl{};
    std::map<std::string, std::string> abi{};
    Time time{};
    std::size_t size{};
};

enum class InfoState { Valid, Writing, Deleted };

Info extractInfo(const std::filesystem::path& path);

fp::UnorderedStringMap<std::pair<InfoState, Info>> scan(const std::filesystem::path& path,
                                                        std::shared_ptr<spdlog::logger> log);

class StoreWriter;
class StoreReader;

template <typename T>
struct WrapWithLock : T {
    WrapWithLock(std::shared_mutex& m, T t)
        : T{t}, lock{std::make_shared<std::shared_lock<std::shared_mutex>>(m)} {}

private:
    std::shared_ptr<std::shared_lock<std::shared_mutex>> lock;
};

template <typename T>
WrapWithLock(T t) -> WrapWithLock<T>;

class Store {
public:
    Store(const std::filesystem::path& aRoot, std::shared_ptr<spdlog::logger> aLog);

    bool exists(std::string_view sha) const;

    const Info* info(std::string_view sha);
    const Info* info(std::string_view sha) const;

    std::shared_ptr<StoreReader> read(std::string_view sha);
    std::shared_ptr<StoreWriter> write(std::string_view sha);

    auto allInfos() const {
        return WrapWithLock{smtx, infos | std::views::filter([](const auto& item) {
                                      return item.second.first == InfoState::Valid;
                                  }) | std::views::transform([](const auto& item) -> const Info& {
                                      return item.second.second;
                                  })};
    }

    std::string statistics() const;

    void remove(std::string_view sha);

private:
    friend StoreWriter;
    friend StoreReader;
    struct Token {};

    std::filesystem::path shaToPath(std::string_view sha) const;

    /* smtx synchronizes read and writing to infos.
     * We make sure to never remove any info items to prevent the need of synchronization.
     */
    mutable std::shared_mutex smtx;
    std::shared_ptr<spdlog::logger> log;
    std::filesystem::path root;
    fp::UnorderedStringMap<std::pair<InfoState, Info>> infos;
};

class StoreReader {
public:
    StoreReader(Store& store, std::pair<InfoState, Info>& infoItem, typename Store::Token)
        : infoItem{infoItem}
        , stream{store.shaToPath(infoItem.second.sha), std::ios_base::in | std::ios_base::binary} {}

    std::ifstream& getStream() { return stream; }
    const Info& getInfo() const { return infoItem.second; }

private:
    std::pair<InfoState, Info>& infoItem;
    std::ifstream stream;
};

class StoreWriter {
public:
    StoreWriter(Store& store, std::pair<InfoState, Info>& infoItem,
                const std::filesystem::path& path, typename Store::Token);
    ~StoreWriter();

    std::ofstream& getStream() { return stream; }

private:
    Store& store;
    std::pair<InfoState, Info>& infoItem;
    std::filesystem::path path;
    std::ofstream stream;
};

}  // namespace vcache
