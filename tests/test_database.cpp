#include <catch2/catch_test_macros.hpp>

#include <vcpkg-cache-server/database.hpp>

#include <string>
#include <optional>

using namespace vcache::db;

// Helper to create an in-memory database for testing
static Database createTestDb() { return create(":memory:"); }

// ============================================================================
// Database creation
// ============================================================================

TEST_CASE("Database can be created in-memory", "[database]") {
    auto db = createTestDb();
    // If we get here without throwing, the schema was created successfully
    CHECK(db.count<Package>() == 0);
    CHECK(db.count<Cache>() == 0);
    CHECK(db.count<Download>() == 0);
}

// ============================================================================
// getOrAddPackageId
// ============================================================================

TEST_CASE("getOrAddPackageId creates new package on first call", "[database]") {
    auto db = createTestDb();

    int id = getOrAddPackageId(db, "my-package");
    CHECK(id > 0);
    CHECK(db.count<Package>() == 1);
}

TEST_CASE("getOrAddPackageId returns existing id on subsequent calls", "[database]") {
    auto db = createTestDb();

    int id1 = getOrAddPackageId(db, "my-package");
    int id2 = getOrAddPackageId(db, "my-package");
    CHECK(id1 == id2);
    CHECK(db.count<Package>() == 1);
}

TEST_CASE("getOrAddPackageId creates separate packages for different names", "[database]") {
    auto db = createTestDb();

    int id1 = getOrAddPackageId(db, "package-a");
    int id2 = getOrAddPackageId(db, "package-b");
    CHECK(id1 != id2);
    CHECK(db.count<Package>() == 2);
}

// ============================================================================
// getCacheId
// ============================================================================

TEST_CASE("getCacheId returns nullopt for non-existent SHA", "[database]") {
    auto db = createTestDb();

    auto result = getCacheId(db, "nonexistent-sha");
    CHECK_FALSE(result.has_value());
}

TEST_CASE("getCacheId returns id for existing cache entry", "[database]") {
    auto db = createTestDb();
    int pkgId = getOrAddPackageId(db, "test-package");

    Cache cache{.sha = "abc123", .package = pkgId, .size = 1024};
    auto inserted = addCache(db, std::move(cache));

    auto result = getCacheId(db, "abc123");
    REQUIRE(result.has_value());
    CHECK(*result == inserted.id);
}

// ============================================================================
// addCache
// ============================================================================

TEST_CASE("addCache inserts a cache entry and returns it with id", "[database]") {
    auto db = createTestDb();
    int pkgId = getOrAddPackageId(db, "test-package");

    Cache cache{.sha = "sha256hash",
                .package = pkgId,
                .created = 1000,
                .ip = "192.168.1.1",
                .user = "testuser",
                .lastUsed = 1000,
                .downloads = 0,
                .size = 2048,
                .deleted = false};

    auto result = addCache(db, std::move(cache));
    CHECK(result.id > 0);
    CHECK(result.sha == "sha256hash");
    CHECK(result.package == pkgId);
    CHECK(result.size == 2048);
    CHECK(db.count<Cache>() == 1);
}

TEST_CASE("addCache preserves all fields", "[database]") {
    auto db = createTestDb();
    int pkgId = getOrAddPackageId(db, "fmt");

    Cache cache{.sha = "deadbeef",
                .package = pkgId,
                .created = 12345,
                .ip = "10.0.0.1",
                .user = "ci-bot",
                .lastUsed = 54321,
                .downloads = 5,
                .size = 4096,
                .deleted = false};

    auto result = addCache(db, std::move(cache));

    auto retrieved = db.get<Cache>(result.id);
    CHECK(retrieved.sha == "deadbeef");
    CHECK(retrieved.ip == "10.0.0.1");
    CHECK(retrieved.user == "ci-bot");
    CHECK(retrieved.created == 12345);
    CHECK(retrieved.lastUsed == 54321);
    CHECK(retrieved.downloads == 5);
    CHECK(retrieved.size == 4096);
    CHECK(retrieved.deleted == false);
}

// ============================================================================
// addDownload
// ============================================================================

TEST_CASE("addDownload inserts a download entry", "[database]") {
    auto db = createTestDb();
    int pkgId = getOrAddPackageId(db, "test-package");
    auto cache = addCache(db, Cache{.sha = "sha1", .package = pkgId, .size = 100});

    Download download{.cache = cache.id, .ip = "192.168.1.1", .user = "user1", .time = 999};

    auto result = addDownload(db, std::move(download));
    CHECK(result.id > 0);
    CHECK(result.cache == cache.id);
    CHECK(db.count<Download>() == 1);
}

