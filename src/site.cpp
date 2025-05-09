#include <vcpkg-cache-server/site.hpp>
#include <vcpkg-cache-server/store.hpp>
#include <vcpkg-cache-server/functional.hpp>
#include <vcpkg-cache-server/database.hpp>

#include <vcpkg-cache-server/resources/htmx.js.hpp>
#include <vcpkg-cache-server/resources/bootstrap.css.hpp>

#include <fmt/format.h>
#include <fmt/std.h>
#include <fmt/chrono.h>

#include <ranges>
#include <algorithm>
#include <set>
#include <string>
#include <numeric>
#include <tuple>

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

namespace {

size_t missmatches(const auto& map1, const auto& map2) {
    auto keys = fp::fromRange<std::set<std::string>>(map1 | std::views::keys);

    const auto keys2 = map2 | std::views::keys;
    keys.insert(keys2.begin(), keys2.end());

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

std::string formatDiff(const auto& dstMap, const auto& srcMap) {
    auto keys = fp::fromRange<std::set<std::string>>(dstMap | std::views::keys);
    const auto srcKeys = srcMap | std::views::keys;
    keys.insert(srcKeys.begin(), srcKeys.end());

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

void formatMapTo(const auto& range, std::string& buff) {
    fmt::format_to(std::back_inserter(buff), "<dl>\n");
    for (const auto& [key, val] : range) {
        fmt::format_to(std::back_inserter(buff), "<dt>{}</dt>\n", key);
        fmt::format_to(std::back_inserter(buff), "<dd>{}</dd>\n", val);
    }
    fmt::format_to(std::back_inserter(buff), "</dl>\n");
}

std::string formatMap(const auto& range) {
    std::string buff;
    formatMapTo(range, buff);
    return buff;
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

std::string button(std::string_view url, std::string_view content, Sort tag, Sort currentSort,
                   Order currentOrder) {
    static constexpr std::string_view str = R"(
        <a class="pointer link-underline 
                  link-offset-2-hover 
                  link-underline-opacity-0 
                  link-underline-opacity-75-hover" 
           hx-get="{0}?mode=plain&sort={1}&order={2}"
           hx-target="#content" 
           hx-swap="innerHTML" 
           hx-push-url="{0}?sort={1}&order={2}">
            {3}{4}
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

    return fmt::format(str, url, tag, newOrder, content, indicator);
}

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

std::string downloadsLink(Params params) {
    Url purl{.path = "/downloads", .params = {{"mode", "plain"}}};
    Url furl{.path = "/downloads", .params = {{"mode", "full"}}};
    purl.params.insert(params.begin(), params.end());
    furl.params.insert(params.begin(), params.end());

    constexpr std::string_view str = R"(
        <div class="d-inline-block float-end fs-4">
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

}  // namespace

std::string detail::nav(const std::vector<std::pair<std::string, std::string>>& path) {
    const auto str = fp::fromRange<std::string>(
        path | std::views::transform([i = size_t{0}, &path](const auto& item) mutable {
            ++i;
            return navItem(item.first, item.second, i == path.size());
        }) |
        std::views::join | std::views::common);

    return fmt::format(R"(<nav class="d-inline-block"><ol class="breadcrumb fs-4">{}</ol></nav>)",
                       str);
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

std::string index(const Store& store, db::Database& db, Mode mode, Sort sort,
                  std::optional<Order> maybeOrder, std::string_view search) {
    const auto order = maybeOrder.value_or(Order::Ascending);
    const auto keys = fp::fromRange<std::set<std::string>>(store.allInfos() |
                                                           std::views::transform(&Info::package));

    std::map<std::string, std::vector<const Info*>> packages;
    std::ranges::for_each(store.allInfos(),
                          [&](const Info& info) { packages[info.package].push_back(&info); });

    rapidfuzz::fuzz::CachedPartialRatio<char> scorer(search);
    auto list = fp::fromRange<std::vector<RowItem>>(
        packages | std::views::transform([&](const auto& package) -> RowItem {
            auto& [name, items] = package;
            const auto range =
                items | std::views::transform([](const auto* item) { return item->size; });
            const auto diskSize =
                std::accumulate(std::begin(range), std::end(range), size_t{0}, std::plus<>{});

            const auto [firstIt, lastIt] = std::ranges::minmax_element(
                items, std::less<>{}, [](const Info* i) { return i->time; });
            const auto similarity = search.empty() ? 1.0 : scorer.similarity(name);

            const auto [downloads, lastUse] = db::getPackageDownloadsAndLastUse(db, name);

            return {name,    items.size(),     diskSize,        downloads,
                    lastUse, (*firstIt)->time, (*lastIt)->time, similarity};
        }) |
        std::views::filter(
            [&](const RowItem& item) { return search.empty() ? true : item.similarity > 55.0; }));

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

    table[std::to_underlying(sort)](list, order);

    if (sort == Sort::Default && !search.empty()) {
        std::ranges::sort(list, std::greater<>{}, &RowItem::similarity);
    }

    static constexpr std::string_view itemStr = R"(
        <div class="row">
            <div class="col">
                <button class="btn btn-link btn-sm"
                        hx-get="/find/{0}?mode=plain" hx-target="#content" 
                        hx-swap="innerHTML" hx-push-url="/find/{0}">
                    <b>{0}</b>
                </button>
            </div>
            <div class="col">{1}</div>
            <div class="col">{2:M}</div>
            <div class="col">{3}</div>
            <div class="col">{4:%Y-%m-%d %H:%M}</div>
            <div class="col">{5:%Y-%m-%d %H:%M}</div>
            <div class="col">{6:%Y-%m-%d %H:%M}</div>
        </div>
    )";

    const auto str = fp::fromRange<std::string>(
        list | std::views::transform([&](const RowItem& item) {
            return fmt::format(itemStr, item.name, item.count, ByteSize{item.diskSize},
                               item.downloads, item.lastUse, item.firstTime, item.lastTime);
        }) |
        std::views::join | std::views::common);

    const auto totalSize = std::ranges::fold_left(
        list | std::views::transform([&](const RowItem& item) { return item.diskSize; }), size_t{0},
        std::plus<>{});
    const auto totalCount = std::ranges::fold_left(
        list | std::views::transform([&](const RowItem& item) { return item.count; }), size_t{0},
        std::plus<>{});

    const auto stats = fmt::format("Found {} caches of {} packages. Using {}", totalCount,
                                   list.size(), ByteSize{totalSize});

    const auto nameButton = button("/", "Package", Sort::Name, sort, order);
    const auto countButton = button("/", "Count", Sort::Count, sort, order);
    const auto sizeButton = button("/", "Size", Sort::Size, sort, order);
    const auto downloadsButton = button("/", "Downloads", Sort::Downloads, sort, order);
    const auto useButton = button("/", "Last Use", Sort::Use, sort, order);
    const auto firstButton = button("/", "First Cache", Sort::First, sort, order);
    const auto lastButton = button("/", "Last Cache", Sort::Last, sort, order);

    const auto headerRow = fmt::format(R"(
            <div class="row">
                <div class="col">{}</div>
                <div class="col">{}</div>
                <div class="col">{}</div>
                <div class="col">{}</div>
                <div class="col">{}</div>
                <div class="col">{}</div>
                <div class="col">{}</div>
            </div>
            )",
                                       nameButton, countButton, sizeButton, downloadsButton,
                                       useButton, firstButton, lastButton);

    const auto nav = detail::nav({{"Packages", "/"}});

    static constexpr std::string_view html = R"(
        <div>{0}{1}</div>
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
        <div class="container text-left align-middle">
            {4}
            {5}
        </div>
    )";
    const auto content = fmt::format(html, nav, downloadsLink({}), search, stats, headerRow, str);

    return detail::deliver(content, mode);
}

std::string match() {
    return fmt::format(R"({}{}<div id="result"></div>{}{})", html::pre, html::form, html::script,
                       html::post);
}

std::string match(std::string_view abi, std::string_view package, const Store& store) {
    const auto abiMap =
        fp::fromRange<std::map<std::string, std::string>>(abi | fp::splitIntoPairs('\n', ' '));

    auto matches = fp::fromRange<std::vector<Info>>(
        store.allInfos() |
        std::views::filter([&](const auto& info) { return info.package == package; }));
    std::ranges::sort(matches, std::less<>{},
                      [&](const auto& info) { return missmatches(info.abi, abiMap); });

    const auto str = fp::fromRange<std::string>(
        matches | std::views::take(3) | std::views::transform([&](const auto& info) {
            return fmt::format("<div><h3>Time: {:%Y-%m-%d %H:%M:%S} {}</h3>{}</div>", info.time,
                               info.sha, formatDiff(abiMap, info.abi));
        }) |
        std::views::join | std::views::common);

    return fmt::format(R"(<h1>Target ABI:</h1><div>{}</div><div>{}</div>)", formatMap(abiMap), str);
}

std::string compare(std::string_view sha, const Store& store, Mode mode) {
    const auto* targetInfo = store.info(sha);
    if (!targetInfo) {
        return detail::deliver(fmt::format("<h1>Error</h1><div>Sha: {} not found</div>", sha),
                               mode);
    }

    const auto& abiMap = targetInfo->abi;
    const auto& package = targetInfo->package;

    auto matches = fp::fromRange<std::vector<Info>>(
        store.allInfos() | std::views::filter([&](const auto& info) { return info.sha != sha; }) |
        std::views::filter([&](const auto& info) { return info.package == package; }));
    std::ranges::sort(matches, std::less<>{},
                      [&](const auto& info) { return missmatches(info.abi, abiMap); });

    const auto str = fp::fromRange<std::string>(
        matches | std::views::take(5) | std::views::transform([&](const auto& info) {
            return fmt::format("<div><h3>Time: {:%Y-%m-%d %H:%M:%S} {}</h3>{}</div>", info.time,
                               info.sha, formatDiff(abiMap, info.abi));
        }) |
        std::views::join | std::views::common);

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

std::string find(std::string_view package, const Store& store, db::Database& db, Mode mode,
                 Sort sort, std::optional<Order> maybeOrder) {
    const auto order = maybeOrder.value_or(Order::Ascending);

    auto list = fp::fromRange<std::vector<CacheItem>>(
        store.allInfos() |
        std::views::filter([&](const auto& info) { return info.package == package; }) |
        std::views::transform([&](const auto& info) -> CacheItem {
            const auto [downloads, lastUse] = db::getCacheDownloadsAndLastUse(db, info.sha);
            return {.version = info.version,
                    .arch = info.arch,
                    .diskSize = info.size,
                    .downloads = downloads,
                    .lastUse = lastUse,
                    .created = info.time,
                    .sha = info.sha};
        }));

    const auto archs =
        fp::fromRange<std::set<std::string>>(list | std::views::transform(&CacheItem::arch));
    // Todo make selectable

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
    table[std::to_underlying(sort)](list, order);

    const auto path = fmt::format("/find/{0}?mode=plain", package);
    const auto versionButton = button(path, "Version", Sort::Version, sort, order);
    const auto archButton = button(path, "Arch", Sort::Arch, sort, order);
    const auto sizeButton = button(path, "Size", Sort::Size, sort, order);
    const auto downloadsButton = button(path, "Downloads", Sort::Downloads, sort, order);
    const auto useButton = button(path, "Last Use", Sort::Use, sort, order);
    const auto firstButton = button(path, "Created", Sort::First, sort, order);
    const auto shaButton = button(path, "SHA", Sort::SHA, sort, order);

    const auto headerRow = fmt::format(R"(
            <div class="row">
                <div class="col">{}</div>
                <div class="col">{}</div>
                <div class="col">{}</div>
                <div class="col">{}</div>
                <div class="col">{}</div>
                <div class="col">{}</div>
                <div class="col">{}</div>
                <div class="col"></div>
            </div>
            )",
                                       versionButton, archButton, sizeButton, downloadsButton,
                                       useButton, firstButton, shaButton);

    static constexpr std::string_view itemStr = R"(
        <div class="row">
            <div class="col">{0}</div>
            <div class="col">{1}</div>
            <div class="col">{2}</div>
            <div class="col">{3}</div>
            <div class="col">{4:%Y-%m-%d %H:%M}</div>
            <div class="col">{5:%Y-%m-%d %H:%M}</div>
            <div class="col">
                <button hx-get="/package/{6}?mode=plain" 
                        hx-target="#content"
                        hx-swap="innerHTML"  
                        hx-push-url="/package/{6}"> 
                    <pre>{7}</pre>
                </button>
            </div>
            <div class="col">
                <button hx-get="/compare/{6}?mode=plain" 
                        hx-target="#content"
                        hx-swap="innerHTML" 
                        hx-push-url="/compare/{6}">
                    Compare
                </button>
            </div>
        </div>
    )";

    const auto str = fp::fromRange<std::string>(
        list | std::views::transform([&](const CacheItem& item) {
            return fmt::format(itemStr, item.version, item.arch, ByteSize{item.diskSize},
                               item.downloads, item.lastUse, item.created, item.sha,
                               item.sha.substr(0, 15));
        }) |
        std::views::join | std::views::common);

    const auto sizes = fp::fromRange<std::vector<size_t>>(
        store.allInfos() |
        std::views::filter([&](const auto& info) { return info.package == package; }) |
        std::views::transform(&Info::size));

    const auto count = sizes.size();
    const auto diskSize = std::accumulate(sizes.begin(), sizes.end(), size_t{0}, std::plus<>());

    const auto nav =
        detail::nav({{"Packages", "/"}, {std::string{package}, fmt::format("/find/{}", package)}});
    const auto content =
        fmt::format(R"(<div>{}{}</div><h4>Count: {}, Total Size: {}</h4>)"
                    R"(<div class="container text-left align-middle">{}{}</div>)",
                    nav, downloadsLink({{"selcol", "name"}, {"selval", std::string{package}}}),
                    count, ByteSize{diskSize}, headerRow, str);

    return detail::deliver(content, mode);
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

    return detail::deliver(
        fmt::format("<div>{}{}</div>{}", nav,
                    downloadsLink({{"selcol", "sha"}, {"selval", info->sha}}), finfo),
        mode);
}

struct AgeAlias : sqlite_orm::alias_tag {
    static const std::string& get() {
        static const std::string res = "Age";
        return res;
    }
};

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
                      std::optional<std::pair<Sort, std::string>> selection) {

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

    constexpr std::array<std::string_view, cols.count> widths{"",   "",   "-1", "",
                                                              "-1", "-1", "",   "-1"};
    names[6] = "age";

    Url url{.path = "/downloads", .params = {}};
    if (selection) {
        url.params["selcol"] = fmt::to_string(selection->first);
        url.params["selval"] = selection->second;
    }

    const auto header = [&]<size_t... Is>(std::integer_sequence<size_t, Is...>) {
        return std::array{[&]() {
            const auto button = buttonIdx(url, names[Is], Is, sortIdx.value_or(size_t{0}),
                                          order.value_or(Order::Descending));
            return fmt::format(R"(<div class="col{}">{}</div>)", widths[Is], button);
        }()...};
    }
    (std::make_integer_sequence<size_t, cols.count>());
    const auto headerRow = fmt::format(R"(<div class="row">{}</div>)",
                                       fp::fromRange<std::string>(header | std::views::join));

    static constexpr std::string_view itemStr = R"(
        <div class="row" {8}>
            <div class="col">{0:%Y-%m-%d %H:%M}</div>
            <div class="col">{1}</div>
            <div class="col-1">{2}</div>
            <div class="col">{3}</div>
            <div class="col-1">{4}</div>
            <div class="col-1">{5}</div>
            <div class="col">{6}</div>
            <div class="col-1">{7}</div>
        </div>
        )";

    auto turl = url;
    turl.params["mode"] = "append";
    turl.params["sortidx"] = fmt::to_string(sortIdx.value_or(size_t{0}));
    turl.params["order"] = fmt::to_string(order);
    turl.params["offset"] =
        fmt::to_string(limits.offset.value_or(size_t{0}) + limits.limit.value_or(size_t{100}));

    const auto trigger =
        fmt::format(R"( hx-get="{}" hx-trigger="revealed" hx-swap="afterend")", turl);

    const auto str = fp::fromRange<std::string>(
        std::views::zip(std::views::iota(size_t{1}), data) |
        std::views::transform([&](auto&& countAndItem) {
            auto&& [count, item] = countAndItem;
            auto&& [time, ip, user, name, downloads, size, age, sha] = item;
            return fmt::format(itemStr, Time{Duration{time}}, ip, user,
                               link(fmt::format("/find/{}", name), name), downloads, ByteSize{size},
                               FormatDuration{static_cast<Rep>(age)},
                               link(fmt::format("/package/{}", sha), sha.substr(0, 10)),
                               (count == data.size() ? trigger : std::string{}));
        }) |
        std::views::join | std::views::common);

    if (mode == Mode::Append) {
        return str;
    }

    const auto content = fmt::format(R"(<h4>Downloads</h4>)"
                                     R"(<div class="container text-left align-middle">{}{}</div>)",
                                     headerRow, str);

    return detail::deliver(content, mode);
}

std::string favicon() { return std::string{html::favicon}; }
std::string maskicon() { return std::string{html::maskicon}; }

std::optional<std::pair<std::string, std::string>> script(std::string_view name) {
    if (name == "htmx.js") {
        return std::optional<std::pair<std::string, std::string>>{std::in_place, "text/js",
                                                                  html::htmxjs};
    }
    if (name == "bootstrap.css") {
        return std::optional<std::pair<std::string, std::string>>{std::in_place, "text/css",
                                                                  html::bootstrap};
    }

    return std::nullopt;
}

}  // namespace site

}  // namespace vcache
