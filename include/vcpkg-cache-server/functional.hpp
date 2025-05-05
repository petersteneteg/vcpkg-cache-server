#pragma once

#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <ranges>
#include <optional>
#include <functional>
#include <algorithm>
#include <exception>
#include <charconv>
#include <chrono>
#include <filesystem>

#include <fmt/format.h>
#include <fmt/chrono.h>

#include <yaml-cpp/yaml.h>

namespace vcache {

using Time = std::filesystem::file_time_type;
using Duration = Time::duration;
using Clock = Time::clock;
using Rep = Time::rep;

namespace fp {

namespace detail {

template <class Default, class AlwaysVoid, template <class...> class Op, class... Args>
struct detector {
    using value_t = std::false_type;
    using type = Default;
};

template <class Default, template <class...> class Op, class... Args>
struct detector<Default, std::void_t<Op<Args...>>, Op, Args...> {
    using value_t = std::true_type;
    using type = Op<Args...>;
};

}  // namespace detail

constexpr std::pair<std::string_view, std::string_view> splitByFirst(std::string_view str,
                                                                     char delimiter = ' ') {
    const auto pos = str.find(delimiter);
    return {str.substr(0, pos), pos != str.npos ? str.substr(pos + 1) : std::string_view{}};
}
constexpr std::string_view trim(std::string_view str) noexcept {
    const auto idx1 = str.find_first_not_of(" \f\n\r\t\v");
    if (idx1 == std::string_view::npos) return "";
    const auto idx2 = str.find_last_not_of(" \f\n\r\t\v");
    return str.substr(idx1, idx2 + 1 - idx1);
}
constexpr std::string_view remove_suffix(std::string_view str, size_t n) noexcept {
    return str.substr(0, str.size() - n);
}

template <typename Map, typename Key>
constexpr std::optional<typename Map::mapped_type> mGet(const Map& map, const Key& key) {
    if (auto it = map.find(key); it != map.end()) {
        return {it->second};
    } else {
        return std::nullopt;
    }
}

struct StringHash {
    using hash_type = std::hash<std::string_view>;
    using is_transparent = void;

