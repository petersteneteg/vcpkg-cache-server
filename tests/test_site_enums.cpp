#include <catch2/catch_test_macros.hpp>

#include <vcpkg-cache-server/site.hpp>

#include <fmt/format.h>
#include <string>

using namespace vcache;
using namespace vcache::site;

// ============================================================================
// enumToStr(Mode)
// ============================================================================

TEST_CASE("enumToStr(Mode) returns correct strings", "[site][enum]") {
    CHECK(enumToStr(Mode::Full) == "full");
    CHECK(enumToStr(Mode::Plain) == "plain");
    CHECK(enumToStr(Mode::Append) == "append");
}

// ============================================================================
// enumToStr(Sort)
// ============================================================================

TEST_CASE("enumToStr(Sort) returns correct strings", "[site][enum]") {
    CHECK(enumToStr(Sort::Default) == "default");
    CHECK(enumToStr(Sort::Name) == "name");
    CHECK(enumToStr(Sort::Count) == "count");
    CHECK(enumToStr(Sort::Size) == "size");
    CHECK(enumToStr(Sort::First) == "first");
    CHECK(enumToStr(Sort::Last) == "last");
    CHECK(enumToStr(Sort::Downloads) == "download");
    CHECK(enumToStr(Sort::Use) == "use");
    CHECK(enumToStr(Sort::Version) == "version");
    CHECK(enumToStr(Sort::Arch) == "arch");
    CHECK(enumToStr(Sort::SHA) == "sha");
    CHECK(enumToStr(Sort::Time) == "time");
    CHECK(enumToStr(Sort::Ip) == "pp");
    CHECK(enumToStr(Sort::User) == "user");
    CHECK(enumToStr(Sort::Age) == "age");
}

// ============================================================================
// enumToStr(Order)
// ============================================================================

TEST_CASE("enumToStr(Order) returns correct strings", "[site][enum]") {
    CHECK(enumToStr(Order::Descending) == "descending");
    CHECK(enumToStr(Order::Ascending) == "ascending");
}

// ============================================================================
// enumTo<Mode> - string to Mode
// ============================================================================

TEST_CASE("enumTo<Mode> parses strings to Mode", "[site][enum]") {
    SECTION("valid values") {
        CHECK(enumTo<Mode>{}("full") == Mode::Full);
        CHECK(enumTo<Mode>{}("plain") == Mode::Plain);
        CHECK(enumTo<Mode>{}("append") == Mode::Append);
    }
    SECTION("invalid value returns nullopt") {
        CHECK_FALSE(enumTo<Mode>{}("invalid").has_value());
        CHECK_FALSE(enumTo<Mode>{}("").has_value());
        CHECK_FALSE(enumTo<Mode>{}("Full").has_value());
    }
}

// ============================================================================
// enumTo<Sort> - string to Sort
// ============================================================================

TEST_CASE("enumTo<Sort> parses strings to Sort", "[site][enum]") {
    SECTION("valid values") {
        CHECK(enumTo<Sort>{}("default") == Sort::Default);
        CHECK(enumTo<Sort>{}("name") == Sort::Name);
        CHECK(enumTo<Sort>{}("count") == Sort::Count);
        CHECK(enumTo<Sort>{}("size") == Sort::Size);
        CHECK(enumTo<Sort>{}("first") == Sort::First);
        CHECK(enumTo<Sort>{}("last") == Sort::Last);
        CHECK(enumTo<Sort>{}("download") == Sort::Downloads);
        CHECK(enumTo<Sort>{}("use") == Sort::Use);
        CHECK(enumTo<Sort>{}("version") == Sort::Version);
        CHECK(enumTo<Sort>{}("arch") == Sort::Arch);
        CHECK(enumTo<Sort>{}("sha") == Sort::SHA);
        CHECK(enumTo<Sort>{}("time") == Sort::Time);
        CHECK(enumTo<Sort>{}("pp") == Sort::Ip);
        CHECK(enumTo<Sort>{}("user") == Sort::User);
        CHECK(enumTo<Sort>{}("age") == Sort::Age);
    }
    SECTION("invalid values return nullopt") {
        CHECK_FALSE(enumTo<Sort>{}("invalid").has_value());
        CHECK_FALSE(enumTo<Sort>{}("").has_value());
        CHECK_FALSE(enumTo<Sort>{}("Name").has_value());
    }
}

