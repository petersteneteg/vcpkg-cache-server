#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vcpkg-cache-server/functional.hpp>

#include <map>
#include <string>
#include <vector>

using namespace vcache;
using namespace vcache::fp;

// ============================================================================
// splitByFirst
// ============================================================================

TEST_CASE("splitByFirst splits on first occurrence of delimiter", "[functional]") {
    SECTION("basic split on space") {
        auto [first, second] = splitByFirst("hello world");
        CHECK(first == "hello");
        CHECK(second == "world");
    }
    SECTION("split with multiple spaces") {
        auto [first, second] = splitByFirst("hello world again");
        CHECK(first == "hello");
        CHECK(second == "world again");
    }
    SECTION("no delimiter found") {
        auto [first, second] = splitByFirst("hello");
        CHECK(first == "hello");
        CHECK(second == "");
    }
    SECTION("delimiter at start") {
        auto [first, second] = splitByFirst(" hello");
        CHECK(first == "");
        CHECK(second == "hello");
    }
    SECTION("delimiter at end") {
        auto [first, second] = splitByFirst("hello ");
        CHECK(first == "hello");
        CHECK(second == "");
    }
    SECTION("empty string") {
        auto [first, second] = splitByFirst("");
        CHECK(first == "");
        CHECK(second == "");
    }
    SECTION("custom delimiter") {
        auto [first, second] = splitByFirst("key:value", ':');
        CHECK(first == "key");
        CHECK(second == "value");
    }
    SECTION("custom delimiter with multiple occurrences") {
        auto [first, second] = splitByFirst("a:b:c", ':');
        CHECK(first == "a");
        CHECK(second == "b:c");
    }
    SECTION("only delimiter") {
        // This should be empty first, empty second when delimiter is ':'
        // Actually: first = "", second = "" (substr(0,0) = "" and substr(1) = "")
        auto [f, s] = splitByFirst(":", ':');
        CHECK(f == "");
        CHECK(s == "");
    }
}

// ============================================================================
// trim
// ============================================================================

TEST_CASE("trim removes leading and trailing whitespace", "[functional]") {
    SECTION("basic trim") {
        CHECK(trim("  hello  ") == "hello");
    }
    SECTION("no whitespace") {
        CHECK(trim("hello") == "hello");
    }
    SECTION("only leading whitespace") {
        CHECK(trim("  hello") == "hello");
    }
    SECTION("only trailing whitespace") {
        CHECK(trim("hello  ") == "hello");
    }
    SECTION("all whitespace types") {
        CHECK(trim(" \t\n\r\f\vhello\t\n ") == "hello");
    }
    SECTION("empty string") {
        CHECK(trim("") == "");
    }
    SECTION("only whitespace") {
        CHECK(trim("   ") == "");
    }
    SECTION("single character") {
        CHECK(trim("x") == "x");
    }
    SECTION("preserves internal whitespace") {
        CHECK(trim("  hello world  ") == "hello world");
    }
}

// ============================================================================
// remove_suffix
// ============================================================================

TEST_CASE("remove_suffix removes n characters from end", "[functional]") {
    SECTION("basic removal") {
        CHECK(remove_suffix("hello", 2) == "hel");
    }
    SECTION("remove nothing") {
        CHECK(remove_suffix("hello", 0) == "hello");
    }
    SECTION("remove all") {
        CHECK(remove_suffix("hello", 5) == "");
    }
    SECTION("remove one character") {
        CHECK(remove_suffix("hello", 1) == "hell");
    }
}

// ============================================================================
// mGet - safe map lookup
// ============================================================================

TEST_CASE("mGet safely looks up map entries", "[functional]") {
    std::map<std::string, std::string> m{{"key1", "value1"}, {"key2", "value2"}};

    SECTION("existing key returns value") {
        auto result = mGet(m, std::string{"key1"});
        REQUIRE(result.has_value());
        CHECK(*result == "value1");
    }
    SECTION("missing key returns nullopt") {
        auto result = mGet(m, std::string{"key3"});
        CHECK_FALSE(result.has_value());
    }
    SECTION("empty map returns nullopt") {
        std::map<std::string, std::string> empty;
        auto result = mGet(empty, std::string{"key1"});
        CHECK_FALSE(result.has_value());
    }
}