    std::size_t operator()(const char* str) const { return hash_type{}(str); }
    std::size_t operator()(std::string_view str) const { return hash_type{}(str); }
    std::size_t operator()(const std::string& str) const { return hash_type{}(str); }
};

template <typename V>
using UnorderedStringMap = std::unordered_map<std::string, V, StringHash, std::equal_to<>>;
using UnorderedStringSet = std::unordered_set<std::string, StringHash, std::equal_to<>>;

constexpr auto isSpace = [](char c) { return std::isspace(c); };

constexpr auto nonSpace = [](auto&& line) { return !std::ranges::all_of(line, isSpace); };

constexpr auto endsWith(std::string_view suffix) {
    return [s = std::string(suffix)](const auto& str) { return str.ends_with(s); };
}

constexpr auto toPair(char sep) {
    return [sep](auto&& line) {
        auto [key, value] = splitByFirst(std::string_view(line), sep);
        return std::pair<const std::string, std::string>(trim(key), trim(value));
    };
}

constexpr auto splitIntoPairs(char sep1, char sep2) {
    return std::views::split(sep1) | std::views::filter(nonSpace) |
           std::views::transform(toPair(sep2));
}

constexpr auto isZipFile = [](const auto& entry) { return entry.path().extension() == ".zip"; };

template <typename F, typename E>
constexpr auto tryTransform(F&& func, E&& error) {
    return std::views::transform(
               [f = std::forward<F>(func), e = std::forward<E>(error)](
                   const auto& item) -> std::optional<std::invoke_result_t<F, decltype(item)>> {
                   try {
                       return std::invoke(f, item);
                   } catch (...) {

                       std::invoke(e, item);
                       return std::nullopt;
                   }
               }) |
           std::views::filter([](const auto& item) { return item.has_value(); }) |
           std::views::transform([](auto&& optional) { return *optional; });
}

inline std::string exceptionToString() {
    try {
        throw;
    } catch (const std::exception& e) {
        return e.what();
    } catch (...) {
        return "Unknown error";
    }
}
inline std::string exceptionToString(std::exception_ptr ep) {
    try {
        std::rethrow_exception(ep);
    } catch (const std::exception& e) {
        return e.what();
    } catch (...) {
        return "Unknown error";
    }
}

template <typename T>
constexpr bool alwaysFalse() {
    return false;
}

struct nonesuch {
    ~nonesuch() = delete;
    nonesuch(const nonesuch&) = delete;
    void operator=(const nonesuch&) = delete;
};

template <template <class...> class Op, class... Args>
using is_detected = typename detail::detector<nonesuch, void, Op, Args...>::value_t;

template <template <class...> class Op, class... Args>
using detected_t = typename detail::detector<nonesuch, void, Op, Args...>::type;

template <class Default, template <class...> class Op, class... Args>
using detected_or = detail::detector<Default, void, Op, Args...>;

template <template <class...> class Op, class... Args>
constexpr bool is_detected_v = is_detected<Op, Args...>::value;

template <class Default, template <class...> class Op, class... Args>
using detected_or_t = typename detected_or<Default, Op, Args...>::type;

template <class Expected, template <class...> class Op, class... Args>
using is_detected_exact = std::is_same<Expected, detected_t<Op, Args...>>;

template <class Expected, template <class...> class Op, class... Args>
constexpr bool is_detected_exact_v = is_detected_exact<Expected, Op, Args...>::value;

template <class To, template <class...> class Op, class... Args>
using is_detected_convertible = std::is_convertible<detected_t<Op, Args...>, To>;

template <class To, template <class...> class Op, class... Args>
constexpr bool is_detected_convertible_v = is_detected_convertible<To, Op, Args...>::value;

// GCC 14 does not support ranges::to
template <typename T, typename R>
auto fromRange(R&& range) {
    return T(std::begin(range), std::end(range));
}

}  // namespace fp

enum struct ByteSize : size_t {};

struct FormatDuration : Duration {
    FormatDuration(Duration d) : Duration{d} {};
    using Duration::Duration;
};

template <typename T>
struct FlagFormatter : fmt::formatter<std::string_view> {
    template <typename U>
    using HasEnumToStr = decltype(enumToStr(std::declval<U>()));

    template <typename FormatContext>
    auto format(T val, FormatContext& ctx) const {
        if constexpr (fp::is_detected_exact_v<std::string_view, HasEnumToStr, T>) {
            return fmt::formatter<std::string_view>::format(enumToStr(val), ctx);
        } else {
            static_assert(fp::alwaysFalse<T>(),
                          "Missing enumToStr(T val) overload for type T "
                          "FlagFormatter requires that a std::string_view enumToStr(T val) "
                          "overload exists in the namespace of T");
        }
    }
};

template <typename T>
struct enumTo {};

template <typename T>
struct FmtSel {
    bool sel;
    T a;
    T b;
};
template <typename T>
FmtSel(bool, T, T) -> FmtSel<T>;

}  // namespace vcache

namespace YAML {
template <>
struct convert<vcache::ByteSize> {
    static Node encode(const vcache::ByteSize& size) { return Node{std::to_underlying(size)}; }

