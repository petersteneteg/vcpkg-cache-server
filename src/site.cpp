#include <vcpkg-cache-server/site.hpp>
#include <vcpkg-cache-server/store.hpp>
#include <vcpkg-cache-server/functional.hpp>
#include <vcpkg-cache-server/database.hpp>

#include <fmt/format.h>
#include <fmt/std.h>
#include <fmt/chrono.h>

#include <ranges>
#include <algorithm>
#include <set>
#include <string>
#include <numeric>
#include <tuple>
#include <memory>
#include <bit>

#include <rapidfuzz/fuzz.hpp>

namespace vcache {

namespace html {

constexpr std::string_view pre = R"(
<html>
  <head>
    <link rel="icon" href="/favicon.svg"/>
    <link rel="mask-icon" href="/maskicon.svg" color="#000000">
    <style>
      dl {
         display: grid;
        grid-template-columns: max-content auto;
      }

      dt {
        grid-column-start: 1;
        padding: 1pt 5pt 1pt 5pt;
      }

      dd {
        grid-column-start: 2;
        padding: 1pt 5pt 1pt 5pt;
      }
      pre {
        display: inline;
      }
    </style>
  </head>
  <body>
)";

constexpr std::string_view post = R"(</body></html>)";

constexpr std::string_view form = R"(
<form id="formElem">
  <input type="file" name="abi_file" accept="text/*">
  Package: <input type="text" name="package">
  <input type="submit">
</form>
)";

constexpr std::string_view script = R"(
<script>
  formElem.onsubmit = async (e) => {
    e.preventDefault();
    let res = await fetch('/match', {
      method: 'POST',
      body: new FormData(formElem)
    });

    result.innerHTML = await res.text();
  };
</script>
)";

constexpr std::string_view favicon = R"(
<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 100 100"><rect width="100" height="100" rx="50" fill="#4a85a9"></rect><path d="M39.25 86.31L39.25 86.31Q37.81 86.31 35.78 85.73Q33.76 85.14 32.18 83.39Q30.61 81.64 30.61 78.30L30.61 78.30Q30.61 75.52 31.69 71.56Q32.77 67.59 34.39 62.73Q36.01 57.88 37.63 52.25Q39.25 46.63 40.33 40.55Q41.41 34.48 41.41 28.18L41.41 28.18Q41.41 25.02 40.95 22.59Q40.51 20.16 39.34 18.82Q38.17 17.47 35.92 17.47L35.92 17.47Q33.58 17.47 31.96 18.82Q30.34 20.16 29.30 22.33Q28.27 24.48 27.77 26.83Q27.28 29.16 27.28 31.23L27.28 31.23Q27.28 32.77 27.59 34.20Q27.91 35.65 28.63 37.09L28.63 37.09Q24.22 37.09 22.24 35.02Q20.26 32.95 20.26 29.88L20.26 29.88Q20.26 27.19 21.61 24.30Q22.96 21.43 25.34 19.04Q27.73 16.66 30.92 15.17Q34.12 13.69 37.72 13.69L37.72 13.69Q43.84 13.69 46.98 18.14Q50.14 22.59 50.14 30.34L50.14 30.34Q50.14 35.29 49.01 40.69Q47.89 46.09 46.27 51.53Q44.65 56.98 42.98 62.11Q41.31 67.23 40.19 71.60Q39.06 75.97 39.06 79.20L39.06 79.20Q39.06 80.64 39.47 81.45Q39.88 82.27 41.23 82.27L41.23 82.27Q43.84 82.27 47.12 79.83Q50.41 77.41 53.92 73.22Q57.42 69.03 60.75 63.73Q64.09 58.41 66.78 52.66Q69.48 46.89 71.06 41.23Q72.64 35.55 72.64 30.79L72.64 30.79Q72.64 26.02 70.92 23.50Q69.22 20.98 66.61 20.08L66.61 20.08Q67.86 17.38 69.71 16.16Q71.56 14.95 73.00 14.95L73.00 14.95Q75.42 14.95 77.59 18.01Q79.75 21.07 79.75 26.65L79.75 26.65Q79.75 31.51 77.90 37.76Q76.06 44.02 72.81 50.72Q69.58 57.42 65.48 63.77Q61.39 70.11 56.84 75.20Q52.30 80.28 47.80 83.30Q43.30 86.31 39.25 86.31Z" fill="#fff"></path></svg>
)";

constexpr std::string_view maskicon = R"(
<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<svg width="256" height="256" viewBox="0 0 100 100" version="1.1">
  <rect width="100" height="100" rx="50" fill="#4a85a9" id="rect1" style="fill:#000000" />
</svg>
)";

constexpr std::string_view style = R"(
<style>
    dl {
        display: grid;
        grid-template-columns: max-content auto;
    }
    dt {
        grid-column-start: 1;
        padding: 1pt 5pt 1pt 5pt;
    }

    dd {
        grid-column-start: 2;
        padding: 1pt 5pt 1pt 5pt;
    }
    pre {
        display: inline;
    }
    .pointer {
        cursor: pointer;
    }
    #search {
        align: left;
    }
</style>
)";

constexpr std::string_view index = R"(
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Vcpkg Cache Server</title>
    <link rel="icon" href="/favicon.svg"/>
    <link rel="mask-icon" href="/maskicon.svg" color="#000000">
    <link rel="stylesheet" href="/script/bootstrap.css">
    {0}
    <script src="/script/htmx.js"></script>
  </head>
  <body>
    <div class="container">
      <h1>
        <a href="/"><img src="/favicon.svg" width="70" height="70"></a>
        Vcpkg Cache Server
      </h1>
      <div id=content class="row">
        {1}
      </div>
    </div>
  </body>
<html>
)";

}  // namespace html

