#include <catch2/catch_test_macros.hpp>

#include <vcpkg-cache-server/maintenance.hpp>

#include <libzippp.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <string>

using namespace vcache;

namespace {

struct Fixture {
    std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        fmt::format("vcache-maintenance-test-{}",
                    std::chrono::steady_clock::now().time_since_epoch().count());
    std::shared_ptr<spdlog::logger> logger = spdlog::default_logger();
    db::Database db = db::create(":memory:");
    const std::string sha = std::string(64, 'a');

    Fixture() { makeZip(sha); }

    void makeZip(const std::string& zipSha) {
        const auto path = root / zipSha.substr(0, 2) / fmt::format("{}.zip", zipSha);
        std::filesystem::create_directories(path.parent_path());
        libzippp::ZipArchive archive{path.generic_string()};
        REQUIRE(archive.open(libzippp::ZipArchive::New));
        const std::string control = "Package: zlib\nVersion: 1.3.1\nArchitecture: x64-linux\n";
        archive.addData("CONTROL", control.data(), control.size());
        const auto abi = fmt::format("abi_tag: {}\n", zipSha);
        archive.addData("share/zlib/vcpkg_abi_info.txt", abi.data(), abi.size());
        archive.close();
    }

    ~Fixture() { std::filesystem::remove_all(root); }

    int addEntry(Store& store, bool deleted) {
        const auto* info = store.info(sha);
        REQUIRE(info);
        return db::addCache(db, db::Cache{.sha = sha,
                                         .package = db::getOrAddPackageId(db, info->package),
                                         .created = info->time.time_since_epoch().count(),
                                         .size = info->size,
                                         .deleted = deleted})
            .id;
    }
};

}  // namespace

TEST_CASE_METHOD(Fixture, "Maintenance retries deletion of files marked deleted", "[maintenance]") {
    Store store{root, logger};
    const auto id = addEntry(store, true);
    Maintenance maintenance;
    maintenance.maxTotalSize = ByteSize{50'000'000'000};

    maintain(store, db, maintenance, logger, Clock::now());

    CHECK(db.get<db::Cache>(id).deleted);
    CHECK_FALSE(store.exists(sha));
    CHECK_FALSE(std::filesystem::exists(root / sha.substr(0, 2) / fmt::format("{}.zip", sha)));
}

TEST_CASE_METHOD(Fixture, "Maintenance dry-run preserves files marked deleted", "[maintenance]") {
    Store store{root, logger};
    const auto id = addEntry(store, true);
    Maintenance maintenance;
    maintenance.dryrun = true;

    maintain(store, db, maintenance, logger, Clock::now());

    CHECK(db.get<db::Cache>(id).deleted);
    CHECK(store.exists(sha));
    CHECK(std::filesystem::exists(root / sha.substr(0, 2) / fmt::format("{}.zip", sha)));
}

TEST_CASE_METHOD(Fixture, "Maintenance enforces the total size limit", "[maintenance]") {
    Store store{root, logger};
    const auto id = addEntry(store, false);
    Maintenance maintenance;
    maintenance.maxTotalSize = ByteSize{1};

    maintain(store, db, maintenance, logger, Clock::now());

    CHECK(db.get<db::Cache>(id).deleted);
    CHECK_FALSE(store.exists(sha));
}

TEST_CASE_METHOD(Fixture, "Maintenance maxUnused keeps recently created never-used caches",
                 "[maintenance]") {
    Store store{root, logger};
    const auto id = addEntry(store, false);  // never used, lastUsed == -1
    Maintenance maintenance;
    maintenance.maxUnused = std::chrono::duration_cast<Duration>(std::chrono::hours{24 * 7});

    maintain(store, db, maintenance, logger, Clock::now());

    CHECK_FALSE(db.get<db::Cache>(id).deleted);
    CHECK(store.exists(sha));
}

TEST_CASE_METHOD(Fixture, "Maintenance maxUnused removes old never-used caches",
                 "[maintenance]") {
    Store store{root, logger};
    const auto* info = store.info(sha);
    REQUIRE(info);

    const auto now = Clock::now();
    // Created 30 days ago, never used (lastUsed == -1)
    const auto id =
        db::addCache(db, db::Cache{.sha = sha,
                                   .package = db::getOrAddPackageId(db, info->package),
                                   .created = (now - std::chrono::duration_cast<Duration>(
                                                         std::chrono::hours{24 * 30}))
                                                  .time_since_epoch()
                                                  .count(),
                                   .size = info->size})
            .id;

    Maintenance maintenance;
    maintenance.maxUnused = std::chrono::duration_cast<Duration>(std::chrono::hours{24 * 7});

    maintain(store, db, maintenance, logger, now);

    CHECK(db.get<db::Cache>(id).deleted);
    CHECK_FALSE(store.exists(sha));
}

TEST_CASE_METHOD(Fixture, "Size eviction removes oldest effective activity first",
                 "[maintenance]") {
    const auto shaB = std::string(64, 'b');
    makeZip(shaB);
    Store store{root, logger};

    const auto* infoA = store.info(sha);
    const auto* infoB = store.info(shaB);
    REQUIRE(infoA);
    REQUIRE(infoB);

    const auto now = Clock::now();
    const auto at = [&](std::chrono::hours ago) {
        return (now - std::chrono::duration_cast<Duration>(ago)).time_since_epoch().count();
    };

    const auto pid = db::getOrAddPackageId(db, infoA->package);
    // A: created 40 days ago, last used 2 days ago -> effective activity 2 days ago
    const auto idA = db::addCache(db, db::Cache{.sha = sha,
                                                .package = pid,
                                                .created = at(std::chrono::hours{24 * 40}),
                                                .lastUsed = at(std::chrono::hours{24 * 2}),
                                                .size = infoA->size})
                         .id;
    // B: created 30 days ago, never used -> effective activity 30 days ago
    const auto idB = db::addCache(db, db::Cache{.sha = shaB,
                                                .package = pid,
                                                .created = at(std::chrono::hours{24 * 30}),
                                                .size = infoB->size})
                         .id;

    Maintenance maintenance;
    maintenance.maxTotalSize = ByteSize{infoA->size + infoB->size - 1};

    maintain(store, db, maintenance, logger, now);

    CHECK(db.get<db::Cache>(idB).deleted);
    CHECK_FALSE(db.get<db::Cache>(idA).deleted);
    CHECK_FALSE(store.exists(shaB));
    CHECK(store.exists(sha));
}