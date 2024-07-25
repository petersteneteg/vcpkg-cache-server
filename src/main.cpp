
#include <vcpkg-cache-server/settings.hpp>
#include <vcpkg-cache-server/store.hpp>
#include <vcpkg-cache-server/site.hpp>
#include <vcpkg-cache-server/functional.hpp>

#include <httplib.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

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

namespace vcache {

auto logger(std::shared_ptr<spdlog::logger> log) {
    return [log](const httplib::Request& req, const httplib::Response& res) {
        log->debug("{:5} {:15} Status: {:4} Vers: {:8} Path: {}", req.method,
                   fp::mGet(req.headers, "REMOTE_ADDR").value_or("?.?.?.?"), res.status,
                   req.version, req.path);
        for (const auto& [key, val] : req.params) {
            log->trace("{:>20}: {}", key, val);
        }
        for (const auto& [key, val] : req.headers) {
            log->trace("{:>20}: {}", key, val);
        }
        for (const auto& [name, file] : req.files) {
            log->trace("{:>20}: {}", "name", name);
            log->trace("{:>20}: {}", "filename", file.filename);
            log->trace("{:>20}: {}", "content type", file.content_type);
            log->trace("{:>20}: {}", "text length", file.content.size());
        }
        for (const auto& [key, val] : res.headers) {
            log->trace("{:>20}: {}", key, val);
        }
    };
}

std::pair<std::string_view, std::string_view> parseAuthHeader(std::string_view authHeader) {
    auto [scheme, token] = fp::splitByFirst(authHeader);
    scheme = fp::trim(scheme);
    token = fp::trim(token);
    return {scheme, token};
}

httplib::Server::HandlerWithContentReader authorizeRequest(
    const Authorization& auth, httplib::Server::HandlerWithContentReader handler) {
    return [handler, &auth](const httplib::Request& req, httplib::Response& res,
                            const httplib::ContentReader& content_reader) {
        if (!req.has_header("Authorization")) {
            res.set_header("WWW-Authenticate", "Bearer");
            res.status = httplib::StatusCode::Unauthorized_401;
            return;
        }

        const auto authHeader = req.get_header_value("Authorization");
        const auto [scheme, token] = parseAuthHeader(authHeader);

        if (scheme != "Bearer" || !auth.write.contains(token)) {
            res.set_header("WWW-Authenticate", "Bearer");
            res.status = httplib::StatusCode::Forbidden_403;
            return;
        }

        handler(req, res, content_reader);
    };
}

std::shared_ptr<spdlog::logger> createLog(spdlog::level::level_enum logLevel,
                                          const std::optional<std::filesystem::path>& logFile) {
    if (logFile) {
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_level(logLevel);
        auto fileSink =
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile->generic_string(), false);
        fileSink->set_level(spdlog::level::trace);
        auto log =
            std::make_shared<spdlog::logger>("log", spdlog::sinks_init_list{consoleSink, fileSink});
        log->set_level(spdlog::level::trace);
        return log;
    } else {
        auto log = spdlog::stdout_color_mt("log");
        log->set_level(logLevel);
        return log;
    }
}

std::unique_ptr<httplib::Server> createServer(
    const std::optional<std::pair<std::filesystem::path, std::filesystem::path>>& certAndKey) {
    if (certAndKey) {
        return std::make_unique<httplib::SSLServer>(certAndKey->first.generic_string().c_str(),
                                                    certAndKey->second.generic_string().c_str());
    } else {
        return std::make_unique<httplib::Server>();
    }
}

std::string logCache(const httplib::Request& req, const Info& info, const Authorization& auth) {
    const auto authHeader = fp::mGet(req.headers, "Authorization");
    const auto [scheme, token] =
        authHeader.transform(parseAuthHeader)
            .value_or(std::pair<std::string_view, std::string_view>{"-", "-"});

    const auto user = mGet(auth.write, token).value_or("-");

    return std::format(
        "{:5} {:15} {:30} v{:<11} {:15} Size: {:10} Created: {:%Y-%m-%d %H:%M} "
        "Sha: {} Auth {} User {}",
        req.method, fp::mGet(req.headers, "REMOTE_ADDR").value_or("?.?.?.?"), info.package,
        info.version, info.arch, ByteSize{info.size}, info.time, info.sha, token, user);
}

}  // namespace vcache