    static bool decode(const Node& node, vcache::ByteSize& byteSize) {
        if (!node.IsScalar()) {
            return false;
        }

        const auto& val = node.Scalar();

        const auto tval = vcache::fp::trim(val);
        using vcache::fp::remove_suffix;
        auto [str, factor] = [&]() -> std::pair<std::string_view, size_t> {
            if (tval.ends_with("TB")) {
                return {remove_suffix(tval, 2), 1'000'000'000'000};
            } else if (tval.ends_with("GB")) {
                return {remove_suffix(tval, 2), 1'000'000'000};
            } else if (tval.ends_with("MB")) {
                return {remove_suffix(tval, 2), 1'000'000};
            } else if (tval.ends_with("kB")) {
                return {remove_suffix(tval, 2), 1'000};
            } else {
                return {tval, 1};
            }
        }();

        const auto tstr = vcache::fp::trim(str);
        size_t size{0};
        if (std::from_chars(tstr.begin(), tstr.end(), size).ec != std::errc{}) {
            return false;
        }
        byteSize = vcache::ByteSize{size * factor};

        return true;
    }
};

template <class Rep, class Period>
struct convert<std::chrono::duration<Rep, Period>> {
    static Node encode(std::chrono::duration<Rep, Period> d) {

        using namespace std::chrono_literals;
        using std::chrono::duration_cast;
        namespace c = std::chrono;

        const auto years = duration_cast<c::years>(d);
        d -= years;
        const auto days = duration_cast<c::days>(d);
        d -= days;
        const auto hours = duration_cast<c::hours>(d);
        d -= hours;
        const auto minutes = duration_cast<c::minutes>(d);
        d -= minutes;
        const auto seconds = duration_cast<c::seconds>(d);

        std::string res;
        if (years != c::years{}) {
            fmt::format_to(std::back_inserter(res), "{:%Q}y ", years);
        }
        if (days != c::days{}) {
            fmt::format_to(std::back_inserter(res), "{:%Q}d ", days);
        }
        if (hours != c::hours{}) {
            fmt::format_to(std::back_inserter(res), "{:%Q}h ", hours);
        }
        if (minutes != c::minutes{}) {
            fmt::format_to(std::back_inserter(res), "{:%Q}m ", minutes);
        }
        if (seconds != c::seconds{}) {
            fmt::format_to(std::back_inserter(res), "{:%Q}s ", seconds);
        }

        return Node{res};
    }

    static bool decode(const Node& node, std::chrono::duration<Rep, Period>& duration) {
        namespace c = std::chrono;

        if (!node.IsScalar()) {
            return false;
        }

        const auto& val = node.Scalar();

        std::string_view rest = val;
        std::string_view current;

        c::seconds res{};
        while (!rest.empty()) {
            std::tie(current, rest) = vcache::fp::splitByFirst(val);
            const auto tval = vcache::fp::trim(current);
            using vcache::fp::remove_suffix;
            auto [str, factor] = [&]() -> std::pair<std::string_view, std::chrono::seconds> {
                if (tval.ends_with("y")) {
                    return {remove_suffix(tval, 1), c::years{1}};
                } else if (tval.ends_with("d")) {
                    return {remove_suffix(tval, 1), c::days{1}};
                } else if (tval.ends_with("h")) {
                    return {remove_suffix(tval, 1), c::hours{1}};
                } else if (tval.ends_with("m")) {
                    return {remove_suffix(tval, 1), c::minutes{1}};
                } else if (tval.ends_with("s")) {
                    return {remove_suffix(tval, 1), c::seconds{1}};
                } else {
                    return {tval, c::seconds{1}};
                }
            }();
            const auto tstr = vcache::fp::trim(str);
            size_t count{0};
            if (std::from_chars(tstr.begin(), tstr.end(), count).ec != std::errc{}) {
                return false;
            }
            res += count * factor;
        }

        duration = res;
        return true;
    }
};

}  // namespace YAML

template <>
struct fmt::formatter<vcache::ByteSize, char> {
    fmt::formatter<std::string_view> strFormatter;
    char prefix = 'A';

    template <class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext& ctx) {
        auto it = ctx.begin();
        if (it == ctx.end()) return it;

        if (*it == 'T' || *it == 'G' || *it == 'M' || *it == 'k' || *it == 'O') {
            prefix = *it;
            ++it;
        }
        if (*it == '}') {
            return it;
        }

        return strFormatter.parse(ctx);
    }

