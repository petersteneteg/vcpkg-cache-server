
#include <argparse/argparse.hpp>
#include <httplib.h>
#include <libzippp.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fmt/std.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <ranges>
#include <format>
#include <print>
#include <optional>
#include <utility>
#include <algorithm>
#include <numeric>

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

namespace fp {

struct StringHash {
    using hash_type = std::hash<std::string_view>;
    using is_transparent = void;

    std::size_t operator()(const char* str) const { return hash_type{}(str); }
    std::size_t operator()(std::string_view str) const { return hash_type{}(str); }
    std::size_t operator()(const std::string& str) const { return hash_type{}(str); }
};

template <typename V>
using UnorderedStringMap = std::unordered_map<std::string, V, StringHash, std::equal_to<>>;

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

std::string exceptionToString() {
    try {
        throw;
    } catch (const std::exception& e) {
        return e.what();
    } catch (...) {
        return "Unknown error";
    }
}

}  // namespace fp

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
</style></head>
<body>)";

constexpr std::string_view post = R"(</body></html>)";

constexpr std::string_view form = R"(
<form id="formElem">
  <input type="file" name="api_file" accept="text/*">
  Package: <input type="text" name="package">
  <input type="submit">
</form>
)";

constexpr std::string_view script = R"(
<script>
  formElem.onsubmit = async (e) => {
    e.preventDefault();
    let res = await fetch('/post', {
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
        auto dst = mGet(dstMap, key);
        auto src = mGet(srcMap, key);
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

const auto logger = [](const httplib::Request& req, const httplib::Response& res) {
    spdlog::trace("================================");
    spdlog::trace("{} {} {}", req.method, req.version, req.path);
    for (auto it = req.params.begin(); it != req.params.end(); ++it) {
        spdlog::trace("{}{}={}", (it == req.params.begin()) ? '?' : '&', it->first, it->second);
    }
    for (const auto& [key, val] : req.headers) {
        spdlog::trace("{}: {}", key, val);
    }
    for (const auto& [name, file] : req.files) {
        spdlog::trace("name: {}\n", name);
        spdlog::trace("filename: {}\n", file.filename);
        spdlog::trace("content type: {}\n", file.content_type);
        spdlog::trace("text length: {}\n", file.content.size());
        spdlog::trace("----------------");
    }
    spdlog::trace("--------------------------------");
    spdlog::trace("{}", res.status);
    for (const auto& [key, val] : res.headers) {
        spdlog::trace("{}: {}", key, val);
    }
};

std::string formatByteSize(size_t size) {
    if (size >= 1'000'000'000) {
        return std::format("{:5.1f}GB", static_cast<double>(size) / 1'000'000'000.0);
    } else if (size >= 1'000'000) {
        return std::format("{:5.1f}MB", static_cast<double>(size) / 1'000'000.0);
    } else if (size >= 1'000) {
        return std::format("{:5.1f}kB", static_cast<double>(size) / 1'000.0);
    } else {
        return std::format("{:4d}B", size);
    }
}

std::string formatMap(auto&& range) {
    std::string buff;
    std::format_to(std::back_inserter(buff), "<dl>\n");
    for (const auto& [key, val] : range) {
        std::format_to(std::back_inserter(buff), "<dt>{}</dt>\n", key);
        std::format_to(std::back_inserter(buff), "<dd>{}</dd>\n", val);
    }
    std::format_to(std::back_inserter(buff), "</dl>\n");
    return buff;
}

struct Info {
    std::string package{};
    std::string version{};
    std::string arch{};
    std::string sha{};
    std::map<std::string, std::string> ctrl{};
    std::map<std::string, std::string> abi{};
    std::filesystem::file_time_type time{};
    std::size_t size{};
};

Info extractInfo(const std::filesystem::path& path) {
    libzippp::ZipArchive zf{path.generic_string()};
    zf.open(libzippp::ZipArchive::ReadOnly);

    auto ctrl = zf.getEntry("CONTROL");
    if (ctrl.isNull()) {
        throw std::runtime_error{"missing CONTROL file"};
    }
    auto ctrlMap = ctrl.readAsText() | fp::splitIntoPairs('\n', ':') | std::ranges::to<std::map>();

    auto abi = zf.getEntry(
        std::format("share/{}/vcpkg_abi_info.txt", mGet(ctrlMap, "Package").value_or("?")));
    if (abi.isNull()) {
        auto entries = zf.getEntries();
        if (auto it = std::ranges::find_if(entries, fp::endsWith("vcpkg_abi_info.txt"),
                                           &libzippp::ZipEntry::getName);
            it != entries.end()) {
            abi = *it;
        } else {
            throw std::runtime_error{"missing vcpkg_abi_info.txt file"};
        }
    }
    auto abiMap = abi.readAsText() | fp::splitIntoPairs('\n', ' ') | std::ranges::to<std::map>();

    return {.package = mGet(ctrlMap, "Package").value_or("?"),
            .version = mGet(ctrlMap, "Version").value_or("?"),
            .arch = mGet(ctrlMap, "Architecture").value_or("?"),
            .sha = path.stem().generic_string(),
            .ctrl = ctrlMap,
            .abi = abiMap,
            .time = std::filesystem::last_write_time(path),
            .size = std::filesystem::file_size(path)};
}

fp::UnorderedStringMap<Info> scan(const std::filesystem::path& path,
                                  std::shared_ptr<spdlog::logger> log) {
    return std::filesystem::recursive_directory_iterator(path) | std::views::filter(fp::isZipFile) |
           fp::tryTransform(
               [&](const auto& entry) {
                   log->debug("scan: {}", entry.path().stem().generic_string());
                   return extractInfo(entry.path());
               },
               [&](const auto& entry) {
                   log->error("error scaning {} : {}", entry.path(), fp::exceptionToString());
               }) |
           std::views::transform([&](auto&& info) {
               return std::pair<const std::string, Info>{info.sha, info};
           }) |
           std::ranges::to<fp::UnorderedStringMap<Info>>();
}

class Store {
public:
    Store(const std::filesystem::path& aRoot, std::shared_ptr<spdlog::logger> aLog)
        : log{aLog}, root{aRoot}, infos{} {
        log->info("Start scan");
        infos = scan(root, log);
        log->info("Scan finished");
        log->info(statistics());
    }

    bool exists(std::string_view sha) { return std::filesystem::is_regular_file(shaToPath(sha)); }

    const Info* info(std::string_view sha) const {
        if (auto it = infos.find(sha); it != infos.end()) {
            return &it->second;
        } else {
            return nullptr;
        }
    }

    std::shared_ptr<std::ifstream> read(std::string_view sha) {
        return std::make_shared<std::ifstream>(shaToPath(sha),
                                               std::ios_base::in | std::ios_base::binary);
    }

    std::shared_ptr<std::ofstream> write(std::string_view sha) {
        return std::make_shared<std::ofstream>(shaToPath(sha),
                                               std::ios_base::out | std::ios_base::binary);
    }

    auto allInfos() const { return infos | std::views::values; }

    std::string statistics() const {
        const auto range = infos | std::views::values | std::views::transform(&Info::size);
        const auto diskSize =
            std::accumulate(std::begin(range), std::end(range), size_t{0}, std::plus<>{});
        const auto packages = infos | std::views::values | std::views::transform(&Info::package) |
                              std::ranges::to<std::set>();

        return std::format("Found {} caches of {} packages. Using {}", infos.size(),
                           packages.size(), formatByteSize(diskSize));
    }

protected:
    std::filesystem::path shaToPath(std::string_view sha) {
        return root / sha.substr(0, 2) / std::format("{}.zip", sha);
    }

    std::shared_ptr<spdlog::logger> log;
    std::filesystem::path root;
    fp::UnorderedStringMap<Info> infos;
};

struct Settings {
    int port = 8085;
    std::filesystem::path base{};
    spdlog::level::level_enum logLevel = spdlog::level::trace;
    std::vector<std::string> authTokens{};

    std::filesystem::path cert;
    std::filesystem::path key;
};

constexpr std::string_view base =
    "C:/Users/petst55.AD/OneDrive - Link\u00F6pings universitet/Cache/vcpkg-cache";

int main(int argc, char* argv[]) {
    Settings settings{};

    argparse::ArgumentParser args("vcpkg_cache_server");

    args.add_argument("--cache_dir")
        .default_value(std::string{base})
        .help("Directory where to read and write cache")
        .metavar("DIR");
    args.add_argument("--port")
        .scan<'d', int>()
        .default_value(settings.port)
        .help("Port to listen to")
        .metavar("PORT");
    args.add_argument("--verbosity")
        .scan<'d', int>()
        .default_value(static_cast<int>(settings.logLevel))
        .help("verbosity level 0-6");
    args.add_argument("--auth")
        .default_value(std::vector<std::string>{})
        .nargs(argparse::nargs_pattern::at_least_one)
        .help("List of auth tokens for write rights");
    args.add_argument("--cert").default_value("").help("Cert File").metavar("FILE");
    args.add_argument("--key").default_value("").help("Key File").metavar("FILE");

    try {
        args.parse_args(argc, argv);
        settings.base = std::filesystem::path{args.get<std::string>("--cache_dir")};
        settings.port = args.get<int>("--port");
        settings.logLevel = static_cast<spdlog::level::level_enum>(args.get<int>("--verbosity"));
        settings.authTokens = args.get<std::vector<std::string>>("--auth");
        settings.cert = std::filesystem::path{args.get<std::string>("--cert")};
        settings.key = std::filesystem::path{args.get<std::string>("--key")};

    } catch (const std::exception& err) {
        std::println("{}\n{}", err.what(), args.help().str());
        std::exit(1);
    }

    // httplib::SSLServer server{};

    auto log = spdlog::stdout_color_mt("console");
    spdlog::set_default_logger(log);
    log->set_level(settings.logLevel);

    Store store(base, log);

    httplib::Server server{};

    server.set_logger(logger);

    server.Get(R"(/cache/([0-9a-f]{64}))", [&](const httplib::Request& req,
                                               httplib::Response& res) {
        const auto sha = req.matches[1].str();

        if (!store.exists(sha)) {
            res.status = httplib::StatusCode::NotFound_404;
            return;
        }
        const auto* info = store.info(sha);

        log->info(std::format(
            "{:15} {:5}: {:30} v{:<11} {:15} Size: {:10} Time: {:%Y-%m-%d %H:%M} Auth {} Sha: {} ",
            mGet(req.headers, "REMOTE_ADDR").value_or("?"), req.method, info->package,
            info->version, info->arch, formatByteSize(info->size), info->time,
            mGet(req.headers, "Authorization").value_or("-"), sha));

        auto fstream = store.read(sha);

        res.set_content_provider(
            info->size, "application/zip",
            [fstream, buff = std::vector<char>(1024)](size_t offset, size_t length,
                                                      httplib::DataSink& sink) mutable -> bool {
                buff.resize(length);
                fstream->seekg(offset);
                fstream->read(buff.data(), length);
                sink.write(buff.data(), length);
                return true;
            });
    });

    server.Put("/cache/([0-9a-f]{64})", [&](const httplib::Request& req, httplib::Response& res,
                                            const httplib::ContentReader& content_reader) {
        auto sha = req.matches[1].str();
        if (store.exists(sha)) {
            res.status = httplib::StatusCode::Conflict_409;
            return;
        }

        log->info(std::format("{:15} {:5}:  Auth {} Sha: {} ",
                              mGet(req.headers, "REMOTE_ADDR").value_or("?"), req.method,
                              mGet(req.headers, "Authorization").value_or("-"), sha));

        auto fstream = store.write(sha);
        content_reader([fstream](const char* data, size_t data_length) {
            fstream->write(data, data_length);
            return true;
        });
    });

    server.Get("/query", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(std::format(R"({}{}<div id="result"></div>{}{})", html::pre, html::form,
                                    html::script, html::post),
                        "text/html");
    });

    server.Post("/post", [&](const httplib::Request& req, httplib::Response& res) {
        const auto api = req.get_file_value("api_file").content;
        const auto package = req.get_file_value("package").content;

        const auto abiMap = api | fp::splitIntoPairs('\n', ' ') | std::ranges::to<std::map>();

        auto matches =
            store.allInfos() |
            std::views::filter([&](const auto& info) { return info.package == package; }) |
            std::views::transform([&](const auto& info) { return info; }) |
            std::ranges::to<std::vector>();
        std::ranges::sort(matches, std::less<>{},
                          [&](const auto& info) { return missmatches(info.abi, abiMap); });

        const auto str =
            matches | std::views::take(3) | std::views::transform([&](const auto& info) {
                return std::format("<div><h3>Time: {:%Y-%m-%d %H:%M:%S} {}</h3>{}</div>", info.time,
                                   info.sha, formatDiff(abiMap, info.abi));
            }) |
            std::views::join | std::ranges::to<std::string>();

        const auto result = std::format(R"(<h1>Target ABI:</h1><div>{}</div><div>{}</div>)",
                                        formatMap(abiMap), str);

        res.set_content(result, "text/html");
    });

    server.Get(R"(/list)", [&](const httplib::Request&, httplib::Response& res) {
        const auto keys =
            store.allInfos() | std::views::transform(&Info::package) | std::ranges::to<std::set>();

        std::map<std::string, std::vector<const Info*>> packages;
        std::ranges::for_each(store.allInfos(),
                              [&](const Info& info) { packages[info.package].push_back(&info); });

        const auto str =
            packages | std::views::transform([&](const auto& package) {
                const auto range = package.second | std::views::transform([](const auto* item) {
                                       return item->size;
                                   });
                const auto diskSize =
                    std::accumulate(std::begin(range), std::end(range), size_t{0}, std::plus<>{});

                return std::format(
                    R"(<dt><a href="/find/{}"><b>{}</b></a></dt><dd><pre>Count: {:3} Size: {}</pre></dd>)",
                    package.first, package.first, package.second.size(), formatByteSize(diskSize));
            }) |
            std::views::join | std::ranges::to<std::string>();

        const auto result =
            std::format("{}<h1>Packages</h1><dl>{}</dl>{}", html::pre, str, html::post);
        res.set_content(result, "text/html");
    });

    server.Get(R"(/find/:package)", [&](const httplib::Request& req, httplib::Response& res) {
        const auto package = req.path_params.at("package");

        const auto str =
            store.allInfos() |
            std::views::filter([&](const auto& info) { return info.package == package; }) |
            std::views::transform([&](const auto& info) {
                return std::format(
                    R"(<li><a href="/package/{}"><b>{}</b> Version: {} Arch: {} Created: {:%Y-%m-%d %H:%M}</a></li>)",
                    info.sha, info.package, info.version, info.arch, info.time);
            }) |
            std::views::join | std::ranges::to<std::string>();

        const auto result =
            std::format("{}<h1>Results</h1><ol>{}</ol>{}", html::pre, str, html::post);
        res.set_content(result, "text/html");
    });

    server.Get(R"(/package/:sha)", [&](const httplib::Request& req, httplib::Response& res) {
        const auto sha = req.path_params.at("sha");

        const auto str =
            store.allInfos() |
            std::views::filter([&](const auto& info) { return info.sha == sha; }) |
            std::views::transform([&](const auto& info) {
                return std::format("<h3>{} {} {} Time: {:%Y-%m-%d %H:%M:%S}</h3>{}{}\n",
                                   info.package, info.version, info.arch, info.time,
                                   formatMap(info.ctrl), formatMap(info.abi));
            }) |
            std::views::join | std::ranges::to<std::string>();

        const auto result = std::format("{}<h1>Results</h1>{}{}", html::pre, str, html::post);
        res.set_content(result, "text/html");
    });

    server.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_content(
            std::format("<p>Error Status: <span style='color:red;'>{}</span></p>", res.status),
            "text/html");
    });
    server.set_exception_handler(
        [](const httplib::Request&, httplib::Response& res, std::exception_ptr ep) {
            try {
                std::rethrow_exception(ep);
            } catch (std::exception& e) {
                res.set_content(std::format("<h1>Error 500</h1><p>{}</p>", e.what()), "text/html");
            } catch (...) {  // See the following NOTE
                res.set_content("<h1>Error 500</h1>Unknown error</p>", "text/html");
            }
            res.status = httplib::StatusCode::InternalServerError_500;
        });

    server.Get("/favicon.svg", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content(std::string{html::favicon}, "image/svg+xml");
    });

    server.Get("/stop", [&](const httplib::Request&, httplib::Response&) { server.stop(); });

    log->info("Start server");
    server.listen("10.245.163.85", 8085);
}