namespace site {

namespace detail {

size_t missmatches(const std::map<std::string, std::string>& map1,
                   const std::map<std::string, std::string>& map2) {
    auto keys = map1 | std::views::keys | std::ranges::to<std::set>();
    keys.insert_range(map2 | std::views::keys);

    return std::transform_reduce(
        std::begin(keys), std::end(keys), size_t{0}, std::plus<>{}, [&](const auto& key) {
            const auto it1 = map1.find(key);
            const auto it2 = map2.find(key);
            if (it1 != std::end(map1) && it2 != std::end(map2) && *it1 == *it2) {
                return size_t{0};
            }
            return size_t{1};
        });
}

std::string formatDiff(const std::map<std::string, std::string>& dstMap,
                       const std::map<std::string, std::string>& srcMap) {
    auto keys = dstMap | std::views::keys | std::ranges::to<std::set>();
    keys.insert_range(srcMap | std::views::keys);

    std::string buff;
    fmt::format_to(std::back_inserter(buff), "<dl>");
    for (const auto& key : keys) {
        auto dst = fp::mGet(dstMap, key);
        auto src = fp::mGet(srcMap, key);
        if (dst && src) {
            if (*dst != *src) {
                fmt::format_to(std::back_inserter(buff),
                               "<dt>{}</dt><dd><ul><li><code>{}</code></"
                               "li><li><code>{}</code></li></ul></dd>\n",
                               key, *dst, *src);
            }
        } else if (dst) {
            fmt::format_to(std::back_inserter(buff),
                           "<dt>{}</dt><dd>Missing in source <code>{}</code></dd>\n", key, *dst);
        } else if (src) {
            fmt::format_to(std::back_inserter(buff),
                           "<dt>{}</dt><dd>Missing in target <code>{}</code></dd>\n", key, *src);
        }
    }
    fmt::format_to(std::back_inserter(buff), "</dl>");
    return buff;
}

std::string formatMap(const std::map<std::string, std::string>& range) {
    std::string buff;
    fmt::format_to(std::back_inserter(buff), "<dl>\n");
    for (const auto& [key, val] : range) {
        fmt::format_to(std::back_inserter(buff), "<dt>{}</dt>\n", key);
        fmt::format_to(std::back_inserter(buff), "<dd>{}</dd>\n", val);
    }
    fmt::format_to(std::back_inserter(buff), "</dl>\n");
    return buff;
}

std::string button(Url url, std::string_view content, Sort tag, Sort currentSort,
                   Order currentOrder) {
    static constexpr std::string_view str = R"(
        <a class="pointer link-underline 
                  link-offset-2-hover 
                  link-underline-opacity-0 
                  link-underline-opacity-75-hover" 
           hx-get="{0}"
           hx-target="#content" 
           hx-swap="innerHTML" 
           hx-push-url="{1}">
            {2}{3}
        </a>
    )";

    static constexpr std::string_view upArrow = "&#8593";
    static constexpr std::string_view downArrow = "&#8595";

    const auto indicator = [&]() {
        if (tag == currentSort) {
            if (currentOrder == Order::Ascending) {
                return upArrow;
            } else {
                return downArrow;
            }
        } else {
            return std::string_view{""};
        }
    }();

    const auto newOrder = [&]() {
        if (tag == currentSort) {
            return currentOrder == Order::Ascending ? Order::Descending : Order::Ascending;
        } else {
            return Order::Ascending;
        }
    }();

    url.params["sort"] = fmt::to_string(tag);
    url.params["order"] = fmt::to_string(newOrder);

    url.params["mode"] = "plain";
    const auto plainUrl = fmt::to_string(url);
    url.params.erase("mode");
    const auto fullUrl = fmt::to_string(url);

    return fmt::format(str, plainUrl, fullUrl, content, indicator);
}

std::string pillBar(Url url, const std::set<std::string>& values, std::string_view selected) {
    static constexpr std::string_view str = R"(
        <a class="badge rounded-pill text-bg-{0} pointer text-decoration-none me-1"
           hx-get="{1}"
           hx-target="#content" 
           hx-swap="innerHTML" 
           hx-push-url="{2}">
            {3}
        </a>
    )";

    const auto pill = [&](std::string_view label, std::string_view value, bool active) {
        Url u = url;
        if (value.empty()) {
            u.params.erase("arch");
        } else {
            u.params["arch"] = std::string{value};
        }
        u.params["mode"] = "plain";
        const auto plainUrl = fmt::to_string(u);
        u.params.erase("mode");
        const auto fullUrl = fmt::to_string(u);
        return fmt::format(str, active ? "primary" : "secondary", plainUrl, fullUrl, label);
    };

    std::string buff = R"(<div class="d-flex flex-wrap gap-1 my-2">)";
    buff += pill("All", "", selected.empty());
    for (const auto& value : values) {
        buff += pill(value, value, value == selected);
    }
    buff += "</div>";
    return buff;
}

std::string navItem(std::string_view name, std::string_view url, bool active) {
    static constexpr std::string_view str = R"(
        <li class="breadcrumb-item">
            <a class="pointer link-underline 
                      link-offset-2-hover 
                      link-underline-opacity-0 
                      link-underline-opacity-75-hover" 
               hx-get="{0}?mode=plain" 
               hx-target="#content" 
               hx-swap="innerHTML" 
               hx-push-url={0}>
                {1}
            </a>
        </li>
    )";
    if (active) {
        return fmt::format(R"(<li class="breadcrumb-item active">{}</li>)", name);
    } else {
        return fmt::format(str, url, name);
    }
}

std::string link(std::string_view url, std::string_view content) {
    static constexpr std::string_view str = R"(
        <a class="pointer link-underline 
                    link-offset-2-hover 
                    link-underline-opacity-0 
                    link-underline-opacity-75-hover" 
            hx-get="{0}?mode=plain" 
            hx-target="#content" 
            hx-swap="innerHTML" 
            hx-push-url={0}>
            {1}
        </a>)";
    return fmt::format(str, url, content);
}

}  // namespace detail

namespace {

void formatMapTo(const auto& range, std::string& buff) {
    fmt::format_to(std::back_inserter(buff), "<dl>\n");
    for (const auto& [key, val] : range) {
        fmt::format_to(std::back_inserter(buff), "<dt>{}</dt>\n", key);
        fmt::format_to(std::back_inserter(buff), "<dd>{}</dd>\n", val);
    }
    fmt::format_to(std::back_inserter(buff), "</dl>\n");
}

void formatInfoTo(const Info& info, std::string& buff) {
    fmt::format_to(std::back_inserter(buff),
                   "<h2>{}</h2><dl>"
                   "<dt>Version:</dt><dd>{}</dd>"
                   "<dt>Arch:</dt><dd>{}</dd>"
                   "<dt>Created:</dt><dd>{:%Y-%m-%d %H:%M:%S}</dd>"
                   "<dt>Size:</dt><dd>{}</dd>"
                   "</dl>\n",
                   info.package, info.version, info.arch, info.time, ByteSize{info.size});
    formatMapTo(info.ctrl, buff);
    formatMapTo(info.abi, buff);
}

std::string formatInfo(const Info& info) {
    std::string buff;
    formatInfoTo(info, buff);
    return buff;
}

template <size_t N>
struct Getter {
    template <typename T>
    decltype(auto) operator()(T&& arg) {
        return std::get<N>(std::forward<T>(arg));
    }
};

std::string buttonIdx(Url url, std::string_view content, size_t sortIdx, size_t currentSortIdx,
                      Order currentOrder) {
    static constexpr std::string_view str = R"(
        <a class="pointer link-underline 
                  link-offset-2-hover 
                  link-underline-opacity-0 
                  link-underline-opacity-75-hover" 
           hx-get="{0}"
           hx-target="#content" 
           hx-swap="innerHTML" 
           hx-push-url="{1}">
            {2}{3}
        </a>
    )";