    template <class FmtContext>
    FmtContext::iterator format(vcache::ByteSize size, FmtContext& ctx) const {
        std::array<char, 20> buff;

        auto end = buff.data();
        if (prefix == 'T' || (prefix == 'A' && std::to_underlying(size) >= 1'000'000'000'000)) {
            end = fmt::format_to_n(buff.data(), buff.size(), "{:.2f} TB",
                                   static_cast<double>(size) / 1'000'000'000'000.0)
                      .out;
        } else if (prefix == 'G' || (prefix == 'A' && std::to_underlying(size) >= 1'000'000'000)) {
            end = fmt::format_to_n(buff.data(), buff.size(), "{:.2f} GB",
                                   static_cast<double>(size) / 1'000'000'000.0)
                      .out;
        } else if (prefix == 'M' || (prefix == 'A' && std::to_underlying(size) >= 1'000'000)) {
            end = fmt::format_to_n(buff.data(), buff.size(), "{:.2f} MB",
                                   static_cast<double>(size) / 1'000'000.0)
                      .out;
        } else if (prefix == 'k' || (prefix == 'A' && std::to_underlying(size) >= 1'000)) {
            end = fmt::format_to_n(buff.data(), buff.size(), "{:.2f} kB",
                                   static_cast<double>(size) / 1'000.0)
                      .out;
        } else {
            end =
                fmt::format_to_n(buff.data(), buff.size(), "{:d} B", std::to_underlying(size)).out;
        }

        return strFormatter.format(
            std::string_view{buff.data(), static_cast<size_t>(std::distance(buff.data(), end))},
            ctx);
    }
};

template <>
struct fmt::formatter<vcache::Time, char> {
    fmt::formatter<std::chrono::sys_time<vcache::Duration>> formatter;

    template <class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext& ctx) {
        return formatter.parse(ctx);
    }

    template <class FmtContext>
    constexpr FmtContext::iterator format(vcache::Time time, FmtContext& ctx) const {
        if (time.time_since_epoch().count() == -1) {
            auto it = ctx.out();
            *it++ = '-';
            return it;
        }
        return formatter.format(vcache::Clock::to_sys(time), ctx);
    }
};

template <>
struct fmt::formatter<vcache::FormatDuration> {
    template <class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw format_error("Invalid unit format found");
        }
        return it;
    }

    template <class FmtContext>
    constexpr FmtContext::iterator format(vcache::FormatDuration d, FmtContext& ctx) const {

        using namespace std::chrono_literals;
        using std::chrono::duration_cast;
        namespace c = std::chrono;

        const auto years = duration_cast<c::years>(d);
        d -= years;
        const auto days = duration_cast<c::days>(d);
        d -= days;
        const auto hours = duration_cast<c::hours>(d);
        d -= hours;
        const auto minutes = duration_cast<c::minutes>(d);
        d -= minutes;
        const auto seconds = duration_cast<c::seconds>(d);

        auto it = ctx.out();
        if (years != c::years{}) {
            fmt::format_to(it, "{:%Q}y ", years);
        }
        if (days != c::days{}) {
            fmt::format_to(it, "{:%Q}d ", days);
        }
        if (hours != c::hours{}) {
            fmt::format_to(it, "{:%Q}h ", hours);
        }
        if (minutes != c::minutes{}) {
            fmt::format_to(it, "{:%Q}m ", minutes);
        }
        if (seconds != c::seconds{}) {
            fmt::format_to(it, "{:%Q}s ", seconds);
        }

        return it;
    }
};

template <typename T>
struct fmt::formatter<vcache::FmtSel<T>> {
    fmt::formatter<T> formatter;

    template <class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext& ctx) {
        return formatter.parse(ctx);
    }

    template <class FmtContext>
    constexpr FmtContext::iterator format(const vcache::FmtSel<T>& opt, FmtContext& ctx) const {
        if (opt.sel) {
            return formatter.format(opt.a, ctx);
        } else {
            return formatter.format(opt.b, ctx);
        }
    }
};