// ============================================================================
// updateLastUse
// ============================================================================

TEST_CASE("updateLastUse increments download count and updates timestamp", "[database]") {
    auto db = createTestDb();
    int pkgId = getOrAddPackageId(db, "test-package");

    Cache cache{.sha = "update-test",
                .package = pkgId,
                .lastUsed = 100,
                .downloads = 0,
                .size = 512};
    auto inserted = addCache(db, std::move(cache));

    Time newTime{Duration{500}};
    updateLastUse(db, inserted.id, newTime);

    auto updated = db.get<Cache>(inserted.id);
    CHECK(updated.downloads == 1);
    CHECK(updated.lastUsed == 500);

    auto pkg = db.get<Package>(pkgId);
    CHECK(pkg.downloads == 1);
    CHECK(pkg.lastUsed == 500);
}

TEST_CASE("updateLastUse accumulates downloads", "[database]") {
    auto db = createTestDb();
    int pkgId = getOrAddPackageId(db, "test-package");

    Cache cache{.sha = "multi-dl", .package = pkgId, .downloads = 0, .size = 256};
    auto inserted = addCache(db, std::move(cache));

    updateLastUse(db, inserted.id, Time{Duration{100}});
    updateLastUse(db, inserted.id, Time{Duration{200}});
    updateLastUse(db, inserted.id, Time{Duration{300}});

    auto updated = db.get<Cache>(inserted.id);
    CHECK(updated.downloads == 3);
    CHECK(updated.lastUsed == 300);
}

// ============================================================================
// getPackageDownloadsAndLastUse
// ============================================================================

TEST_CASE("getPackageDownloadsAndLastUse returns correct values", "[database]") {
    auto db = createTestDb();
    int pkgId = getOrAddPackageId(db, "test-package");

    Cache cache{.sha = "pkg-dl-test", .package = pkgId, .downloads = 0, .size = 100};
    auto inserted = addCache(db, std::move(cache));

    updateLastUse(db, inserted.id, Time{Duration{42}});

    auto [downloads, lastUse] = getPackageDownloadsAndLastUse(db, "test-package");
    CHECK(downloads == 1);
    CHECK(lastUse.time_since_epoch().count() == 42);
}

TEST_CASE("getPackageDownloadsAndLastUse throws for invalid package", "[database]") {
    auto db = createTestDb();
    CHECK_THROWS_AS(getPackageDownloadsAndLastUse(db, "nonexistent"), std::runtime_error);
}

// ============================================================================
// getCacheDownloadsAndLastUse
// ============================================================================

TEST_CASE("getCacheDownloadsAndLastUse returns correct values", "[database]") {
    auto db = createTestDb();
    int pkgId = getOrAddPackageId(db, "test-package");

    Cache cache{.sha = "cache-dl-test", .package = pkgId, .downloads = 0, .size = 100};
    auto inserted = addCache(db, std::move(cache));

    updateLastUse(db, inserted.id, Time{Duration{99}});

    auto [downloads, lastUse] = getCacheDownloadsAndLastUse(db, "cache-dl-test");
    CHECK(downloads == 1);
    CHECK(lastUse.time_since_epoch().count() == 99);
}

TEST_CASE("getCacheDownloadsAndLastUse throws for invalid SHA", "[database]") {
    auto db = createTestDb();
    CHECK_THROWS_AS(getCacheDownloadsAndLastUse(db, "nonexistent-sha"), std::runtime_error);
}

// ============================================================================
// Multiple packages and caches
// ============================================================================

TEST_CASE("Database handles multiple packages with multiple caches", "[database]") {
    auto db = createTestDb();

    int pkg1 = getOrAddPackageId(db, "fmt");
    int pkg2 = getOrAddPackageId(db, "spdlog");

    auto cache1 = addCache(db, Cache{.sha = "sha1", .package = pkg1, .size = 100});
    auto cache2 = addCache(db, Cache{.sha = "sha2", .package = pkg1, .size = 200});
    auto cache3 = addCache(db, Cache{.sha = "sha3", .package = pkg2, .size = 300});

    CHECK(db.count<Package>() == 2);
    CHECK(db.count<Cache>() == 3);

    CHECK(getCacheId(db, "sha1").has_value());
    CHECK(getCacheId(db, "sha2").has_value());
    CHECK(getCacheId(db, "sha3").has_value());
    CHECK_FALSE(getCacheId(db, "sha4").has_value());
}