    static constexpr std::string_view upArrow = "&#8593";
    static constexpr std::string_view downArrow = "&#8595";

    const auto indicator = [&]() {
        if (sortIdx == currentSortIdx) {
            if (currentOrder == Order::Ascending) {
                return upArrow;
            } else {
                return downArrow;
            }
        } else {
            return std::string_view{};
        }
    }();

    const auto newOrder = [&]() {
        if (sortIdx == currentSortIdx) {
            return currentOrder == Order::Ascending ? Order::Descending : Order::Ascending;
        } else {
            return Order::Ascending;
        }
    }();

    url.params["sortidx"] = fmt::to_string(sortIdx);
    url.params["order"] = fmt::to_string(newOrder);

    url.params["mode"] = "plain";
    const auto plainUrl = fmt::to_string(url);

    url.params["mode"] = "full";
    const auto fullUrl = fmt::to_string(url);

    return fmt::format(str, plainUrl, fullUrl, content, indicator);
}

std::string downloadsLink(Params params, std::optional<Crumb> back = std::nullopt) {
    Url purl{.path = "/downloads", .params = {{"mode", "plain"}}};
    Url furl{.path = "/downloads", .params = {{"mode", "full"}}};
    purl.params.insert_range(params);
    furl.params.insert_range(params);
    if (back) {
        purl.params["backLabel"] = back->first;
        purl.params["backUrl"] = back->second;
        furl.params["backLabel"] = back->first;
        furl.params["backUrl"] = back->second;
    }

    constexpr std::string_view str = R"(
        <div class="d-inline-block float-end fs-4 me-3">
            <a class="pointer link-underline 
                    link-offset-2-hover 
                    link-underline-opacity-0 
                    link-underline-opacity-75-hover" 
                hx-get="{}" 
                hx-target="#content" 
                hx-swap="innerHTML" 
                hx-push-url={}>
                    Downloads
            </a>
        </div>)";
    return fmt::format(str, purl, furl);
}

std::string purgeLink(Params params, std::optional<Crumb> back = std::nullopt) {
    Url purl{.path = "/purge", .params = {{"mode", "plain"}}};
    Url furl{.path = "/purge", .params = {{"mode", "full"}}};
    purl.params.insert_range(params);
    furl.params.insert_range(params);
    if (back) {
        purl.params["backLabel"] = back->first;
        purl.params["backUrl"] = back->second;
        furl.params["backLabel"] = back->first;
        furl.params["backUrl"] = back->second;
    }

    constexpr std::string_view str = R"(
        <div class="d-inline-block float-end fs-4 me-3">
            <a class="pointer link-underline 
                    link-offset-2-hover 
                    link-underline-opacity-0 
                    link-underline-opacity-75-hover" 
                hx-get="{}" 
                hx-target="#content" 
                hx-swap="innerHTML" 
                hx-push-url={}>
                    Purge
            </a>
        </div>)";
    return fmt::format(str, purl, furl);
}

}  // namespace

std::string detail::nav(const std::vector<std::pair<std::string, std::string>>& path) {
    const auto str = path | std::views::transform([i = size_t{0}, &path](const auto& item) mutable {
                         ++i;
                         return detail::navItem(item.first, item.second, i == path.size());
                     }) |
                     std::views::join | std::ranges::to<std::string>();

    return fmt::format(R"(<nav class="d-inline-block"><ol class="breadcrumb fs-4">{}</ol></nav>)",
                       str);
}

std::string detail::nav(std::optional<Crumb> back, Crumb self) {
    std::vector<Crumb> path{{"Packages", "/"}};
    if (back) {
        path.push_back(*back);
    }
    path.push_back(std::move(self));
    return nav(path);
}

std::string detail::deliver(std::string_view content, Mode mode) {
    if (mode == Mode::Plain) {
        return std::string{content};
    } else if (mode == Mode::Full) {
        return fmt::format(html::index, html::style, content);
    } else {
        return "";
    }
}

struct RowItem {
    std::string name;
    size_t count;
    size_t diskSize;
    size_t downloads;
    Time lastUse;
    Time firstTime;
    Time lastTime;
    double similarity;
};

template <Sort S>
decltype(auto) getRowItem() {
    using enum Sort;
    if constexpr (S == Name)
        return &RowItem::name;
    else if constexpr (S == Count)
        return &RowItem::count;
    else if constexpr (S == Size)
        return &RowItem::diskSize;
    else if constexpr (S == First)
        return &RowItem::firstTime;
    else if constexpr (S == Last)
        return &RowItem::lastTime;
    else if constexpr (S == Downloads)
        return &RowItem::downloads;
    else if constexpr (S == Use)
        return &RowItem::lastUse;
    else
        return nullptr;
}

