#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <vcpkg-cache-server/functional.hpp>

#include <fmt/format.h>
#include <string>

using namespace vcache;
using Catch::Matchers::ContainsSubstring;

// ============================================================================
// fmt::formatter<ByteSize> - Auto unit selection
// ============================================================================

TEST_CASE("ByteSize formatter auto-selects appropriate unit", "[fmt][bytesize]") {
    SECTION("bytes") {
        auto result = fmt::format("{}", ByteSize{42});
        CHECK(result == "42 B");
    }
    SECTION("kilobytes") {
        auto result = fmt::format("{}", ByteSize{1'500});
        CHECK_THAT(result, ContainsSubstring("kB"));
    }
    SECTION("megabytes") {
        auto result = fmt::format("{}", ByteSize{1'500'000});
        CHECK_THAT(result, ContainsSubstring("MB"));
    }
    SECTION("gigabytes") {
        auto result = fmt::format("{}", ByteSize{1'500'000'000});
        CHECK_THAT(result, ContainsSubstring("GB"));
    }
    SECTION("terabytes") {
        auto result = fmt::format("{}", ByteSize{2'000'000'000'000});
        CHECK_THAT(result, ContainsSubstring("TB"));
    }
    SECTION("zero bytes") {
        auto result = fmt::format("{}", ByteSize{0});
        CHECK(result == "0 B");
    }
}

// ============================================================================
// fmt::formatter<ByteSize> - Explicit unit prefix
// ============================================================================

TEST_CASE("ByteSize formatter respects explicit unit prefix", "[fmt][bytesize]") {
    SECTION("force kB") {
        auto result = fmt::format("{:k}", ByteSize{1'500'000});
        CHECK_THAT(result, ContainsSubstring("kB"));
    }
    SECTION("force MB") {
        auto result = fmt::format("{:M}", ByteSize{1'500'000'000});
        CHECK_THAT(result, ContainsSubstring("MB"));
    }
    SECTION("force GB") {
        auto result = fmt::format("{:G}", ByteSize{2'000'000'000'000});
        CHECK_THAT(result, ContainsSubstring("GB"));
    }
    SECTION("force TB") {
        auto result = fmt::format("{:T}", ByteSize{42});
        CHECK_THAT(result, ContainsSubstring("TB"));
    }
}

// ============================================================================
// fmt::formatter<FormatDuration>
// ============================================================================

TEST_CASE("FormatDuration formatter formats durations correctly", "[fmt][duration]") {
    using namespace std::chrono;
    using namespace std::chrono_literals;

    SECTION("seconds only") {
        auto result = fmt::format("{}", FormatDuration{30s});
        CHECK_THAT(result, ContainsSubstring("30s"));
    }
    SECTION("minutes only") {
        auto result = fmt::format("{}", FormatDuration{duration_cast<Duration>(5min)});
        CHECK_THAT(result, ContainsSubstring("5m"));
    }
    SECTION("hours only") {
        auto result = fmt::format("{}", FormatDuration{duration_cast<Duration>(2h)});
        CHECK_THAT(result, ContainsSubstring("2h"));
    }
    SECTION("days") {
        auto result = fmt::format("{}", FormatDuration{duration_cast<Duration>(days{3})});
        CHECK_THAT(result, ContainsSubstring("3d"));
    }
    SECTION("combined duration") {
        auto d = duration_cast<Duration>(hours{25} + minutes{30} + seconds{15});
        auto result = fmt::format("{}", FormatDuration{d});
        CHECK_THAT(result, ContainsSubstring("1d"));
        CHECK_THAT(result, ContainsSubstring("1h"));
        CHECK_THAT(result, ContainsSubstring("30m"));
        CHECK_THAT(result, ContainsSubstring("15s"));
    }
}

// ============================================================================
// fmt::formatter<FmtSel>
// ============================================================================

TEST_CASE("FmtSel conditional formatter", "[fmt][fmtsel]") {
    SECTION("selects first when true") {
        CHECK(fmt::format("{}", FmtSel{true, "A", "B"}) == "A");
    }
    SECTION("selects second when false") {
        CHECK(fmt::format("{}", FmtSel{false, "A", "B"}) == "B");
    }
    SECTION("works with string_view") {
        using namespace std::string_view_literals;
        CHECK(fmt::format("{}", FmtSel{true, "yes"sv, "no"sv}) == "yes");
    }
}
