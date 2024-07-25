#include <vcpkg-cache-server/site.hpp>
#include <vcpkg-cache-server/store.hpp>
#include <vcpkg-cache-server/functional.hpp>

#include <format>
#include <ranges>
#include <algorithm>
#include <set>
#include <string>
#include <numeric>

namespace vcache {

namespace html {
constexpr std::string_view pre = R"(
<html><head>
    <link rel="icon" type="image/svg+xml" href="favicon.svg" />
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
</style></head>
<body>)";

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

}  // namespace html

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

}  // namespace

namespace site {

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

std::string compare(std::string_view sha, const Store& store) {
    const auto* targetInfo = store.info(sha);
    if (!targetInfo) {
        return std::format("{}<h1>Error</h1><div>Sha: {} not found</div>", html::pre, sha,
                           html::post);
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

    return std::format(R"({}{}<div>{}</div>{})", html::pre, formatInfo(*targetInfo), str,
                       html::post);
}

std::string list(const Store& store) {
    const auto keys =
        store.allInfos() | std::views::transform(&Info::package) | std::ranges::to<std::set>();

    std::map<std::string, std::vector<const Info*>> packages;
    std::ranges::for_each(store.allInfos(),
                          [&](const Info& info) { packages[info.package].push_back(&info); });

    const auto str =
        packages | std::views::transform([&](const auto& package) {
            const auto range =
                package.second | std::views::transform([](const auto* item) { return item->size; });
            const auto diskSize =
                std::accumulate(std::begin(range), std::end(range), size_t{0}, std::plus<>{});

            return std::format(
                "<dt><a href=\"/find/{}\"><b>{}</b></a></dt><dd><pre>Count: {:3} Size: "
                "{}</pre></dd>",
                package.first, package.first, package.second.size(), ByteSize{diskSize});
        }) |
        std::views::join | std::ranges::to<std::string>();

    return std::format("{}<h1>Packages</h1><div>{}</div><dl>{}</dl>{}", html::pre,
                       store.statistics(), str, html::post);
}

std::string find(std::string_view package, const Store& store) {
    auto list = store.allInfos() |
                std::views::filter([&](const auto& info) { return info.package == package; }) |
                std::ranges::to<std::vector>();
    
    std::ranges::sort(list, std::greater<>{}, &Info::time);
    
    const auto str = list |
                     std::views::transform([&](const auto& info) {
                         return std::format(
                             "<li><pre>Version: {:10} Arch: {:25} "
                             "Size: {:14} Created: {:%Y-%m-%d %H:%M} "
                             "SHA: </pre><a href=\"/package/{}\"><pre>{}</pre></a>"
                             "<a href=\"/compare/{}\"><pre> diff</pre></a></li>",
                             info.version, info.arch, ByteSize{info.size}, info.time,
                             info.sha, info.sha, info.sha);
                     }) |
                     std::views::join | std::ranges::to<std::string>();

    const auto sizes =
        store.allInfos() |
        std::views::filter([&](const auto& info) { return info.package == package; }) |
        std::views::transform(&Info::size) | std::ranges::to<std::vector>();

    const auto count = sizes.size();
    const auto diskSize = std::accumulate(sizes.begin(), sizes.end(), size_t{0}, std::plus<>());

    return std::format("{}<h1>{}</h1>Count: {}, Total Size: {}\n<ol>{}</ol>{}", html::pre, package,
                       count, ByteSize{diskSize}, str, html::post);
}

std::string sha(std::string_view sha, const Store& store) {
    const auto* info = store.info(sha);
    if (!info) {
        return std::format("{}<h1>Error</h1><div>Sha: {} not found</div>", html::pre, sha,
                           html::post);
    }
    return std::format("{}{}{}", html::pre, formatInfo(*info), html::post);
}

std::string favicon() { return std::string{html::favicon}; }

}  // namespace site

}  // namespace vcache