std::string index(const Store& store, db::Database& db, State state) {
    const auto keys =
        store.allInfos() | std::views::transform(&Info::package) | std::ranges::to<std::set>();
    const auto archs =
        store.allInfos() | std::views::transform(&Info::arch) | std::ranges::to<std::set>();

    std::map<std::string, std::vector<const Info*>> packages;
    std::ranges::for_each(store.allInfos(), [&](const Info& info) {
        if (state.archFilter.empty() || info.arch == state.archFilter) {
            packages[info.package].push_back(&info);
        }
    });

    rapidfuzz::fuzz::CachedPartialRatio<char> scorer(state.search);
    auto list = packages | std::views::transform([&](const auto& package) -> RowItem {
                    auto& [name, items] = package;
                    const auto range =
                        items | std::views::transform([](const auto* item) { return item->size; });
                    const auto diskSize = std::accumulate(std::begin(range), std::end(range),
                                                          size_t{0}, std::plus<>{});

                    const auto [firstIt, lastIt] = std::ranges::minmax_element(
                        items, std::less<>{}, [](const Info* i) { return i->time; });

                    auto similarity = 100.0;
                    if (!state.search.empty()) {
                        const auto shaSimilarity =
                            std::ranges::max(items | std::views::transform([&](const auto* item) {
                                                 return scorer.similarity(item->sha);
                                             }));
                        similarity = std::max(shaSimilarity, scorer.similarity(name));
                    }

                    const auto [downloads, lastUse] = db::getPackageDownloadsAndLastUse(db, name);

                    return {name,    items.size(),     diskSize,        downloads,
                            lastUse, (*firstIt)->time, (*lastIt)->time, similarity};
                }) |
                std::views::filter([](const RowItem& item) { return item.similarity > 55.0; }) |
                std::ranges::to<std::vector>();

    constexpr auto table = []<size_t... Is>(std::integer_sequence<size_t, Is...>) {
        return std::array{+[](decltype(list)& list, Order order) {
            auto proj = getRowItem<static_cast<Sort>(Is)>();
            if constexpr (!std::is_same_v<decltype(proj), std::nullptr_t>) {
                if (order == Order::Ascending) {
                    std::ranges::sort(list, std::less<>{}, proj);
                } else {
                    std::ranges::sort(list, std::greater<>{}, proj);
                }
            }
        }...};
    }
    (std::make_integer_sequence<size_t, std::to_underlying(Sort::NumSortMethods)>());
    if (state.sort == Sort::Default && !state.search.empty()) {
        std::ranges::sort(list, std::greater<>{}, &RowItem::similarity);
    } else {
        table[std::to_underlying(state.sort)](list, state.order);
    }

    static constexpr std::string_view itemStr = R"(
        <tr>
            <td>
               <a class="pointer link-underline 
                        link-offset-2-hover 
                        link-underline-opacity-0 
                        link-underline-opacity-75-hover" 
                  hx-get="{7}"
                  hx-target="#content" 
                  hx-swap="innerHTML"
                  hx-push-url="{8}">
                    <b>{0}</b>
                </a>
            </td>
            <td>{1}</td>
            <td>{2:M}</td>
            <td>{3}</td>
            <td>{4:%Y-%m-%d %H:%M}</td>
            <td>{5:%Y-%m-%d %H:%M}</td>
            <td>{6:%Y-%m-%d %H:%M}</td>
        </tr>
    )";

    const auto str =
        list | std::views::transform([&](const RowItem& item) {
            Url plainUrl{.path = fmt::format("/find/{}", item.name), .params = {{"mode", "plain"}}};
            Url fullUrl{.path = fmt::format("/find/{}", item.name), .params = {}};
            if (!state.archFilter.empty()) {
                plainUrl.params["arch"] = std::string{state.archFilter};
                fullUrl.params["arch"] = std::string{state.archFilter};
            }
            if (!state.search.empty()) {
                plainUrl.params["search"] = std::string{state.search};
                fullUrl.params["search"] = std::string{state.search};
            }
            return fmt::format(itemStr, item.name, item.count, ByteSize{item.diskSize},
                               item.downloads, item.lastUse, item.firstTime, item.lastTime,
                               plainUrl, fullUrl);
        }) |
        std::views::join | std::ranges::to<std::string>();

    const auto totalSize = std::ranges::fold_left(
        list | std::views::transform([&](const RowItem& item) { return item.diskSize; }), size_t{0},
        std::plus<>{});
    const auto totalCount = std::ranges::fold_left(
        list | std::views::transform([&](const RowItem& item) { return item.count; }), size_t{0},
        std::plus<>{});

    const auto stats = fmt::format("Found {} caches of {} packages. Using {}", totalCount,
                                   list.size(), ByteSize{totalSize});

    // Shared state carried by both the sort buttons and the arch pills, so clicking one
    // doesn't discard the other's current selection (or the search term).
    Url stateUrl{
        .path = "/",
        .params = {{"sort", fmt::to_string(state.sort)}, {"order", fmt::to_string(state.order)}}};
    if (!state.search.empty()) stateUrl.params["search"] = std::string{state.search};
    if (!state.archFilter.empty()) stateUrl.params["arch"] = std::string{state.archFilter};

    const auto nameButton =
        detail::button(stateUrl, "Package", Sort::Name, state.sort, state.order);
    const auto countButton =
        detail::button(stateUrl, "Count", Sort::Count, state.sort, state.order);
    const auto sizeButton = detail::button(stateUrl, "Size", Sort::Size, state.sort, state.order);
    const auto downloadsButton =
        detail::button(stateUrl, "Downloads", Sort::Downloads, state.sort, state.order);
    const auto useButton = detail::button(stateUrl, "Last Use", Sort::Use, state.sort, state.order);
    const auto firstButton =
        detail::button(stateUrl, "First Cache", Sort::First, state.sort, state.order);
    const auto lastButton =
        detail::button(stateUrl, "Last Cache", Sort::Last, state.sort, state.order);

    const auto headerRow = fmt::format(R"(
            <tr>
                <th>{}</th>
                <th>{}</th>
                <th>{}</th>
                <th>{}</th>
                <th>{}</th>
                <th>{}</th>
                <th>{}</th>
            </tr>
            )",
                                       nameButton, countButton, sizeButton, downloadsButton,
                                       useButton, firstButton, lastButton);

    const auto nav = detail::nav({{"Packages", "/"}});
    const auto archPills = detail::pillBar(stateUrl, archs, state.archFilter);

    static constexpr std::string_view html = R"(
        <div>{0}{1}</div>
        {6}
        <input class="form-control"
               id="search"
               type="search"
               name="search"
               value="{2}"
               placeholder="Search Packages..."
               hx-get="?mode=plain" 
               hx-target="#content" 
               hx-swap="innerHTML"
               hx-trigger="input changed delay:500ms, keyup[key=='Enter']"
               hx-indicator=".htmx-indicator">
        <h4>{3}</h4>
        <span class="htmx-indicator">Searching...</span>
        <div class="table-responsive">
            <table class="table table-hover table-sm align-middle">
                <thead>{4}</thead>
                <tbody>{5}</tbody>
            </table>
        </div>
    )";
    const auto content =
        fmt::format(html, nav, fmt::format("{}{}", purgeLink({}), downloadsLink({})), state.search,
                    stats, headerRow, str, archPills);

    return detail::deliver(content, state.mode);
}

namespace {

std::string purgeRow(const Info& info) {
    static constexpr std::string_view itemStr = R"(
        <tr>
            <td>{0}</td>
            <td>{1}</td>
            <td>{2}</td>
            <td><pre>{3}</pre></td>
            <td>{4}</td>
            <td>{5:%Y-%m-%d %H:%M}</td>
        </tr>
    )";
    return fmt::format(itemStr, info.package, info.version, info.arch, info.sha.substr(0, 15),
                       ByteSize{info.size}, info.time);
}

constexpr std::string_view purgeTableHeader = R"(
    <tr>
        <th>Package</th>
        <th>Version</th>
        <th>Arch</th>
        <th>SHA</th>
        <th>Size</th>
        <th>Created</th>
    </tr>
)";

}  // namespace

