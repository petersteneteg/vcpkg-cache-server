#include <catch2/catch_test_macros.hpp>

#include <vcpkg-cache-server/functional.hpp>

#include <yaml-cpp/yaml.h>

#include <chrono>

using namespace vcache;

// ============================================================================
// YAML::convert<ByteSize> - ByteSize parsing
// ============================================================================

TEST_CASE("ByteSize YAML parsing handles various units", "[yaml][bytesize]") {
    SECTION("parse TB") {
        YAML::Node node = YAML::Load("5TB");
        auto result = node.as<ByteSize>();
        CHECK(std::to_underlying(result) == 5'000'000'000'000);
    }
    SECTION("parse GB") {
        YAML::Node node = YAML::Load("10GB");
        auto result = node.as<ByteSize>();
        CHECK(std::to_underlying(result) == 10'000'000'000);
    }
    SECTION("parse MB") {
        YAML::Node node = YAML::Load("500MB");
        auto result = node.as<ByteSize>();
        CHECK(std::to_underlying(result) == 500'000'000);
    }
    SECTION("parse kB") {
        YAML::Node node = YAML::Load("100kB");
        auto result = node.as<ByteSize>();
        CHECK(std::to_underlying(result) == 100'000);
    }
    SECTION("parse raw bytes") {
        YAML::Node node = YAML::Load("42");
        auto result = node.as<ByteSize>();
        CHECK(std::to_underlying(result) == 42);
    }
    SECTION("parse with whitespace") {
        YAML::Node node = YAML::Load("  10 GB  ");
        auto result = node.as<ByteSize>();
        CHECK(std::to_underlying(result) == 10'000'000'000);
    }
    SECTION("parse 1GB") {
        YAML::Node node = YAML::Load("1GB");
        auto result = node.as<ByteSize>();
        CHECK(std::to_underlying(result) == 1'000'000'000);
    }
    SECTION("parse 1TB") {
        YAML::Node node = YAML::Load("1TB");
        auto result = node.as<ByteSize>();
        CHECK(std::to_underlying(result) == 1'000'000'000'000);
    }
}

TEST_CASE("ByteSize YAML encoding roundtrips", "[yaml][bytesize]") {
    SECTION("encode and decode") {
        ByteSize original{1'000'000'000};
        YAML::Node node = YAML::convert<ByteSize>::encode(original);
        ByteSize decoded{};
        REQUIRE(YAML::convert<ByteSize>::decode(node, decoded));
        CHECK(std::to_underlying(decoded) == std::to_underlying(original));
    }
}

TEST_CASE("ByteSize YAML parsing rejects invalid input", "[yaml][bytesize]") {
    SECTION("non-numeric") {
        YAML::Node node = YAML::Load("abc");
        ByteSize result{};
        CHECK_FALSE(YAML::convert<ByteSize>::decode(node, result));
    }
    SECTION("sequence node is rejected") {
        YAML::Node node = YAML::Load("[1, 2, 3]");
        ByteSize result{};
        CHECK_FALSE(YAML::convert<ByteSize>::decode(node, result));
    }
}

// ============================================================================
// YAML::convert<Duration> - Duration parsing
// ============================================================================

TEST_CASE("Duration YAML parsing handles time units", "[yaml][duration]") {
    using namespace std::chrono;

    SECTION("parse seconds") {
        YAML::Node node = YAML::Load("30s");
        auto result = node.as<seconds>();
        CHECK(result == seconds{30});
    }
    SECTION("parse minutes") {
        YAML::Node node = YAML::Load("5m");
        auto result = node.as<seconds>();
        CHECK(result == minutes{5});
    }
    SECTION("parse hours") {
        YAML::Node node = YAML::Load("2h");
        auto result = node.as<seconds>();
        CHECK(result == hours{2});
    }
    SECTION("parse days") {
        YAML::Node node = YAML::Load("1d");
        auto result = node.as<seconds>();
        CHECK(result == days{1});
    }
    SECTION("parse years") {
        YAML::Node node = YAML::Load("1y");
        auto result = node.as<seconds>();
        CHECK(result == years{1});
    }
    SECTION("parse plain number as seconds") {
        YAML::Node node = YAML::Load("60");
        auto result = node.as<seconds>();
        CHECK(result == seconds{60});
    }
}

TEST_CASE("Duration YAML parsing rejects invalid input", "[yaml][duration]") {
    using namespace std::chrono;

    SECTION("non-numeric") {
        YAML::Node node = YAML::Load("abc");
        seconds result{};
        CHECK_FALSE(YAML::convert<seconds>::decode(node, result));
    }
    SECTION("sequence node is rejected") {
        YAML::Node node = YAML::Load("[1, 2, 3]");
        seconds result{};
        CHECK_FALSE(YAML::convert<seconds>::decode(node, result));
    }
}

TEST_CASE("Duration YAML parsing handles multi-part durations", "[yaml][duration]") {
    using namespace std::chrono;

    SECTION("days and hours") {
        YAML::Node node = YAML::Load("1d 2h");
        auto result = node.as<seconds>();
        CHECK(result == duration_cast<seconds>(days{1}) + hours{2});
    }
    SECTION("hours and minutes") {
        YAML::Node node = YAML::Load("3h 30m");
        auto result = node.as<seconds>();
        CHECK(result == hours{3} + minutes{30});
    }
    SECTION("days, hours, minutes, and seconds") {
        YAML::Node node = YAML::Load("1d 2h 30m 15s");
        auto result = node.as<seconds>();
        CHECK(result == duration_cast<seconds>(days{1}) + hours{2} + minutes{30} + seconds{15});
    }
    SECTION("years and days") {
        YAML::Node node = YAML::Load("1y 30d");
        auto result = node.as<seconds>();
        CHECK(result == duration_cast<seconds>(years{1}) + duration_cast<seconds>(days{30}));
    }
}
