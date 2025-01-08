#pragma once

#include <string>
#include <filesystem>
#include <optional>
#include <ostream>
#include <chrono>

inline std::ostream& operator<<(std::ostream& dest, __int128_t value) {
    std::ostream::sentry s(dest);
    if (s) {
        __uint128_t tmp = value < 0 ? -value : value;
        char buffer[128];
        char* d = std::end(buffer);
        do {
            --d;
            *d = "0123456789"[tmp % 10];
            tmp /= 10;
        } while (tmp != 0);
        if (value < 0) {
            --d;
            *d = '-';
        }
        int len = static_cast<int>(std::end(buffer) - d);
        if (dest.rdbuf()->sputn(d, len) != len) {
            dest.setstate(std::ios_base::badbit);
        }
    }
    return dest;
}

#include <sqlite_orm/sqlite_orm.h>

namespace vcache::db {

using Time = std::filesystem::file_time_type;
using Duration = Time::duration;
using Clock = Time::clock;
using Rep = Time::rep;

struct Package {
    int id = -1;
    std::string name{};
    Rep lastUsed = -1;
    size_t downloads = 0;
};

struct Cache {
    int id = -1;
    std::string sha{};
    int package = -1;
    Rep created{};
    std::string ip{};
    std::string user{};
    Rep lastUsed = -1;
    size_t downloads = 0;
    size_t size{};
    bool deleted = false;
};

struct Download {
    int id = -1;
    int cache = -1;
    std::string ip{};
    std::string user{};
    Rep time{};
};

inline auto create(const std::filesystem::path& file) {
    auto storage = sqlite_orm::make_storage(
        file.string(),
        sqlite_orm::make_table(
            "packages",
            sqlite_orm::make_column("id", &Package::id, sqlite_orm::primary_key().autoincrement()),
            sqlite_orm::make_column("name", &Package::name, sqlite_orm::unique()),
            sqlite_orm::make_column("lastUsed", &Package::lastUsed),
            sqlite_orm::make_column("downloads", &Package::downloads)),
        sqlite_orm::make_table(
            "caches",
            sqlite_orm::make_column("id", &Cache::id, sqlite_orm::primary_key().autoincrement()),
            sqlite_orm::make_column("sha", &Cache::sha, sqlite_orm::unique()),
            sqlite_orm::make_column("package", &Cache::package),
            sqlite_orm::make_column("created", &Cache::created),
            sqlite_orm::make_column("ip", &Cache::ip),
            sqlite_orm::make_column("user", &Cache::user),
            sqlite_orm::make_column("lastUsed", &Cache::lastUsed),
            sqlite_orm::make_column("downloads", &Cache::downloads),
            sqlite_orm::make_column("size", &Cache::size),
            sqlite_orm::make_column("deleted", &Cache::deleted),
            sqlite_orm::foreign_key(&Cache::package).references(&Package::id)),
        sqlite_orm::make_table(
            "downloads",
            sqlite_orm::make_column("id", &Download::id, sqlite_orm::primary_key().autoincrement()),
            sqlite_orm::make_column("cache", &Download::cache),
            sqlite_orm::make_column("ip", &Download::ip),
            sqlite_orm::make_column("user", &Download::user),
            sqlite_orm::make_column("time", &Download::time),
            sqlite_orm::foreign_key(&Download::cache).references(&Cache::id)));

    storage.sync_schema();
    return storage;
}

using Database = decltype(create({}));

inline int getOrAddPackageId(Database& db, std::string_view name) {
    using namespace sqlite_orm;
    auto pIds = db.select(&Package::id, where(c(&Package::name) == name));
    if (pIds.empty()) {
        return db.insert(Package{.name = std::string{name}});
    } else {
        return pIds.front();
    }
}

inline std::optional<int> getCacheId(Database& db, std::string_view sha) {
    using namespace sqlite_orm;
    auto cIds = db.select(&Cache::id, where(c(&Cache::sha) == sha));
    if (cIds.empty()) {
        return std::nullopt;
    } else {
        return cIds.front();
    }
}

inline Cache addCache(Database& db, Cache&& cache) {
    auto id = db.insert(cache);
    cache.id = id;
    return std::move(cache);
}

inline Download addDownload(Database& db, Download&& download) {
    auto id = db.insert(download);
    download.id = id;
    return std::move(download);
}

inline void updateLastUse(Database& db, int cid, Time t) {
    using namespace sqlite_orm;

    auto cache = db.get<Cache>(cid);
    cache.lastUsed = t.time_since_epoch().count();
    ++cache.downloads;
    db.update(cache);

    auto package = db.get<Package>(cache.package);
    package.lastUsed = t.time_since_epoch().count();
    ++package.downloads;
    db.update(package);
}

inline std::pair<size_t, Time> getPackageDownloadsAndLastUse(Database& db, std::string_view name) {
    using namespace sqlite_orm;
    auto res = db.select(columns(&Package::downloads, &Package::lastUsed),
                         where(c(&Package::name) == name));
    if (res.size() != 1) {
        throw std::runtime_error("invalid package name");
    }

    const auto [downloads, reps] = res.front();

    return {downloads, Time{Duration{reps}}};
}

inline std::string formatTimeStamp(Rep rep) {
    return rep != -1 ? std::format("{:%Y-%m-%d %H:%M}", Time{Duration{rep}})
                     : std::string{"Unused"};
}

inline std::string formatTimeStamp(Time time) {
    return formatTimeStamp(time.time_since_epoch().count());
}

}  // namespace vcache::db