std::string purge(const PurgePattern& pattern, const Store& store, Limit limit, Mode mode,
                  std::optional<Crumb> back) {
    Url purgeUrl{.path = "/purge", .params = {}};
    if (pattern.sha) purgeUrl.params["sha"] = *pattern.sha;
    if (pattern.package) purgeUrl.params["package"] = *pattern.package;
    if (pattern.version) purgeUrl.params["version"] = *pattern.version;
    if (pattern.arch) purgeUrl.params["arch"] = *pattern.arch;

    const auto isEmpty = vcache::empty(pattern);

    size_t totalMatches = 0;
    size_t totalSize = 0;
    std::string previewSection;
    std::string curlSection;

    if (!isEmpty) {
        auto matched = findMatches(store, pattern);
        std::ranges::sort(matched, std::less<>{}, [](const Info& i) {
            return std::tie(i.package, i.version, i.arch, i.sha);
        });

        totalMatches = matched.size();
        totalSize = std::ranges::fold_left(matched | std::views::transform(&Info::size), size_t{0},
                                           std::plus<>{});

        if (totalMatches == 0) {
            previewSection = "<p>No cache entries match this pattern.</p>";
        } else {
            const auto pageSize = limit.limit.value_or(size_t{50});
            const auto offset = std::min(limit.offset.value_or(size_t{0}), matched.size());
            const auto count = std::min(pageSize, matched.size() - offset);

            const auto rows = matched | std::views::drop(offset) | std::views::take(count) |
                              std::views::transform([](const Info& i) { return purgeRow(i); }) |
                              std::views::join | std::ranges::to<std::string>();

            std::string pager;
            if (offset + count < matched.size()) {
                auto nextUrl = purgeUrl;
                nextUrl.params["mode"] = "plain";
                nextUrl.params["offset"] = fmt::to_string(offset + count);
                nextUrl.params["limit"] = fmt::to_string(pageSize);
                pager = fmt::format(
                    R"(<button class="btn btn-link" hx-get="{}" hx-target="#content" hx-swap="innerHTML">More…</button>)",
                    nextUrl);
            }

            previewSection = fmt::format(
                R"(<h5>{} matching entries, {} (showing {}-{})</h5>)"
                R"(<div class="table-responsive"><table class="table table-hover table-sm align-middle"><thead>{}</thead><tbody>{}</tbody></table></div>{})",
                totalMatches, ByteSize{totalSize}, offset + 1, offset + count, purgeTableHeader,
                rows, pager);

            curlSection = fmt::format(
                R"(<h6>Equivalent curl command</h6><pre>curl -X POST -H "Authorization: Bearer &lt;TOKEN&gt;" "https://&lt;server&gt;{}"</pre>)",
                purgeUrl);
        }
    } else {
        previewSection =
            "<p>Enter at least one pattern above to preview matching cache entries.</p>";
    }

    static constexpr std::string_view formHtml = R"(
        <div class="row g-2">
            <div class="col">
                <input class="form-control" type="text" id="purge-sha" name="sha"
                       value="{0}" placeholder="SHA pattern, e.g. abcd*"
                       hx-get="/purge?mode=plain" hx-target="#content" hx-swap="innerHTML"
                       hx-include="#purge-sha,#purge-package,#purge-version,#purge-arch"
                       hx-trigger="input changed delay:500ms, keyup[key=='Enter']">
            </div>
            <div class="col">
                <input class="form-control" type="text" id="purge-package" name="package"
                       value="{1}" placeholder="Package pattern, e.g. zlib*"
                       hx-get="/purge?mode=plain" hx-target="#content" hx-swap="innerHTML"
                       hx-include="#purge-sha,#purge-package,#purge-version,#purge-arch"
                       hx-trigger="input changed delay:500ms, keyup[key=='Enter']">
            </div>
            <div class="col">
                <input class="form-control" type="text" id="purge-version" name="version"
                       value="{2}" placeholder="Version pattern, e.g. 1.2.*"
                       hx-get="/purge?mode=plain" hx-target="#content" hx-swap="innerHTML"
                       hx-include="#purge-sha,#purge-package,#purge-version,#purge-arch"
                       hx-trigger="input changed delay:500ms, keyup[key=='Enter']">
            </div>
            <div class="col">
                <input class="form-control" type="text" id="purge-arch" name="arch"
                       value="{3}" placeholder="Arch pattern, e.g. x64-linux"
                       hx-get="/purge?mode=plain" hx-target="#content" hx-swap="innerHTML"
                       hx-include="#purge-sha,#purge-package,#purge-version,#purge-arch"
                       hx-trigger="input changed delay:500ms, keyup[key=='Enter']">
            </div>
        </div>
        <div class="row g-2 mt-1">
            <div class="col-4">
                <input class="form-control" type="text" id="purge-token"
                       placeholder="Bearer token, required to purge">
            </div>
            <div class="col-auto">
                <button class="btn btn-danger"
                        hx-post="{4}"
                        hx-headers='js:{{"Authorization": "Bearer " + document.getElementById("purge-token").value}}'
                        hx-confirm="Permanently delete all {5} matching cache entries?"
                        hx-target="#content" hx-swap="innerHTML" {6}>
                    Purge matches
                </button>
            </div>
        </div>
    )";

    const auto disabledAttr = (isEmpty || totalMatches == 0) ? "disabled" : "";
    const auto form = fmt::format(formHtml, pattern.sha.value_or(""), pattern.package.value_or(""),
                                  pattern.version.value_or(""), pattern.arch.value_or(""), purgeUrl,
                                  totalMatches, disabledAttr);

    const auto nav = detail::nav(back, {"Purge", "/purge"});
    const auto content = fmt::format(R"(<div>{}</div><h4>Purge Caches</h4>{}{}{})", nav, form,
                                     previewSection, curlSection);

    return detail::deliver(content, mode);
}

std::string purgeResult(const PurgeResult& result, Mode mode) {
    const auto rows = result.removed |
                      std::views::transform([](const Info& i) { return purgeRow(i); }) |
                      std::views::join | std::ranges::to<std::string>();

    const auto heading = result.dryrun
                             ? fmt::format("Preview: {} entries ({}) would be purged",
                                           result.removed.size(), ByteSize{result.totalSize})
                             : fmt::format("Purged {} entries, freed {}", result.removed.size(),
                                           ByteSize{result.totalSize});

    const auto nav = detail::nav({{"Packages", "/"}, {"Purge", "/purge"}});
    const auto content = fmt::format(
        R"(<div>{}</div><h4>{}</h4><div class="table-responsive">)"
        R"(<table class="table table-hover table-sm align-middle"><thead>{}</thead><tbody>{}</tbody></table></div>)",
        nav, heading, purgeTableHeader, rows);

    return detail::deliver(content, mode);
}