// ============================================================================
// enumTo<Order> - string to Order
// ============================================================================

TEST_CASE("enumTo<Order> parses strings to Order", "[site][enum]") {
    SECTION("valid values") {
        CHECK(enumTo<Order>{}("ascending") == Order::Ascending);
        CHECK(enumTo<Order>{}("descending") == Order::Descending);
    }
    SECTION("invalid values return nullopt") {
        CHECK_FALSE(enumTo<Order>{}("invalid").has_value());
        CHECK_FALSE(enumTo<Order>{}("").has_value());
        CHECK_FALSE(enumTo<Order>{}("Ascending").has_value());
    }
}

// ============================================================================
// enumToStr/enumTo roundtrip
// ============================================================================

TEST_CASE("Mode enum roundtrips through string", "[site][enum]") {
    for (auto mode : {Mode::Full, Mode::Plain, Mode::Append}) {
        auto str = enumToStr(mode);
        auto parsed = enumTo<Mode>{}(str);
        REQUIRE(parsed.has_value());
        CHECK(*parsed == mode);
    }
}

TEST_CASE("Sort enum roundtrips through string", "[site][enum]") {
    for (auto sort : {Sort::Default, Sort::Name, Sort::Count, Sort::Size, Sort::First, Sort::Last,
                      Sort::Downloads, Sort::Use, Sort::Version, Sort::Arch, Sort::SHA, Sort::Time,
                      Sort::Ip, Sort::User, Sort::Age}) {
        auto str = enumToStr(sort);
        auto parsed = enumTo<Sort>{}(str);
        REQUIRE(parsed.has_value());
        CHECK(*parsed == sort);
    }
}

TEST_CASE("Order enum roundtrips through string", "[site][enum]") {
    for (auto order : {Order::Ascending, Order::Descending}) {
        auto str = enumToStr(order);
        auto parsed = enumTo<Order>{}(str);
        REQUIRE(parsed.has_value());
        CHECK(*parsed == order);
    }
}

// ============================================================================
// fmt::formatter for enums (via FlagFormatter)
// ============================================================================

TEST_CASE("fmt::format works with Mode enum", "[site][fmt]") {
    CHECK(fmt::format("{}", Mode::Full) == "full");
    CHECK(fmt::format("{}", Mode::Plain) == "plain");
    CHECK(fmt::format("{}", Mode::Append) == "append");
}

TEST_CASE("fmt::format works with Sort enum", "[site][fmt]") {
    CHECK(fmt::format("{}", Sort::Name) == "name");
    CHECK(fmt::format("{}", Sort::Size) == "size");
    CHECK(fmt::format("{}", Sort::Downloads) == "download");
}

TEST_CASE("fmt::format works with Order enum", "[site][fmt]") {
    CHECK(fmt::format("{}", Order::Descending) == "descending");
    CHECK(fmt::format("{}", Order::Ascending) == "ascending");
}

// ============================================================================
// fmt::formatter<Url>
// ============================================================================

TEST_CASE("Url formatter produces correct URLs", "[site][fmt]") {
    SECTION("path only") {
        Url url{"/test", {}};
        CHECK(fmt::format("{}", url) == "/test");
    }
    SECTION("path with single parameter") {
        Url url{"/test", {{"key", "value"}}};
        CHECK(fmt::format("{}", url) == "/test?key=value");
    }
    SECTION("path with multiple parameters") {
        Url url{"/test", {{"a", "1"}, {"b", "2"}}};
        CHECK(fmt::format("{}", url) == "/test?a=1&b=2");
    }
    SECTION("empty path with parameters") {
        Url url{"", {{"sort", "name"}}};
        CHECK(fmt::format("{}", url) == "?sort=name");
    }
}