TEST_CASE("mGet works with UnorderedStringMap", "[functional]") {
    UnorderedStringMap<int> m{{"apple", 1}, {"banana", 2}};

    SECTION("lookup by string_view") {
        auto result = mGet(m, std::string_view{"apple"});
        REQUIRE(result.has_value());
        CHECK(*result == 1);
    }
    SECTION("missing key") {
        auto result = mGet(m, std::string_view{"cherry"});
        CHECK_FALSE(result.has_value());
    }
}

// ============================================================================
// StringHash - transparent hashing
// ============================================================================

TEST_CASE("StringHash produces consistent hashes", "[functional]") {
    StringHash hasher;

    SECTION("same string, different types produce same hash") {
        std::string str = "hello";
        std::string_view sv = "hello";
        const char* cs = "hello";

        CHECK(hasher(str) == hasher(sv));
        CHECK(hasher(str) == hasher(cs));
        CHECK(hasher(sv) == hasher(cs));
    }
    SECTION("different strings produce different hashes") {
        CHECK(hasher("hello") != hasher("world"));
    }
}

// ============================================================================
// UnorderedStringMap - heterogeneous lookup
// ============================================================================

TEST_CASE("UnorderedStringMap supports heterogeneous lookup", "[functional]") {
    UnorderedStringMap<int> m{{"one", 1}, {"two", 2}, {"three", 3}};

    SECTION("find with string_view") {
        auto it = m.find(std::string_view{"two"});
        REQUIRE(it != m.end());
        CHECK(it->second == 2);
    }
    SECTION("find with const char*") {
        auto it = m.find("three");
        REQUIRE(it != m.end());
        CHECK(it->second == 3);
    }
    SECTION("contains with string_view") {
        CHECK(m.contains(std::string_view{"one"}));
        CHECK_FALSE(m.contains(std::string_view{"four"}));
    }
}

// ============================================================================
// endsWith predicate
// ============================================================================

TEST_CASE("endsWith creates a suffix-checking predicate", "[functional]") {
    auto endsWithTxt = endsWith(".txt");
    auto endsWithZip = endsWith(".zip");

    SECTION("matching suffix") {
        CHECK(endsWithTxt(std::string{"file.txt"}));
        CHECK(endsWithZip(std::string{"archive.zip"}));
    }
    SECTION("non-matching suffix") {
        CHECK_FALSE(endsWithTxt(std::string{"file.cpp"}));
        CHECK_FALSE(endsWithZip(std::string{"file.txt"}));
    }
    SECTION("empty string") {
        CHECK_FALSE(endsWithTxt(std::string{""}));
    }
    SECTION("suffix only") {
        CHECK(endsWithTxt(std::string{".txt"}));
    }
}

// ============================================================================
// toPair - string to key-value pair
// ============================================================================

TEST_CASE("toPair splits and trims into key-value pair", "[functional]") {
    auto colonPair = toPair(':');

    SECTION("basic key:value") {
        auto [key, value] = colonPair(std::string_view{"Package: my-package"});
        CHECK(key == "Package");
        CHECK(value == "my-package");
    }
    SECTION("no value") {
        auto [key, value] = colonPair(std::string_view{"Key:"});
        CHECK(key == "Key");
        CHECK(value == "");
    }
    SECTION("trims whitespace") {
        auto [key, value] = colonPair(std::string_view{"  Key  :  Value  "});
        CHECK(key == "Key");
        CHECK(value == "Value");
    }

    auto spacePair = toPair(' ');
    SECTION("space-separated pair") {
        auto [key, value] = spacePair(std::string_view{"sha abc123"});
        CHECK(key == "sha");
        CHECK(value == "abc123");
    }
}

