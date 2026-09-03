#include <catch2/catch_test_macros.hpp>

#include <vcpkg-cache-server/purge.hpp>
#include <vcpkg-cache-server/store.hpp>
#include <vcpkg-cache-server/database.hpp>

#include <libzippp.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <string>
#include <string_view>

using namespace vcache;

namespace {

Info makeInfo(std::string_view sha, std::string_view package, std::string_view version,
              std::string_view arch) {
    return Info{.package = std::string{package},
                .version = std::string{version},
                .arch = std::string{arch},
                .sha = std::string{sha}};
}

// Writes a minimal but valid cache zip (CONTROL + vcpkg_abi_info.txt) that
// Store::scan()/extractInfo() can parse.
void writeCacheZip(const std::filesystem::path& root, std::string_view sha,
                   std::string_view package, std::string_view version, std::string_view arch) {
    const auto path = root / sha.substr(0, 2) / fmt::format("{}.zip", sha);
    std::filesystem::create_directories(path.parent_path());

    libzippp::ZipArchive zf{path.generic_string()};
    REQUIRE(zf.open(libzippp::ZipArchive::New));

    const auto control =
        fmt::format("Package: {}\nVersion: {}\nArchitecture: {}\n", package, version, arch);
    zf.addData("CONTROL", control.data(), control.size());

    const auto abi = fmt::format("abi_tag: {}\n", sha);
    zf.addData(fmt::format("share/{}/vcpkg_abi_info.txt", package), abi.data(), abi.size());

    zf.close();
}

struct Fixture {
    std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        fmt::format("vcache-purge-test-{}",
                    std::chrono::steady_clock::now().time_since_epoch().count());
    std::shared_ptr<spdlog::logger> logger = spdlog::default_logger();
    db::Database db = db::create(":memory:");

    std::optional<Store> storeOpt;

    Fixture() { std::filesystem::create_directories(root); }
    ~Fixture() { std::filesystem::remove_all(root); }

    Store& makeStore() {
        storeOpt.emplace(root, logger);
        for (const auto& info : storeOpt->allInfos()) {
            const auto pid = db::getOrAddPackageId(db, info.package);
            db::addCache(db, db::Cache{.sha = info.sha,
                                       .package = pid,
                                       .created = info.time.time_since_epoch().count(),
                                       .size = info.size});
        }
        return *storeOpt;
    }
};

}  // namespace

// ============================================================================
// empty
// ============================================================================

TEST_CASE("empty is true when no pattern field is set", "[purge]") { CHECK(empty(PurgePattern{})); }

TEST_CASE("empty is false when any pattern field is set", "[purge]") {
    CHECK_FALSE(empty(PurgePattern{.sha = "abc"}));
    CHECK_FALSE(empty(PurgePattern{.package = "zlib"}));
    CHECK_FALSE(empty(PurgePattern{.version = "1.0"}));
    CHECK_FALSE(empty(PurgePattern{.arch = "x64-linux"}));
}

// ============================================================================
// matches
// ============================================================================

TEST_CASE("matches with no fields set matches everything", "[purge]") {
    const auto info = makeInfo("abc123", "zlib", "1.3.1", "x64-linux");
    CHECK(matches(PurgePattern{}, info));
}

TEST_CASE("matches requires all set fields to match (AND)", "[purge]") {
    const auto info = makeInfo("abc123", "zlib", "1.3.1", "x64-linux");

    CHECK(matches(PurgePattern{.package = "zlib"}, info));
    CHECK(matches(PurgePattern{.package = "zlib", .arch = "x64-*"}, info));
    CHECK_FALSE(matches(PurgePattern{.package = "zlib", .arch = "x86-*"}, info));
    CHECK_FALSE(matches(PurgePattern{.package = "boost*"}, info));
}

TEST_CASE("matches supports glob wildcards per field", "[purge]") {
    const auto info = makeInfo("abc123def456", "nlohmann-json", "3.11.2", "x64-windows");

    CHECK(matches(PurgePattern{.sha = "abc123*"}, info));
    CHECK(matches(PurgePattern{.package = "nlohmann-*"}, info));
    CHECK(matches(PurgePattern{.version = "3.11.*"}, info));
    CHECK(matches(PurgePattern{.arch = "*-windows"}, info));
    CHECK_FALSE(matches(PurgePattern{.arch = "*-linux"}, info));
}

// ============================================================================
// findMatches / purge (require a real Store backed by on-disk zip caches)
// ============================================================================

TEST_CASE_METHOD(Fixture, "findMatches filters entries by pattern", "[purge]") {
    writeCacheZip(root, std::string(64, 'a'), "zlib", "1.3.1", "x64-linux");
    writeCacheZip(root, std::string(64, 'b'), "zlib", "1.3.1", "x64-windows");
    writeCacheZip(root, std::string(64, 'c'), "boost-core", "1.85.0", "x64-linux");

    auto& store = makeStore();

    CHECK(findMatches(store, PurgePattern{.package = "zlib"}).size() == 2);
    CHECK(findMatches(store, PurgePattern{.package = "zlib", .arch = "x64-linux"}).size() == 1);
    CHECK(findMatches(store, PurgePattern{}).size() == 3);
    CHECK(findMatches(store, PurgePattern{.package = "does-not-exist"}).empty());
}

TEST_CASE_METHOD(Fixture, "purge refuses to run with an empty pattern", "[purge]") {
    writeCacheZip(root, std::string(64, 'a'), "zlib", "1.3.1", "x64-linux");
    auto& store = makeStore();

    CHECK_THROWS_AS(purge(store, db, PurgePattern{}, /*dryrun=*/true, logger),
                    std::invalid_argument);
}

TEST_CASE_METHOD(Fixture, "purge dry-run leaves the store and database untouched", "[purge]") {
    const auto sha = std::string(64, 'a');
    writeCacheZip(root, sha, "zlib", "1.3.1", "x64-linux");
    auto& store = makeStore();

    const auto result = purge(store, db, PurgePattern{.package = "zlib"}, /*dryrun=*/true, logger);

    CHECK(result.dryrun);
    CHECK(result.removed.size() == 1);
    CHECK(result.totalSize > 0);
    CHECK(store.exists(sha));
    CHECK_FALSE(db.get<db::Cache>(*db::getCacheId(db, sha)).deleted);
}

TEST_CASE_METHOD(Fixture, "purge removes matching entries from store and database", "[purge]") {
    const auto keepSha = std::string(64, 'a');
    const auto removeSha = std::string(64, 'b');
    writeCacheZip(root, keepSha, "boost-core", "1.85.0", "x64-linux");
    writeCacheZip(root, removeSha, "zlib", "1.3.1", "x64-linux");
    auto& store = makeStore();

    const auto result = purge(store, db, PurgePattern{.package = "zlib"}, /*dryrun=*/false, logger);

    CHECK_FALSE(result.dryrun);
    CHECK(result.removed.size() == 1);
    CHECK_FALSE(store.exists(removeSha));
    CHECK(db.get<db::Cache>(*db::getCacheId(db, removeSha)).deleted);

    CHECK(store.exists(keepSha));
    CHECK_FALSE(db.get<db::Cache>(*db::getCacheId(db, keepSha)).deleted);
}
