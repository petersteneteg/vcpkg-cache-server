#pragma once

#include <vcpkg-cache-server/functional.hpp>
#include <vcpkg-cache-server/database.hpp>
#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include <utility>
#include <tuple>
#include <map>
#include <ranges>

#include <fmt/format.h>
#include <fmt/ranges.h>

namespace vcache {

class Store;

namespace site {

enum class Mode { Full, Plain, Append };
enum class Sort {
    Default,
    Name,
    Count,
    Size,
    First,
    Last,
    Downloads,
    Use,
    Version,
    Arch,
    SHA,
    Time,
    Ip,
    User,
    Age,
    NumSortMethods
};
enum class Order { Descending, Ascending };

struct Limit {
    std::optional<size_t> offset = std::nullopt;
    std::optional<size_t> limit = std::nullopt;
};

using Params = std::map<std::string, std::string>;
struct Url {
    std::string path;
    Params params;
};

std::string match();
std::string compare(std::string_view sha, const Store& store, Mode mode);
std::string match(std::string_view abi, std::string_view package, const Store& store);
std::string list(const Store& store);
std::string find(std::string_view package, const Store& store, db::Database& db, Mode mode,
                 Sort sort, std::optional<Order> order);
std::string sha(std::string_view package, const Store& store, Mode mode);
std::string favicon();
std::string maskicon();

std::optional<std::pair<std::string, std::string>> script(std::string_view name);

std::string downloads(db::Database& db, Mode mode, std::optional<size_t> sortIdx,
                      std::optional<Order> order, Limit limit,
                      std::optional<std::pair<Sort, std::string>> selection);

std::string index(const Store& store, db::Database& db, Mode mode, Sort sort,
                  std::optional<Order> order, std::string_view search);

namespace detail {

std::string deliver(std::string_view content, Mode mode);
std::string nav(const std::vector<std::pair<std::string, std::string>>& path = {});

}  // namespace detail

template <typename T>
auto setOrder(T item, Order order) {
    if (order == Order::Descending) {
        return item.desc();
    } else {
        return item.asc();
    }
}

constexpr std::string_view enumToStr(Mode mode) {
    switch (mode) {
        case Mode::Full:
            return "full";
        case Mode::Plain:
            return "plain";
        case Mode::Append:
            return "append";
        default:
            throw std::runtime_error("Invalid Mode enum");
    }
}

constexpr std::string_view enumToStr(Sort sort) {
    switch (sort) {
        using enum Sort;
        case Default:
            return "default";
        case Name:
            return "name";
        case Count:
            return "count";
        case Size:
            return "size";
        case First:
            return "first";
        case Last:
            return "last";
        case Downloads:
            return "download";
        case Use:
            return "use";
        case Version:
            return "version";
        case Arch:
            return "arch";
        case SHA:
            return "sha";
        case Time:
            return "time";
        case Ip:
            return "pp";
        case User:
            return "user";
        case Age:
            return "age";

        default:
            throw std::runtime_error("Invalid Sort enum");
    }
}
constexpr std::string_view enumToStr(Order mode) {
    switch (mode) {
        case Order::Descending:
            return "descending";
        case Order::Ascending:
            return "ascending";
        default:
            throw std::runtime_error("Invalid Order enum");
    }
}

}  // namespace site

template <>
struct enumTo<site::Mode> {
    using enum site::Mode;
    static constexpr std::optional<site::Mode> operator()(std::string_view str) {
        if (str == enumToStr(Full)) {
            return Full;
        } else if (str == enumToStr(Plain)) {
            return Plain;
        } else if (str == enumToStr(Append)) {
            return Append;
        } else {
            return std::nullopt;
        }
    }
};
template <>
struct enumTo<site::Sort> {
    using enum site::Sort;
    static constexpr std::optional<site::Sort> operator()(std::string_view str) {
        if (str == enumToStr(Default)) {
            return Default;
        } else if (str == enumToStr(Name)) {
            return Name;
        } else if (str == enumToStr(Count)) {
            return Count;
        } else if (str == enumToStr(Size)) {
            return Size;
        } else if (str == enumToStr(First)) {
            return First;
        } else if (str == enumToStr(Last)) {
            return Last;
        } else if (str == enumToStr(Downloads)) {
            return Downloads;
        } else if (str == enumToStr(Use)) {
            return Use;
        } else if (str == enumToStr(Version)) {
            return Version;
        } else if (str == enumToStr(Arch)) {
            return Arch;
        } else if (str == enumToStr(SHA)) {
            return SHA;
        } else if (str == enumToStr(Time)) {
            return Time;
        } else if (str == enumToStr(Ip)) {
            return Ip;
        } else if (str == enumToStr(User)) {
            return User;
        } else if (str == enumToStr(Age)) {
            return Age;
        } else {
            return std::nullopt;
        }
    }
};
template <>
struct enumTo<site::Order> {
    using enum site::Order;
    static constexpr std::optional<site::Order> operator()(std::string_view str) {
        if (str == enumToStr(Ascending)) {
            return Ascending;
        } else if (str == enumToStr(Descending)) {
            return Descending;
        } else {
            return std::nullopt;
        }
    }
};

}  // namespace vcache

template <>
struct fmt::formatter<vcache::site::Mode> : vcache::FlagFormatter<vcache::site::Mode> {};
template <>
struct fmt::formatter<vcache::site::Sort> : vcache::FlagFormatter<vcache::site::Sort> {};
template <>
struct fmt::formatter<vcache::site::Order> : vcache::FlagFormatter<vcache::site::Order> {};

template <>
struct fmt::formatter<vcache::site::Url> {
    template <class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw format_error("Invalid url format found");
        }
        return it;
    }

    template <class FmtContext>
    constexpr FmtContext::iterator format(const vcache::site::Url& url, FmtContext& ctx) const {
        auto it = ctx.out();

        auto params = url.params |
                      std::views::transform([](const auto& item) { return fmt::join(item, "="); });

        fmt::format_to(it, "{}{}{}", url.path, vcache::FmtSel{url.params.empty(), "", "?"},
                       fmt::join(params, "&"));

        return it;
    }
};