// ============================================================================
// exceptionToString
// ============================================================================

TEST_CASE("exceptionToString converts exception_ptr to string", "[functional]") {
    SECTION("std::runtime_error") {
        auto ep = std::make_exception_ptr(std::runtime_error("test error"));
        CHECK(exceptionToString(ep) == "test error");
    }
    SECTION("std::invalid_argument") {
        auto ep = std::make_exception_ptr(std::invalid_argument("bad arg"));
        CHECK(exceptionToString(ep) == "bad arg");
    }
    SECTION("non-std exception") {
        auto ep = std::make_exception_ptr(42);
        CHECK(exceptionToString(ep) == "Unknown error");
    }
}

// ============================================================================
// FmtSel - conditional formatting
// ============================================================================

TEST_CASE("FmtSel formats conditionally", "[functional]") {
    SECTION("true selects first value") {
        auto result = fmt::format("{}", FmtSel{true, "yes", "no"});
        CHECK(result == "yes");
    }
    SECTION("false selects second value") {
        auto result = fmt::format("{}", FmtSel{false, "yes", "no"});
        CHECK(result == "no");
    }
}

// ============================================================================
// strToNum - numeric parsing
// ============================================================================

TEST_CASE("strToNum parses valid integers", "[functional]") {
    SECTION("basic integer") {
        auto result = strToNum<size_t>("42");
        REQUIRE(result.has_value());
        CHECK(*result == 42);
    }
    SECTION("zero") {
        auto result = strToNum<size_t>("0");
        REQUIRE(result.has_value());
        CHECK(*result == 0);
    }
    SECTION("large number") {
        auto result = strToNum<size_t>("1000000");
        REQUIRE(result.has_value());
        CHECK(*result == 1000000);
    }
}

TEST_CASE("strToNum parses valid doubles", "[functional]") {
    SECTION("basic double") {
        auto result = strToNum<double>("3.14");
        REQUIRE(result.has_value());
        CHECK(*result == Catch::Approx(3.14));
    }
    SECTION("negative double") {
        auto result = strToNum<double>("-2.5");
        REQUIRE(result.has_value());
        CHECK(*result == Catch::Approx(-2.5));
    }
}

TEST_CASE("strToNum returns nullopt for invalid input", "[functional]") {
    SECTION("empty string") {
        CHECK_FALSE(strToNum<size_t>("").has_value());
    }
    SECTION("non-numeric") {
        CHECK_FALSE(strToNum<size_t>("abc").has_value());
    }
    SECTION("trailing non-numeric") {
        CHECK_FALSE(strToNum<size_t>("42abc").has_value());
    }
    SECTION("leading non-numeric") {
        CHECK_FALSE(strToNum<size_t>("abc42").has_value());
    }
}

// ============================================================================
// parseAuthHeader - authorization header parsing
// ============================================================================

TEST_CASE("parseAuthHeader splits scheme and token", "[functional]") {
    SECTION("Bearer token") {
        auto [scheme, token] = parseAuthHeader("Bearer mytoken123");
        CHECK(scheme == "Bearer");
        CHECK(token == "mytoken123");
    }
    SECTION("Basic auth") {
        auto [scheme, token] = parseAuthHeader("Basic dXNlcjpwYXNz");
        CHECK(scheme == "Basic");
        CHECK(token == "dXNlcjpwYXNz");
    }
    SECTION("trims whitespace") {
        auto [scheme, token] = parseAuthHeader("  Bearer   mytoken  ");
        CHECK(scheme == "Bearer");
        CHECK(token == "mytoken");
    }
    SECTION("scheme only, no token") {
        auto [scheme, token] = parseAuthHeader("Bearer");
        CHECK(scheme == "Bearer");
        CHECK(token == "");
    }
    SECTION("empty header") {
        auto [scheme, token] = parseAuthHeader("");
        CHECK(scheme == "");
        CHECK(token == "");
    }
}
