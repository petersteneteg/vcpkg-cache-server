#include <catch2/catch_test_macros.hpp>

#include <vcpkg-cache-server/site.hpp>

#include <fmt/format.h>
#include <string>

using namespace vcache;
using namespace vcache::site;

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

// ============================================================================
// detail::missmatches
// ============================================================================

TEST_CASE("detail::missmatches counts map differences", "[site][detail]") {
    SECTION("identical maps have zero mismatches") {
        std::map<std::string, std::string> a{{"k1", "v1"}, {"k2", "v2"}};
        std::map<std::string, std::string> b{{"k1", "v1"}, {"k2", "v2"}};
        CHECK(detail::missmatches(a, b) == 0);
    }
    SECTION("completely different values") {
        std::map<std::string, std::string> a{{"k1", "a"}, {"k2", "b"}};
        std::map<std::string, std::string> b{{"k1", "x"}, {"k2", "y"}};
        CHECK(detail::missmatches(a, b) == 2);
    }
    SECTION("partially different values") {
        std::map<std::string, std::string> a{{"k1", "same"}, {"k2", "diff"}};
        std::map<std::string, std::string> b{{"k1", "same"}, {"k2", "other"}};
        CHECK(detail::missmatches(a, b) == 1);
    }
    SECTION("keys missing in one map count as mismatches") {
        std::map<std::string, std::string> a{{"k1", "v1"}, {"k2", "v2"}};
        std::map<std::string, std::string> b{{"k1", "v1"}};
        CHECK(detail::missmatches(a, b) == 1);
    }
    SECTION("extra keys in second map count as mismatches") {
        std::map<std::string, std::string> a{{"k1", "v1"}};
        std::map<std::string, std::string> b{{"k1", "v1"}, {"k2", "v2"}};
        CHECK(detail::missmatches(a, b) == 1);
    }
    SECTION("both empty maps have zero mismatches") {
        std::map<std::string, std::string> a;
        std::map<std::string, std::string> b;
        CHECK(detail::missmatches(a, b) == 0);
    }
    SECTION("disjoint keys count all as mismatches") {
        std::map<std::string, std::string> a{{"k1", "v1"}};
        std::map<std::string, std::string> b{{"k2", "v2"}};
        CHECK(detail::missmatches(a, b) == 2);
    }
}

// ============================================================================
// detail::formatDiff
// ============================================================================

TEST_CASE("detail::formatDiff generates HTML diff", "[site][detail]") {
    SECTION("identical maps produce only dl tags") {
        std::map<std::string, std::string> a{{"k1", "v1"}};
        auto result = detail::formatDiff(a, a);
        CHECK(result == "<dl></dl>");
    }
    SECTION("different values show both") {
        std::map<std::string, std::string> a{{"k1", "valA"}};
        std::map<std::string, std::string> b{{"k1", "valB"}};
        auto result = detail::formatDiff(a, b);
        CHECK(result.find("valA") != std::string::npos);
        CHECK(result.find("valB") != std::string::npos);
    }
    SECTION("missing in source is reported") {
        std::map<std::string, std::string> a{{"k1", "v1"}};
        std::map<std::string, std::string> b;
        auto result = detail::formatDiff(a, b);
        CHECK(result.find("Missing in source") != std::string::npos);
    }
    SECTION("missing in target is reported") {
        std::map<std::string, std::string> a;
        std::map<std::string, std::string> b{{"k1", "v1"}};
        auto result = detail::formatDiff(a, b);
        CHECK(result.find("Missing in target") != std::string::npos);
    }
}

// ============================================================================
// detail::formatMap
// ============================================================================

TEST_CASE("detail::formatMap generates HTML definition list", "[site][detail]") {
    SECTION("empty map produces empty dl tags") {
        std::map<std::string, std::string> m;
        auto result = detail::formatMap(m);
        CHECK(result.find("<dl>") != std::string::npos);
        CHECK(result.find("</dl>") != std::string::npos);
    }
    SECTION("entries produce dt/dd pairs") {
        std::map<std::string, std::string> m{{"key1", "val1"}, {"key2", "val2"}};
        auto result = detail::formatMap(m);
        CHECK(result.find("<dt>key1</dt>") != std::string::npos);
        CHECK(result.find("<dd>val1</dd>") != std::string::npos);
        CHECK(result.find("<dt>key2</dt>") != std::string::npos);
        CHECK(result.find("<dd>val2</dd>") != std::string::npos);
    }
}

// ============================================================================
// detail::link
// ============================================================================

TEST_CASE("detail::link creates HTMX link", "[site][detail]") {
    auto result = detail::link("/some/path", "Click Me");
    CHECK(result.find("/some/path") != std::string::npos);
    CHECK(result.find("Click Me") != std::string::npos);
    CHECK(result.find("hx-get") != std::string::npos);
}

// ============================================================================
// detail::button
// ============================================================================

TEST_CASE("detail::button creates sortable column header", "[site][detail]") {
    SECTION("shows up arrow when sorted ascending") {
        auto result = detail::button("/", "Name", Sort::Name, Sort::Name, Order::Ascending);
        CHECK(result.find("Name") != std::string::npos);
        CHECK(result.find("&#8593") != std::string::npos);  // up arrow
    }
    SECTION("shows down arrow when sorted descending") {
        auto result = detail::button("/", "Name", Sort::Name, Sort::Name, Order::Descending);
        CHECK(result.find("&#8595") != std::string::npos);  // down arrow
    }
    SECTION("shows no arrow when different column sorted") {
        auto result = detail::button("/", "Name", Sort::Name, Sort::Size, Order::Ascending);
        CHECK(result.find("&#8593") == std::string::npos);
        CHECK(result.find("&#8595") == std::string::npos);
    }
}

// ============================================================================
// detail::navItem
// ============================================================================

TEST_CASE("detail::navItem creates breadcrumb items", "[site][detail]") {
    SECTION("active item is static text") {
        auto result = detail::navItem("Home", "/", true);
        CHECK(result.find("active") != std::string::npos);
        CHECK(result.find("Home") != std::string::npos);
        CHECK(result.find("hx-get") == std::string::npos);
    }
    SECTION("inactive item is a link") {
        auto result = detail::navItem("Home", "/", false);
        CHECK(result.find("Home") != std::string::npos);
        CHECK(result.find("hx-get") != std::string::npos);
    }
}
