#pragma once

#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <ranges>
#include <optional>
#include <functional>
#include <algorithm>
#include <format>
#include <exception>

namespace vcache {

namespace fp {

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

}  // namespace fp

enum struct ByteSize : size_t {};

}  // namespace vcache

template <>
struct std::formatter<vcache::ByteSize, char> {
    std::formatter<std::string_view> formatter;

    template <class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext& ctx) {
        auto it = ctx.begin();
        if (*it == '}') {
            return it;
        }
        return formatter.parse(ctx);
    }

    template <class FmtContext>
    FmtContext::iterator format(vcache::ByteSize size, FmtContext& ctx) const {
        std::array<char, 20> buff;

        auto end = buff.data();
        if (std::to_underlying(size) >= 1'000'000'000'000) {
            end = std::format_to_n(buff.data(), buff.size(), "{:5.1f} TB",
                                   static_cast<double>(size) / 1'000'000'000'000.0)
                      .out;
        } else if (std::to_underlying(size) >= 1'000'000'000) {
            end = std::format_to_n(buff.data(), buff.size(), "{:5.1f} GB",
                                   static_cast<double>(size) / 1'000'000'000.0)
                      .out;
        } else if (std::to_underlying(size) >= 1'000'000) {
            end = std::format_to_n(buff.data(), buff.size(), "{:5.1f} MB",
                                   static_cast<double>(size) / 1'000'000.0)
                      .out;
        } else if (std::to_underlying(size) >= 1'000) {
            end = std::format_to_n(buff.data(), buff.size(), "{:5.1f} kB",
                                   static_cast<double>(size) / 1'000.0)
                      .out;
        } else {
            end =
                std::format_to_n(buff.data(), buff.size(), "{:4d} B", std::to_underlying(size)).out;
        }

        return formatter.format(
            std::string_view{buff.data(), static_cast<size_t>(std::distance(buff.data(), end))},
            ctx);
    }
};
