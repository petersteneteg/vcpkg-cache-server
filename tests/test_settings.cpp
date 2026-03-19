#include <catch2/catch_test_macros.hpp>

#include <vcpkg-cache-server/settings.hpp>

#include <yaml-cpp/yaml.h>

#include <fstream>
#include <filesystem>
#include <chrono>

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

// ============================================================================
// generateConfigYaml
// ============================================================================

TEST_CASE("generateConfigYaml produces valid YAML", "[settings][generate_config]") {
    Settings s{};
    const auto yaml = generateConfigYaml(s);
    REQUIRE_NOTHROW(YAML::Load(yaml));
}

TEST_CASE("generateConfigYaml default settings: optional keys are commented out",
          "[settings][generate_config]") {
    Settings s{};
    const auto yaml = generateConfigYaml(s);

    // Uncommented YAML should not contain these keys (they should appear only in comments)
    const auto doc = YAML::Load(yaml);
    CHECK_FALSE(doc["cache_dir"]);
    CHECK_FALSE(doc["port"]);
    CHECK_FALSE(doc["log_file"]);
    CHECK_FALSE(doc["db_file"]);
    CHECK_FALSE(doc["ssl"]);
    CHECK_FALSE(doc["auth"]);
}

TEST_CASE("generateConfigYaml default settings: constant-default keys are present",
          "[settings][generate_config]") {
    Settings s{};
    const auto yaml = generateConfigYaml(s);
    const auto doc = YAML::Load(yaml);

    // host and verbosity always have defaults and should appear uncommented
    REQUIRE(doc["host"]);
    CHECK(doc["host"].as<std::string>() == "0.0.0.0");

    REQUIRE(doc["verbosity"]);
    CHECK(doc["verbosity"].as<int>() == static_cast<int>(spdlog::level::info));

    // maintenance section is always emitted
    REQUIRE(doc["maintenance"]);
    CHECK(doc["maintenance"]["dry_run"].as<bool>() == false);
}

TEST_CASE("generateConfigYaml reflects explicitly set values", "[settings][generate_config]") {
    Settings s{};
    s.cacheDir = "/tmp/cache";
    s.dbFile = "/tmp/cache.db";
    s.port = 9090;
    s.host = "127.0.0.1";
    s.logLevel = spdlog::level::debug;
    s.logFile = "/tmp/server.log";
    s.certAndKey = {"/tmp/cert.pem", "/tmp/key.pem"};
    s.auth.write["mytoken"] = "Alice";
    s.maintenance.dryrun = true;
    s.maintenance.maxTotalSize = ByteSize{100'000'000'000};  // 100GB
    s.maintenance.maxPackageSize = ByteSize{1'000'000'000};  // 1GB
    s.maintenance.maxAge = std::chrono::duration_cast<Duration>(std::chrono::years{1});
    s.maintenance.maxUnused = std::chrono::duration_cast<Duration>(std::chrono::days{30});

    const auto yaml = generateConfigYaml(s);
    REQUIRE_NOTHROW(YAML::Load(yaml));
    const auto doc = YAML::Load(yaml);

    CHECK(doc["cache_dir"].as<std::string>() == "/tmp/cache");
    CHECK(doc["db_file"].as<std::string>() == "/tmp/cache.db");
    CHECK(doc["port"].as<int>() == 9090);
    CHECK(doc["host"].as<std::string>() == "127.0.0.1");
    CHECK(doc["verbosity"].as<int>() == static_cast<int>(spdlog::level::debug));
    CHECK(doc["log_file"].as<std::string>() == "/tmp/server.log");
    CHECK(doc["ssl"]["cert"].as<std::string>() == "/tmp/cert.pem");
    CHECK(doc["ssl"]["key"].as<std::string>() == "/tmp/key.pem");
    CHECK(doc["auth"]["mytoken"].as<std::string>() == "Alice");
    CHECK(doc["maintenance"]["dry_run"].as<bool>() == true);
    CHECK(doc["maintenance"]["max_total_size"].as<ByteSize>() == ByteSize{100'000'000'000});
    CHECK(doc["maintenance"]["max_package_size"].as<ByteSize>() == ByteSize{1'000'000'000});
    CHECK(doc["maintenance"]["max_age"].as<Duration>() ==
          std::chrono::duration_cast<Duration>(std::chrono::years{1}));
    CHECK(doc["maintenance"]["max_unused"].as<Duration>() ==
          std::chrono::duration_cast<Duration>(std::chrono::days{30}));

    // Verify that the formatted duration strings are human-readable
    CHECK(doc["maintenance"]["max_age"].as<std::string>() == "1y");
    CHECK(doc["maintenance"]["max_unused"].as<std::string>() == "30d");
    // Verify that the formatted byte-size strings are human-readable
    CHECK(doc["maintenance"]["max_total_size"].as<std::string>() == "100GB");
    CHECK(doc["maintenance"]["max_package_size"].as<std::string>() == "1GB");
}

