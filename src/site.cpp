#include <vcpkg-cache-server/site.hpp>
#include <vcpkg-cache-server/store.hpp>
#include <vcpkg-cache-server/functional.hpp>
#include <vcpkg-cache-server/database.hpp>

#include <vcpkg-cache-server/resources/htmx.js.hpp>
#include <vcpkg-cache-server/resources/bootstrap.css.hpp>

#include <format>
#include <ranges>
#include <algorithm>
#include <set>
#include <string>
#include <numeric>

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

std::string formatDiff(const auto& dstMap, const auto& srcMap) {
    auto keys = dstMap | std::views::keys | std::ranges::to<std::set>();
    keys.insert_range(srcMap | std::views::keys);

    std::string buff;
    std::format_to(std::back_inserter(buff), "<dl>");
    for (const auto& key : keys) {
        auto dst = fp::mGet(dstMap, key);
        auto src = fp::mGet(srcMap, key);
        if (dst && src) {
            if (*dst != *src) {
                std::format_to(std::back_inserter(buff),
                               "<dt>{}</dt><dd><ul><li><code>{}</code></"
                               "li><li><code>{}</code></li></ul></dd>\n",
                               key, *dst, *src);
            }
        } else if (dst) {
            std::format_to(std::back_inserter(buff),
                           "<dt>{}</dt><dd>Missing in source <code>{}</code></dd>\n", key, *dst);
        } else if (src) {
            std::format_to(std::back_inserter(buff),
                           "<dt>{}</dt><dd>Missing in target <code>{}</code></dd>\n", key, *src);
        }
    }
    std::format_to(std::back_inserter(buff), "</dl>");
    return buff;
}

void formatMapTo(const auto& range, std::string& buff) {
    std::format_to(std::back_inserter(buff), "<dl>\n");
    for (const auto& [key, val] : range) {
        std::format_to(std::back_inserter(buff), "<dt>{}</dt>\n", key);
        std::format_to(std::back_inserter(buff), "<dd>{}</dd>\n", val);
    }
    std::format_to(std::back_inserter(buff), "</dl>\n");
}

std::string formatMap(const auto& range) {
    std::string buff;
    formatMapTo(range, buff);
    return buff;
}

void formatInfoTo(const Info& info, std::string& buff) {
    std::format_to(std::back_inserter(buff),
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
           hx-get="{0}?plain=1&sort={1}&order={2}"
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

    return std::format(str, url, tag, newOrder, content, indicator);
}

std::string navItem(std::string_view name, std::string_view url, bool active) {
    static constexpr std::string_view str = R"(
        <li class="breadcrumb-item {0}">
            <a class="pointer link-underline 
                      link-offset-2-hover 
                      link-underline-opacity-0 
                      link-underline-opacity-75-hover" 
               hx-get="{1}?plain=1" 
               hx-target="#content" 
               hx-swap="innerHTML" 
               hx-push-url={1}>
                {2}
            </a>
        </li>
    )";

    return std::format(str, active ? "active" : "", url, name);
}

}  // namespace

std::string detail::nav(const std::vector<std::pair<std::string, std::string>>& path) {
    const auto str = path | std::views::transform([i = size_t{0}, &path](const auto& item) mutable {
                         ++i;
                         return navItem(item.first, item.second, i == path.size());
                     }) |
                     std::views::join | std::ranges::to<std::string>();

    return std::format(R"(
    <nav>
        <ol class="breadcrumb fs-4">
            {}
        </ol>
    </nav>
    )",
                       str);
}