int main(int argc, char* argv[]) {
    using namespace vcache;

    const auto settings = parseArgs(argc, argv);
    auto log = createLog(settings.logLevel, settings.logFile);
    auto store = Store(settings.cacheDir, log);
    auto server = createServer(settings.certAndKey);
    server->set_logger(logger(log));

    server->Get(R"(/cache/([0-9a-f]{64}))",
                [&](const httplib::Request& req, httplib::Response& res) {
                    const auto sha = req.matches[1].str();
                    if (!store.exists(sha)) {
                        res.status = httplib::StatusCode::NotFound_404;
                        return;
                    }
                    const auto* info = store.info(sha);
                    log->info(logCache(req, *info, settings.auth));

                    res.set_content_provider(
                        info->size, "application/zip",
                        [fstream = store.read(sha), buff = std::vector<char>(1024)](
                            size_t offset, size_t length, httplib::DataSink& sink) mutable -> bool {
                            buff.resize(length);
                            fstream->seekg(offset);
                            fstream->read(buff.data(), length);
                            sink.write(buff.data(), length);
                            return true;
                        });
                });

    server->Put(
        "/cache/([0-9a-f]{64})",
        authorizeRequest(settings.auth, [&](const httplib::Request& req, httplib::Response& res,
                                            const httplib::ContentReader& content_reader) {
            const auto sha = req.matches[1].str();
            if (store.exists(sha)) {
                res.status = httplib::StatusCode::Conflict_409;
                return;
            }

            {
                auto fstream = store.write(sha);
                content_reader([fstream](const char* data, size_t data_length) {
                    fstream->write(data, data_length);
                    return true;
                });
                fstream->close();
            }

            if (const auto* info = store.info(sha)) {
                log->info(logCache(req, *info, settings.auth));
            } else {
                log->warn("Expected to find a new package at {}", sha);
            }
        }));

    server->Get("/match", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(site::match(), "text/html");
    });
    server->Post("/match", [&](const httplib::Request& req, httplib::Response& res) {
        const auto abi = req.get_file_value("abi_file").content;
        const auto package = req.get_file_value("package").content;
        res.set_content(site::match(abi, package, store), "text/html");
    });
    server->Get("/compare/:sha", [&](const httplib::Request& req, httplib::Response& res) {
        const auto sha = req.path_params.at("sha");
        res.set_content(site::compare(sha, store), "text/html");
    });
    server->Get(R"(/list)", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content(site::list(store), "text/html");
    });
    server->Get(R"(/find/:package)", [&](const httplib::Request& req, httplib::Response& res) {
        const auto package = req.path_params.at("package");
        res.set_content(site::find(package, store), "text/html");
    });
    server->Get(R"(/package/:sha)", [&](const httplib::Request& req, httplib::Response& res) {
        const auto sha = req.path_params.at("sha");
        res.set_content(site::sha(sha, store), "text/html");
    });
    server->Get("/favicon.svg", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content(site::favicon(), "image/svg+xml");
    });

    server->set_error_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_content(
            std::format("<p>Error Status: <span style='color:red;'>{}</span></p>", res.status),
            "text/html");
    });
    server->set_exception_handler(
        [](const httplib::Request&, httplib::Response& res, std::exception_ptr ep) {
            res.set_content(std::format("<h1>Error 500</h1><p>{}</p>", fp::exceptionToString(ep)),
                            "text/html");
            res.status = httplib::StatusCode::InternalServerError_500;
        });

    log->info("Start server {}:{}", settings.host, settings.port);
    server->listen(settings.host, settings.port);
}
