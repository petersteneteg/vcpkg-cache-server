#include <catch2/catch_test_macros.hpp>

#include <vcpkg-cache-server/settings.hpp>

#include <yaml-cpp/yaml.h>

#include <fstream>
#include <filesystem>

using namespace vcache;

// ============================================================================
// Settings struct defaults
// ============================================================================

TEST_CASE("Settings has sensible defaults", "[settings]") {
    Settings s{};
    CHECK(s.cacheDir.empty());
    CHECK(s.logLevel == spdlog::level::info);
    CHECK(s.port == -1);
    CHECK(s.host == "0.0.0.0");
    CHECK_FALSE(s.certAndKey.has_value());
    CHECK_FALSE(s.logFile.has_value());
}

TEST_CASE("Maintenance has sensible defaults", "[settings]") {
    Maintenance m{};
    CHECK(m.dryrun == false);
    CHECK_FALSE(m.maxTotalSize.has_value());
    CHECK_FALSE(m.maxPackageSize.has_value());
    CHECK_FALSE(m.maxAge.has_value());
    CHECK_FALSE(m.maxUnused.has_value());
}

// ============================================================================
// Authorization
// ============================================================================

TEST_CASE("Authorization write map supports heterogeneous lookup", "[settings]") {
    Authorization auth;
    auth.write["token123"] = "User 1";
    auth.write["token456"] = "User 2";

    CHECK(auth.write.contains(std::string_view{"token123"}));
    CHECK(auth.write.contains(std::string_view{"token456"}));
    CHECK_FALSE(auth.write.contains(std::string_view{"badtoken"}));
}