std::string detail::deliver(std::string_view content, Mode mode) {
    if (mode == Mode::Plain) {
        return std::string{content};
    } else if (mode == Mode::Full) {
        return std::format(html::index, html::style, content);
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

std::string index(const Store& store, db::Database& db, Mode mode, Sort sort, Order order,
                  std::string_view search) {
    const auto keys =
        store.allInfos() | std::views::transform(&Info::package) | std::ranges::to<std::set>();

    std::map<std::string, std::vector<const Info*>> packages;
    std::ranges::for_each(store.allInfos(),
                          [&](const Info& info) { packages[info.package].push_back(&info); });

    rapidfuzz::fuzz::CachedPartialRatio<char> scorer(search);
    auto list = packages | std::views::transform([&](const auto& package) -> RowItem {
                    auto& [name, items] = package;
                    const auto range =
                        items | std::views::transform([](const auto* item) { return item->size; });
                    const auto diskSize = std::accumulate(std::begin(range), std::end(range),
                                                          size_t{0}, std::plus<>{});

                    const auto [firstIt, lastIt] = std::ranges::minmax_element(
                        items, std::less<>{}, [](const Info* i) { return i->time; });
                    const auto similarity = search.empty() ? 1.0 : scorer.similarity(name);

                    const auto [downloads, lastUse] = db::getPackageDownloadsAndLastUse(db, name);

                    return {name,    items.size(),     diskSize,        downloads,
                            lastUse, (*firstIt)->time, (*lastIt)->time, similarity};
                }) |
                std::views::filter([&](const RowItem& item) {
                    return search.empty() ? true : item.similarity > 55.0;
                }) |
                std::ranges::to<std::vector>();

    if (order == Order::Ascending) {
        switch (sort) {
            case Sort::Name:
                std::ranges::sort(list, std::less<>{}, &RowItem::name);
                break;
            case Sort::Count:
                std::ranges::sort(list, std::less<>{}, &RowItem::count);
                break;
            case Sort::Size:
                std::ranges::sort(list, std::less<>{}, &RowItem::diskSize);
                break;
            case Sort::First:
                std::ranges::sort(list, std::less<>{}, &RowItem::firstTime);
                break;
            case Sort::Last:
                std::ranges::sort(list, std::less<>{}, &RowItem::lastTime);
                break;
            case Sort::Downloads:
                std::ranges::sort(list, std::less<>{}, &RowItem::downloads);
                break;
            case Sort::Use:
                std::ranges::sort(list, std::less<>{}, &RowItem::lastUse);
                break;
            case Sort::Default:
                break;
        }
    } else {
        switch (sort) {
            case Sort::Name:
                std::ranges::sort(list, std::greater<>{}, &RowItem::name);
                break;
            case Sort::Count:
                std::ranges::sort(list, std::greater<>{}, &RowItem::count);
                break;
            case Sort::Size:
                std::ranges::sort(list, std::greater<>{}, &RowItem::diskSize);
                break;
            case Sort::First:
                std::ranges::sort(list, std::greater<>{}, &RowItem::firstTime);
                break;
            case Sort::Last:
                std::ranges::sort(list, std::greater<>{}, &RowItem::lastTime);
                break;
            case Sort::Downloads:
                std::ranges::sort(list, std::greater<>{}, &RowItem::downloads);
                break;
            case Sort::Use:
                std::ranges::sort(list, std::greater<>{}, &RowItem::lastUse);
                break;
            case Sort::Default:
                break;
        }
    }
    if (sort == Sort::Default && !search.empty()) {
        std::ranges::sort(list, std::greater<>{}, &RowItem::similarity);
    }

    static constexpr std::string_view itemStr = R"(
        <div class="row">
            <div class="col">
                <button class="btn btn-link btn-sm"
                        hx-get="/find/{0}?plain=1" hx-target="#content" 
                        hx-swap="innerHTML" hx-push-url="/find/{0}">
                    <b>{0}</b>
                </button>
            </div>
            <div class="col">{1}</div>
            <div class="col">{2:M}</div>
            <div class="col">{3}</div>
            <div class="col">{4}</div>
            <div class="col">{5:%Y-%m-%d %H:%M}</div>
            <div class="col">{6:%Y-%m-%d %H:%M}</div>
        </div>
    )";

    const auto str = list | std::views::transform([&](const RowItem& item) {
                         const auto lastUse = db::formatTimeStamp(item.lastUse);
                         return std::format(itemStr, item.name, item.count, ByteSize{item.diskSize},
                                            item.downloads, lastUse, item.firstTime, item.lastTime);
                     }) |
                     std::views::join | std::ranges::to<std::string>();

    const auto totalSize = std::ranges::fold_left(
        list | std::views::transform([&](const RowItem& item) { return item.diskSize; }), size_t{0},
        std::plus<>{});
    const auto totalCount = std::ranges::fold_left(
        list | std::views::transform([&](const RowItem& item) { return item.count; }), size_t{0},
        std::plus<>{});

    const auto stats = std::format("Found {} caches of {} packages. Using {}", totalCount,
                                   list.size(), ByteSize{totalSize});

    const auto nameButton = button("/", "Package", Sort::Name, sort, order);
    const auto countButton = button("/", "Count", Sort::Count, sort, order);
    const auto sizeButton = button("/", "Size", Sort::Size, sort, order);

    const auto downloadsButton = button("/", "Downloads", Sort::Downloads, sort, order);
    const auto useButton = button("/", "Last Use", Sort::Use, sort, order);

    const auto firstButton = button("/", "First Cache", Sort::First, sort, order);
    const auto lastButton = button("/", "Last Cache", Sort::Last, sort, order);

    const auto headerRow = std::format(R"(
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
        {0}
        <input class="form-control"
               id="search"
               type="search"
               name="search"
               value="{1}"
               placeholder="Search Packages..."
               hx-get="?plain=1" 
               hx-target="#content" 
               hx-swap="innerHTML"
               hx-trigger="input changed delay:500ms, keyup[key=='Enter']"
               hx-indicator=".htmx-indicator">
        <h4>{2}</h4>
        <span class="htmx-indicator">Searching...</span>
        <div class="container text-left align-middle">
            {3}
            {4}
        </div>
    )";
    const auto content = std::format(html, nav, search, stats, headerRow, str);

    return detail::deliver(content, mode);
}

std::string match() {
    return std::format(R"({}{}<div id="result"></div>{}{})", html::pre, html::form, html::script,
                       html::post);
}

std::string match(std::string_view abi, std::string_view package, const Store& store) {
    const auto abiMap = abi | fp::splitIntoPairs('\n', ' ') | std::ranges::to<std::map>();

    auto matches = store.allInfos() |
                   std::views::filter([&](const auto& info) { return info.package == package; }) |
                   std::views::transform([&](const auto& info) { return info; }) |
                   std::ranges::to<std::vector>();
    std::ranges::sort(matches, std::less<>{},
                      [&](const auto& info) { return missmatches(info.abi, abiMap); });

    const auto str = matches | std::views::take(3) | std::views::transform([&](const auto& info) {
                         return std::format("<div><h3>Time: {:%Y-%m-%d %H:%M:%S} {}</h3>{}</div>",
                                            info.time, info.sha, formatDiff(abiMap, info.abi));
                     }) |
                     std::views::join | std::ranges::to<std::string>();

    return std::format(R"(<h1>Target ABI:</h1><div>{}</div><div>{}</div>)", formatMap(abiMap), str);
}

std::string compare(std::string_view sha, const Store& store, Mode mode) {
    const auto* targetInfo = store.info(sha);
    if (!targetInfo) {
        return detail::deliver(std::format("<h1>Error</h1><div>Sha: {} not found</div>", sha),
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
                      [&](const auto& info) { return missmatches(info.abi, abiMap); });

    const auto str = matches | std::views::take(5) | std::views::transform([&](const auto& info) {
                         return std::format("<div><h3>Time: {:%Y-%m-%d %H:%M:%S} {}</h3>{}</div>",
                                            info.time, info.sha, formatDiff(abiMap, info.abi));
                     }) |
                     std::views::join | std::ranges::to<std::string>();

    const auto nav =
        detail::nav({{"Packages", "/"},
                     {targetInfo->package, std::format("/find/{}", targetInfo->package)},
                     {targetInfo->sha, std::format("/package/{}", targetInfo->sha)},
                     {"Compare", std::format("/compare/{}", targetInfo->sha)}});

    return detail::deliver(std::format("{}{}<div>{}</div>", nav, formatInfo(*targetInfo), str),
                           mode);
}

std::string find(std::string_view package, const Store& store, Mode mode) {
    auto list = store.allInfos() |
                std::views::filter([&](const auto& info) { return info.package == package; }) |
                std::ranges::to<std::vector>();

    std::ranges::sort(list, std::greater<>{}, &Info::time);

    const auto str = list | std::views::transform([&](const auto& info) {
                         return std::format(
                             "<li><pre>Version: {0:10} Arch: {1:25} "
                             "Size: {2:14} Created: {3:%Y-%m-%d %H:%M} "
                             "SHA: </pre>"
                             R"(<button hx-get="/package/{4}?plain=1" hx-target="#content")"
                             R"(  hx-swap="innerHTML"  hx-push-url="/package/{4}"> )"
                             R"(    <pre>{4}</pre>)"
                             R"(</button>)"
                             R"(<button hx-get="/compare/{4}?plain=1" hx-target="#content")"
                             R"(  hx-swap="innerHTML"  hx-push-url="/compare/{4}"> )"
                             R"(    Compare)"
                             R"(</button></li>)",
                             info.version, info.arch, ByteSize{info.size}, info.time, info.sha);
                     }) |
                     std::views::join | std::ranges::to<std::string>();

    const auto sizes =
        store.allInfos() |
        std::views::filter([&](const auto& info) { return info.package == package; }) |
        std::views::transform(&Info::size) | std::ranges::to<std::vector>();

    const auto count = sizes.size();
    const auto diskSize = std::accumulate(sizes.begin(), sizes.end(), size_t{0}, std::plus<>());

    const auto nav =
        detail::nav({{"Packages", "/"}, {std::string{package}, std::format("/find/{}", package)}});
    const auto content = std::format("{}<h4>Count: {}, Total Size: {}</h4><ol>{}</ol>", nav, count,
                                     ByteSize{diskSize}, str);

    return detail::deliver(content, mode);
}

std::string sha(std::string_view sha, const Store& store, Mode mode) {
    const auto* info = store.info(sha);
    if (!info) {
        return detail::deliver(std::format("<h1>Error</h1><div>Sha: {} not found</div>", sha),
                               mode);
    } else {
        const auto finfo = formatInfo(*info);
        const auto nav = detail::nav({{"Packages", "/"},
                                      {info->package, std::format("/find/{}", info->package)},
                                      {info->sha, std::format("/package/{}", info->sha)}});

        return detail::deliver(std::format("{}{}", nav, finfo), mode);
    }
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