std::string match() {
    return fmt::format(R"({}{}<div id="result"></div>{}{})", html::pre, html::form, html::script,
                       html::post);
}

std::string match(std::string_view abi, std::string_view package, const Store& store) {
    const auto abiMap = abi | fp::splitIntoPairs('\n', ' ') | std::ranges::to<std::map>();

    auto matches = store.allInfos() |
                   std::views::filter([&](const auto& info) { return info.package == package; }) |
                   std::views::transform([&](const auto& info) { return info; }) |
                   std::ranges::to<std::vector>();
    std::ranges::sort(matches, std::less<>{},
                      [&](const auto& info) { return detail::missmatches(info.abi, abiMap); });

    const auto str =
        matches | std::views::take(3) | std::views::transform([&](const auto& info) {
            return fmt::format("<div><h3>Time: {:%Y-%m-%d %H:%M:%S} {}</h3>{}</div>", info.time,
                               info.sha, detail::formatDiff(abiMap, info.abi));
        }) |
        std::views::join | std::ranges::to<std::string>();

    return fmt::format(R"(<h1>Target ABI:</h1><div>{}</div><div>{}</div>)",
                       detail::formatMap(abiMap), str);
}

std::string compare(std::string_view sha, const Store& store, Mode mode) {
    const auto* targetInfo = store.info(sha);
    if (!targetInfo) {
        return detail::deliver(fmt::format("<h1>Error</h1><div>Sha: {} not found</div>", sha),
                               mode);
    }

    const auto& abiMap = targetInfo->abi;
    const auto& package = targetInfo->package;

    auto matches = store.allInfos() |
                   std::views::filter([&](const auto& info) { return info.sha != sha; }) |
                   std::views::filter([&](const auto& info) { return info.package == package; }) |
                   std::views::transform([&](const auto& info) { return info; }) |
                   std::ranges::to<std::vector>();
    std::ranges::sort(matches, std::less<>{},
                      [&](const auto& info) { return detail::missmatches(info.abi, abiMap); });

    const auto str =
        matches | std::views::take(5) | std::views::transform([&](const auto& info) {
            return fmt::format("<div><h3>Time: {:%Y-%m-%d %H:%M:%S} {}</h3>{}</div>", info.time,
                               info.sha, detail::formatDiff(abiMap, info.abi));
        }) |
        std::views::join | std::ranges::to<std::string>();

    const auto nav =
        detail::nav({{"Packages", "/"},
                     {targetInfo->package, fmt::format("/find/{}", targetInfo->package)},
                     {targetInfo->sha, fmt::format("/package/{}", targetInfo->sha)},
                     {"Compare", fmt::format("/compare/{}", targetInfo->sha)}});

    return detail::deliver(fmt::format("{}{}<div>{}</div>", nav, formatInfo(*targetInfo), str),
                           mode);
}

struct CacheItem {
    std::string_view version;
    std::string_view arch;
    size_t diskSize;
    size_t downloads;
    Time lastUse;
    Time created;
    std::string_view sha;
    double similarity;
};

template <Sort S>
decltype(auto) getCacheItem() {
    using enum Sort;
    if constexpr (S == Version)
        return &CacheItem::version;
    else if constexpr (S == Arch)
        return &CacheItem::arch;
    else if constexpr (S == Size)
        return &CacheItem::diskSize;
    else if constexpr (S == First)
        return &CacheItem::created;
    else if constexpr (S == Downloads)
        return &CacheItem::downloads;
    else if constexpr (S == Use)
        return &CacheItem::lastUse;
    else if constexpr (S == SHA)
        return &CacheItem::sha;
    else
        return &CacheItem::created;
}

