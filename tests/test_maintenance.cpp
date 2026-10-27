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

    Fixture() {
        const auto path = root / sha.substr(0, 2) / fmt::format("{}.zip", sha);
        std::filesystem::create_directories(path.parent_path());
        libzippp::ZipArchive archive{path.generic_string()};
        REQUIRE(archive.open(libzippp::ZipArchive::New));
        const std::string control = "Package: zlib\nVersion: 1.3.1\nArchitecture: x64-linux\n";
        archive.addData("CONTROL", control.data(), control.size());
        const auto abi = fmt::format("abi_tag: {}\n", sha);
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