std::string find(std::string_view package, const Store& store, db::Database& db, State state) {

    const auto matchesPackage = [&](const auto& info) { return info.package == package; };

    const auto archs = store.allInfos() | std::views::filter(matchesPackage) |
                       std::views::transform(&Info::arch) | std::ranges::to<std::set>();

    rapidfuzz::fuzz::CachedPartialRatio<char> scorer(state.search);
    auto list =
        store.allInfos() | std::views::filter(matchesPackage) |
        std::views::filter([&](const auto& info) {
            return state.archFilter.empty() || info.arch == state.archFilter;
        }) |
        std::views::transform([&](const auto& info) -> CacheItem {
            const auto [downloads, lastUse] = db::getCacheDownloadsAndLastUse(db, info.sha);
            const auto similarity =
                state.search.empty()
                    ? 100.0
                    : std::ranges::max({scorer.similarity(package), scorer.similarity(info.version),
                                        scorer.similarity(info.sha), scorer.similarity(info.arch)});

            return {.version = info.version,
                    .arch = info.arch,
                    .diskSize = info.size,
                    .downloads = downloads,
                    .lastUse = lastUse,
                    .created = info.time,
                    .sha = info.sha,
                    .similarity = similarity};
        }) |
        std::views::filter([&](const CacheItem& item) { return item.similarity > 55.0; }) |
        std::ranges::to<std::vector>();

    constexpr auto table = []<size_t... Is>(std::integer_sequence<size_t, Is...>) {
        return std::array{+[](decltype(list)& list, Order order) {
            auto proj = getCacheItem<static_cast<Sort>(Is)>();
            if (order == Order::Ascending) {
                std::ranges::sort(list, std::less<>{}, proj);
            } else {
                std::ranges::sort(list, std::greater<>{}, proj);
            }
        }...};
    }
    (std::make_integer_sequence<size_t, std::to_underlying(Sort::NumSortMethods)>());
    if (state.sort == Sort::Default && !state.search.empty()) {
        std::ranges::sort(list, std::greater<>{}, &CacheItem::similarity);
    } else {
        table[std::to_underlying(state.sort)](list, state.order);
    }

    const auto path = fmt::format("/find/{0}", package);

    // Shared state carried by both the sort buttons and the arch pills, so clicking one
    // doesn't discard the other's current selection.
    Url stateUrl{
        .path = path,
        .params = {{"sort", fmt::to_string(state.sort)}, {"order", fmt::to_string(state.order)}}};
    if (!state.archFilter.empty()) stateUrl.params["arch"] = std::string{state.archFilter};
    if (!state.search.empty()) stateUrl.params["search"] = std::string{state.search};

    const auto versionButton =
        detail::button(stateUrl, "Version", Sort::Version, state.sort, state.order);
    const auto archButton = detail::button(stateUrl, "Arch", Sort::Arch, state.sort, state.order);
    const auto sizeButton = detail::button(stateUrl, "Size", Sort::Size, state.sort, state.order);
    const auto downloadsButton =
        detail::button(stateUrl, "Downloads", Sort::Downloads, state.sort, state.order);
    const auto useButton = detail::button(stateUrl, "Last Use", Sort::Use, state.sort, state.order);
    const auto firstButton =
        detail::button(stateUrl, "Created", Sort::First, state.sort, state.order);
    const auto shaButton = detail::button(stateUrl, "SHA", Sort::SHA, state.sort, state.order);

    const auto headerRow = fmt::format(R"(
            <tr>
                <th>{}</th>
                <th>{}</th>
                <th>{}</th>
                <th>{}</th>
                <th>{}</th>
                <th>{}</th>
                <th>{}</th>
                <th></th>
            </tr>
            )",
                                       versionButton, archButton, sizeButton, downloadsButton,
                                       useButton, firstButton, shaButton);

    static constexpr std::string_view itemStr = R"(
        <tr>
            <td>{0}</td>
            <td>{1}</td>
            <td>{2}</td>
            <td>{3}</td>
            <td>{4:%Y-%m-%d %H:%M}</td>
            <td>{5:%Y-%m-%d %H:%M}</td>
            <td>
                <a class="pointer link-underline 
                         link-offset-2-hover 
                         link-underline-opacity-0 
                         link-underline-opacity-75-hover"
                   hx-get="/package/{6}?mode=plain" 
                   hx-target="#content"
                   hx-swap="innerHTML"  
                   hx-push-url="/package/{6}"> 
                    {7}
                </a>
            </td>
            <td>
                <a class="pointer link-underline 
                          link-offset-2-hover 
                          link-underline-opacity-0 
                          link-underline-opacity-75-hover"
                   hx-get="/compare/{6}?mode=plain" 
                   hx-target="#content"
                   hx-swap="innerHTML" 
                   hx-push-url="/compare/{6}">
                    Compare
                </a>
            </td>
        </tr>
    )";

    const auto str = list | std::views::transform([&](const CacheItem& item) {
                         return fmt::format(itemStr, item.version, item.arch,
                                            ByteSize{item.diskSize}, item.downloads, item.lastUse,
                                            item.created, item.sha, item.sha.substr(0, 15));
                     }) |
                     std::views::join | std::ranges::to<std::string>();

    const auto count = list.size();
    const auto diskSize = std::ranges::fold_left(list | std::views::transform(&CacheItem::diskSize),
                                                 size_t{0}, std::plus<>{});

    const auto backCrumb = Crumb{std::string{package}, fmt::format("/find/{}", package)};
    const auto nav =
        detail::nav({{"Packages", "/"}, {std::string{package}, fmt::format("/find/{}", package)}});

    static constexpr std::string_view html = R"(
        <div>{0}{1}{2}</div>
        {3}
        <input class="form-control"
               id="search"
               type="search"
               name="search"
               value="{4}"
               placeholder="Search Versions..."
               hx-get="{5}?mode=plain" 
               hx-target="#content" 
               hx-swap="innerHTML"
               hx-trigger="input changed delay:500ms, keyup[key=='Enter']"
               hx-indicator=".htmx-indicator">
        <h4>Count: {6}, Total Size: {7}</h4>
        <span class="htmx-indicator">Searching...</span>
        <div class="table-responsive">
            <table class="table table-hover table-sm align-middle">
                <thead>{8}</thead>
                <tbody>{9}</tbody>
            </table>
        </div>
    )";
    const auto content = fmt::format(
        html, nav, purgeLink({{"package", std::string{package}}}, backCrumb),
        downloadsLink({{"selcol", "name"}, {"selval", std::string{package}}}, backCrumb),
        detail::pillBar(stateUrl, archs, state.archFilter), state.search, path, count,
        ByteSize{diskSize}, headerRow, str);

    return detail::deliver(content, state.mode);
}

std::string sha(std::string_view sha, const Store& store, Mode mode) {
    const auto* info = store.info(sha);
    if (!info) {
        return detail::deliver(fmt::format("<h1>Error</h1><div>Sha: {} not found</div>", sha),
                               mode);
    }
    const auto finfo = formatInfo(*info);
    const auto nav =
        detail::nav({{"Packages", "/"},
                     {info->package, fmt::format("/find/{}", info->package)},
                     {info->sha.substr(0, 16), fmt::format("/package/{}", info->sha)}});

    const auto backCrumb =
        Crumb{std::string{info->sha.substr(0, 16)}, fmt::format("/package/{}", info->sha)};
    return detail::deliver(
        fmt::format("<div>{}{}{}</div>{}", nav, purgeLink({{"sha", info->sha}}, backCrumb),
                    downloadsLink({{"selcol", "sha"}, {"selval", info->sha}}, backCrumb), finfo),
        mode);
}

auto colNames(const auto& stmt) {
    constexpr auto count = decltype(stmt.expression.col)::count;
    std::array<std::string, count> names{};
    for (int i = 0; i < count; ++i) {
        names[i] = stmt.column_name(i);
    }
    return names;
}

auto executeQueary(db::Database& db, auto& cols, auto& orderBy, Limit limits,
                   std::optional<std::pair<Sort, std::string>> selection) {
    using namespace sqlite_orm;

    const auto join1 = inner_join<db::Cache>(on(c(&db::Download::cache) == &db::Cache::id));
    const auto join2 = inner_join<db::Package>(on(c(&db::Cache::package) == &db::Package::id));
    const auto lim = limit(limits.offset.value_or(size_t{0}), limits.limit.value_or(size_t{100}));

    if (selection && selection->first == Sort::SHA) {
        const auto w = where(c(&db::Cache::sha) == selection->second);
        const auto stmt = db.prepare(select(cols, join1, join2, w, orderBy, lim));
        return std::tuple{db.execute(stmt), colNames(stmt)};
    } else if (selection && selection->first == Sort::Name) {
        const auto w = where(c(&db::Package::name) == selection->second);
        const auto stmt = db.prepare(select(cols, join1, join2, w, orderBy, lim));
        return std::tuple{db.execute(stmt), colNames(stmt)};
    } else if (selection && selection->first == Sort::Ip) {
        const auto w = where(c(&db::Download::ip) == selection->second);
        const auto stmt = db.prepare(select(cols, join1, join2, w, orderBy, lim));
        return std::tuple{db.execute(stmt), colNames(stmt)};
    } else if (selection && selection->first == Sort::User) {
        const auto w = where(c(&db::Download::user) == selection->second);
        const auto stmt = db.prepare(select(cols, join1, join2, w, orderBy, lim));
        return std::tuple{db.execute(stmt), colNames(stmt)};
    } else {
        const auto stmt = db.prepare(select(cols, join1, join2, orderBy, lim));
        return std::tuple{db.execute(stmt), colNames(stmt)};
    }
}

std::string downloads(db::Database& db, Mode mode, std::optional<size_t> sortIdx,
                      std::optional<Order> order, Limit limits,
                      std::optional<std::pair<Sort, std::string>> selection,
                      std::optional<Crumb> back) {

    using namespace sqlite_orm;

    const auto cols = columns(&db::Download::time, &db::Download::ip, &db::Download::user,
                              &db::Package::name, &db::Package::downloads, &db::Cache::size,
                              (c(&db::Download::time) - c(&db::Cache::created)), &db::Cache::sha);

    auto orderBy = dynamic_order_by(db);
    constexpr auto table = []<size_t... Is>(std::integer_sequence<size_t, Is...>) {
        return std::array{+[](decltype(orderBy)& ordering, decltype(cols)& cols, Order order) {
            auto item = order_by(std::get<Is>(cols.columns));
            ordering.push_back(setOrder(item, order));
        }...};
    }
    (std::make_integer_sequence<size_t, cols.count>());
    table[sortIdx.value_or(size_t{0})](orderBy, cols, order.value_or(Order::Descending));

    auto [data, names] = executeQueary(db, cols, orderBy, limits, selection);

    names[6] = "age";

    Url url{.path = "/downloads", .params = {}};
    if (selection) {
        url.params["selcol"] = fmt::to_string(selection->first);
        url.params["selval"] = selection->second;
    }
    if (back) {
        url.params["backLabel"] = back->first;
        url.params["backUrl"] = back->second;
    }

    const auto header = [&]<size_t... Is>(std::integer_sequence<size_t, Is...>) {
        return std::array{[&]() {
            const auto button = buttonIdx(url, names[Is], Is, sortIdx.value_or(size_t{0}),
                                          order.value_or(Order::Descending));
            return fmt::format(R"(<th>{}</th>)", button);
        }()...};
    }
    (std::make_integer_sequence<size_t, cols.count>());
    const auto headerRow =
        fmt::format(R"(<tr>{}</tr>)", header | std::views::join | std::ranges::to<std::string>());

    static constexpr std::string_view itemStr = R"(
        <tr {8}>
            <td>{0:%Y-%m-%d %H:%M}</td>
            <td>{1}</td>
            <td>{2}</td>
            <td>{3}</td>
            <td>{4}</td>
            <td>{5}</td>
            <td>{6}</td>
            <td>{7}</td>
        </tr>
        )";

    auto turl = url;
    turl.params["mode"] = "append";
    turl.params["sortidx"] = fmt::to_string(sortIdx.value_or(size_t{0}));
    turl.params["order"] = fmt::to_string(order);
    turl.params["offset"] =
        fmt::to_string(limits.offset.value_or(size_t{0}) + limits.limit.value_or(size_t{100}));

    const auto trigger =
        fmt::format(R"( hx-get="{}" hx-trigger="revealed" hx-swap="afterend")", turl);

    const auto str =
        std::views::zip(std::views::iota(size_t{1}), data) |
        std::views::transform([&](auto&& countAndItem) {
            auto&& [count, item] = countAndItem;
            auto&& [time, ip, user, name, downloads, size, age, sha] = item;
            return fmt::format(itemStr, Time{Duration{time}}, ip, user,
                               detail::link(fmt::format("/find/{}", name), name), downloads,
                               ByteSize{size}, FormatDuration{static_cast<Rep>(age)},
                               detail::link(fmt::format("/package/{}", sha), sha.substr(0, 10)),
                               (count == data.size() ? trigger : std::string{}));
        }) |
        std::views::join | std::ranges::to<std::string>();

    if (mode == Mode::Append) {
        return str;
    }

    const auto nav = detail::nav(back, {"Downloads", "/downloads"});
    const auto content = fmt::format(R"(<div>{}</div><h4>Downloads</h4>)"
                                     R"(<div class="table-responsive">)"
                                     R"(<table class="table table-hover table-sm align-middle">)"
                                     R"(<thead>{}</thead><tbody>{}</tbody></table></div>)",
                                     nav, headerRow, str);

    return detail::deliver(content, mode);
}

std::string statusData() {
    const auto fds = fp::openFileDescriptors();
    const auto threads = fp::threadCount();
    const auto mem = fp::memoryUsageBytes();
    const auto pid = fp::processId();

    static constexpr std::string_view html = R"(
        <div hx-get="/status/data" hx-trigger="every 2s" hx-swap="outerHTML">
            <dl>
                <dt>PID:</dt><dd>{}</dd>
                <dt>Open file descriptors:</dt><dd>{}</dd>
                <dt>Threads:</dt><dd>{}</dd>
                <dt>Peak memory (RSS):</dt><dd>{}</dd>
            </dl>
        </div>
    )";

    return fmt::format(html, pid, fds ? fmt::to_string(*fds) : "N/A",
                       threads ? fmt::to_string(*threads) : "N/A",
                       mem ? fmt::to_string(ByteSize{*mem}) : "N/A");
}

std::string status(Mode mode) {
    const auto nav = detail::nav({{"Packages", "/"}, {"Status", "/status"}});
    const auto content = fmt::format("<div>{}</div><h4>Process Status</h4>{}", nav, statusData());
    return detail::deliver(content, mode);
}

std::string favicon() { return std::string{html::favicon}; }
std::string maskicon() { return std::string{html::maskicon}; }

std::optional<std::pair<std::string, std::string>> script(std::string_view name) {
    // clang-format off
    static constexpr unsigned char bootstrapcss[] = {
#embed <bootstrap.min.css>
    };

    static constexpr unsigned char htmxjs[] = {
#embed <htmx.min.js>
    };
    // clang-format on

    if (name == "htmx.js") {
        const auto data = std::bit_cast<std::array<char, sizeof(htmxjs)>>(htmxjs);
        return std::optional<std::pair<std::string, std::string>>{
            std::in_place, "text/js", std::string{data.data(), data.size()}};
    }
    if (name == "bootstrap.css") {
        const auto data = std::bit_cast<std::array<char, sizeof(bootstrapcss)>>(bootstrapcss);
        return std::optional<std::pair<std::string, std::string>>{
            std::in_place, "text/js", std::string{data.data(), data.size()}};
    }

    return std::nullopt;
}

}  // namespace site

}  // namespace vcache
