#include <vcpkg-cache-server/settings.hpp>
#include <vcpkg-cache-server/store.hpp>
#include <vcpkg-cache-server/site.hpp>
#include <vcpkg-cache-server/functional.hpp>
#include <vcpkg-cache-server/database.hpp>
#include <vcpkg-cache-server/maintenance.hpp>
#include <vcpkg-cache-server/logging.hpp>

#include <httplib.h>

#include <fmt/format.h>
#include <fmt/std.h>
#include <fmt/chrono.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <unordered_map>
#include <string>
#include <string_view>
#include <ranges>
#include <optional>
#include <utility>
#include <algorithm>
#include <numeric>
#include <thread>
#include <condition_variable>
#include <chrono>
#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace vcache {

void logRequest(spdlog::logger& logger, spdlog::level::level_enum lvl,
                const httplib::Request& req) {
    log::report(logger, lvl, "{:>20}: {}", "method", req.method);
    log::report(logger, lvl, "{:>20}: {}", "path", req.path);
    log::report(logger, lvl, "{:>20}: {}", "matched_route", req.matched_route);
    log::report(logger, lvl, "{:>20}: {}", "remote_addr", req.remote_addr);
    log::report(logger, lvl, "{:>20}: {}", "local_addr", req.local_addr);

    for (const auto& [key, val] : req.params) {
        log::report(logger, lvl, "{:>20}: {}", key, val);
    }
    for (const auto& [key, val] : req.headers) {
        log::report(logger, lvl, "{:>20}: {}", key, val);
    }
    for (const auto& [key, val] : req.trailers) {
        log::report(logger, lvl, "{:>20}: {}", key, val);
    }
    for (const auto& [key, val] : req.path_params) {
        log::report(logger, lvl, "{:>20}: {}", key, val);
    }
    for (const auto& [name, file] : req.form.files) {
        log::report(logger, lvl, "{:>20}: {}", "name", name);
        log::report(logger, lvl, "{:>20}: {}", "filename", file.filename);
        log::report(logger, lvl, "{:>20}: {}", "content type", file.content_type);
        log::report(logger, lvl, "{:>20}: {}", "text length", file.content.size());
    }
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
        const auto [scheme, token] = fp::parseAuthHeader(authHeader);

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
    auto logger = [&]() {
        if (logFile) {
            auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                logFile->generic_string(), false);
            return std::make_shared<spdlog::logger>("log",
                                                    spdlog::sinks_init_list{consoleSink, fileSink});
        } else {
            return spdlog::stdout_color_mt("log");
        }
    }();

    logger->set_level(logLevel);
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%P/%t] [%15s:%4#] [%8l]: %v");
    log::info(*logger, "logLevel is set to {}", to_string_view(logLevel));
    return logger;
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

std::pair<std::string, std::string> requestUserToken(const httplib::Request& req,
                                                     const Authorization& auth) {
    const auto authHeader = fp::mGet(req.headers, "Authorization");
    const auto [scheme, token] =
        authHeader.transform(fp::parseAuthHeader)
            .value_or(std::pair<std::string_view, std::string_view>{"-", "-"});

    const auto user = mGet(auth.write, token).value_or("-");
    return {user, std::string{token}};
}

void logCache(spdlog::logger& logger, const httplib::Request& req, const Info& info,
              const Authorization& auth) {
    const auto [user, token] = requestUserToken(req, auth);
    log::info(logger,
              "{:5} {:15} {:30} v{:<11} {:15} Size: {:10} Created: {:%Y-%m-%d %H:%M} "
              "Sha: {} Auth {} User {}",
              req.method, req.remote_addr, info.package, info.version, info.arch,
              ByteSize{info.size}, info.time, info.sha, token, user);
}

}  // namespace vcache

int main(int argc, char* argv[]) {
    using namespace vcache;

    const auto settings = parseArgs(argc, argv);
    auto logger = createLog(settings.logLevel, settings.logFile);
    logger->flush_on(spdlog::level::trace);

    auto db = db::create(settings.dbFile);

    auto store = Store(settings.cacheDir, logger);

    for (auto& item : store.allInfos()) {
        auto pid = db::getOrAddPackageId(db, item.package);
        db::getCacheId(db, item.sha).or_else([&]() -> std::optional<int> {
            return db::addCache(db, db::Cache{.sha = item.sha,
                                              .package = pid,
                                              .created = item.time.time_since_epoch().count(),
                                              .size = item.size})
                .id;
        });
    }

    std::jthread maintenance{[logger, &settings, &db, &store](std::stop_token token) {
        try {
            std::mutex mutex;
            while (!token.stop_requested()) {
                vcache::maintain(store, db, settings.maintenance, logger, Clock::now());
                std::unique_lock lock(mutex);
                std::condition_variable_any().wait_for(lock, token, std::chrono::hours{1},
                                                       [] { return false; });
            }
        } catch (const std::exception& e) {
            log::error(*logger, "[Maintain] failed with error {}", e.what());
        } catch (...) {
            log::error(*logger, "[Maintain] failed unknown error");
        }
    }};

    auto server = createServer(settings.certAndKey);

    if (settings.threadPool.baseThreads || settings.threadPool.maxThreads ||
        settings.threadPool.maxQueuedRequests) {
        const auto baseThreads = settings.threadPool.baseThreads.value_or(std::max(
            8u,
            std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() - 1 : 0));
        const auto maxThreads = settings.threadPool.maxThreads.value_or(baseThreads * 4);
        const auto maxQueuedRequests = settings.threadPool.maxQueuedRequests.value_or(0);
        log::info(*logger, "Thread pool: base_threads={}, max_threads={}, max_queued_requests={}",
                  baseThreads, maxThreads, maxQueuedRequests);
        server->new_task_queue = [=] {
            return new httplib::ThreadPool(baseThreads, maxThreads, maxQueuedRequests);
        };
    }

    server
        ->set_logger([logger](const httplib::Request& req, const httplib::Response& res) {
            log::debug(*logger, "{:5} {:15} Status: {:4} Vers: {:8} Path: {}", req.method,
                       req.remote_addr, res.status, req.version, req.path);
            logRequest(*logger, spdlog::level::trace, req);
        })
        .set_error_logger([logger](const httplib::Error& error, const httplib::Request* req) {
            log::error(*logger, "Error happened: {}", to_string(error));
            if (req) {
                logRequest(*logger, spdlog::level::err, *req);
            }
        })
        .set_error_handler([](const httplib::Request&, httplib::Response& res) {
            res.set_content(
                fmt::format("<p>Error Status: <span style='color:red;'>{}</span></p>", res.status),
                "text/html");
        })
        .set_exception_handler(
            [&](const httplib::Request& req, httplib::Response& res, std::exception_ptr ep) {
                const auto error = fp::exceptionToString(ep);
                log::error(*logger, "{}", error);
                logRequest(*logger, spdlog::level::err, req);
                res.set_content(fmt::format("<h1>Error 500</h1><p>{}</p>", error), "text/html");
                res.status = httplib::StatusCode::InternalServerError_500;
            });

    server->Get(
        R"(/cache/([0-9a-f]{64}))", [&](const httplib::Request& req, httplib::Response& res) {
            const auto sha = req.matches[1].str();

            if (auto reader = store.read(sha)) {
                const auto& info = reader->getInfo();
                logCache(*logger, req, info, settings.auth);

                const auto cid = *db::getCacheId(db, info.sha);
                const auto now = Clock::now();
                db::addDownload(
                    db, db::Download{.cache = cid,
                                     .ip = fp::mGet(req.headers, "REMOTE_ADDR").value_or("?.?.?.?"),
                                     .user = requestUserToken(req, settings.auth).first,
                                     .time = now.time_since_epoch().count()});
                db::updateLastUse(db, cid, now);

                res.set_content_provider(
                    info.size, "application/zip",
                    [reader, buff = std::vector<char>(1024)](
                        size_t offset, size_t length, httplib::DataSink& sink) mutable -> bool {
                        buff.resize(length);
                        reader->getStream().seekg(offset);
                        reader->getStream().read(buff.data(), length);
                        sink.write(buff.data(), length);
                        return true;
                    });

            } else {
                res.status = httplib::StatusCode::NotFound_404;
                return;
            }
        });

    server->Put(
        "/cache/([0-9a-f]{64})",
        authorizeRequest(settings.auth, [&](const httplib::Request& req, httplib::Response& res,
                                            const httplib::ContentReader& content_reader) {
            const auto sha = req.matches[1].str();

            if (auto writer = store.write(sha)) {
                content_reader([writer](const char* data, size_t data_length) {
                    writer->getStream().write(data, data_length);
                    return true;
                });
            } else {
                res.status = httplib::StatusCode::Conflict_409;
                return;
            }

            if (const auto* info = store.info(sha)) {
                logCache(*logger, req, *info, settings.auth);

                db::addCache(
                    db, db::Cache{.sha = info->sha,
                                  .package = db::getOrAddPackageId(db, info->package),
                                  .created = info->time.time_since_epoch().count(),
                                  .ip = fp::mGet(req.headers, "REMOTE_ADDR").value_or("?.?.?.?"),
                                  .user = requestUserToken(req, settings.auth).first,
                                  .size = info->size});

            } else {
                log::warn(*logger, "Expected to find a new package at {}", sha);
            }
        }));

    const auto mode = [](const httplib::Request& req) -> site::Mode {
        return fp::mGet(req.params, "mode")
            .and_then(enumTo<site::Mode>{})
            .value_or(site::Mode::Full);
    };

    const auto sort = [](const httplib::Request& req) -> site::Sort {
        return fp::mGet(req.params, "sort")
            .and_then(enumTo<site::Sort>{})
            .value_or(site::Sort::Default);
    };
    const auto sortIdx = [](const httplib::Request& req) -> std::optional<size_t> {
        return fp::mGet(req.params, "sortidx").and_then(fp::strToNum<size_t>);
    };

    const auto selection =
        [](const httplib::Request& req) -> std::optional<std::pair<site::Sort, std::string>> {
        auto selCol = fp::mGet(req.params, "selcol").and_then(enumTo<site::Sort>{});
        auto selVal = fp::mGet(req.params, "selval");
        if (selCol && selVal) {
            return std::pair<site::Sort, std::string>{*selCol, *selVal};
        } else {
            return std::nullopt;
        }
    };

    const auto order = [](const httplib::Request& req) -> std::optional<site::Order> {
        return fp::mGet(req.params, "order").and_then(enumTo<site::Order>{});
    };
    const auto limit = [](const httplib::Request& req) -> site::Limit {
        return {.offset = fp::mGet(req.params, "offset").and_then(fp::strToNum<size_t>),
                .limit = fp::mGet(req.params, "limit").and_then(fp::strToNum<size_t>)};
    };

    const auto search = [](const httplib::Request& req) -> std::string {
        return fp::mGet(req.params, "search").value_or(std::string{});
    };

    server->Get("/status", [](const httplib::Request&, httplib::Response& res) {
        const auto fds = fp::openFileDescriptors();
        const auto fdsStr = fds ? fmt::format("{}", *fds) : "null";
#if defined(_WIN32)
        const auto pid = static_cast<long>(::GetCurrentProcessId());
#else
        const auto pid = static_cast<long>(::getpid());
#endif
        res.set_content(fmt::format(R"({{"pid":{},"open_fds":{}}})", pid, fdsStr),
                        "application/json");
    });

    server->Get("/match", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(site::match(), "text/html");
    });
    server->Post("/match", [&](const httplib::Request& req, httplib::Response& res) {
        const auto abi = req.form.get_file("abi_file").content;
        const auto package = req.form.get_file("package").content;
        res.set_content(site::match(abi, package, store), "text/html");
    });
    server->Get("/compare/:sha", [&](const httplib::Request& req, httplib::Response& res) {
        const auto sha = req.path_params.at("sha");
        res.set_content(site::compare(sha, store, mode(req)), "text/html");
    });
    server->Get(R"(/list)", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(site::index(store, db, mode(req), sort(req), order(req), search(req)),
                        "text/html");
    });
    server->Get(R"(/find/:package)", [&](const httplib::Request& req, httplib::Response& res) {
        const auto package = req.path_params.at("package");
        res.set_content(site::find(package, store, db, mode(req), sort(req), order(req)),
                        "text/html");
    });
    server->Get(R"(/package/:sha)", [&](const httplib::Request& req, httplib::Response& res) {
        const auto sha = req.path_params.at("sha");
        res.set_content(site::sha(sha, store, mode(req)), "text/html");
    });

    server->Get(R"(/downloads)", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            site::downloads(db, mode(req), sortIdx(req), order(req), limit(req), selection(req)),
            "text/html");
    });

    server->Get("/index.html", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(site::index(store, db, mode(req), sort(req), order(req), search(req)),
                        "text/html");
    });
    server->Get("/", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(site::index(store, db, mode(req), sort(req), order(req), search(req)),
                        "text/html");
    });

    server->Get("/favicon.svg", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content(site::favicon(), "image/svg+xml");
    });
    server->Get("/maskicon.svg", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content(site::maskicon(), "image/svg+xml");
    });
    server->Get(R"(/script/:name)", [&](const httplib::Request& req, httplib::Response& res) {
        const auto name = req.path_params.at("name");
        if (auto script = site::script(name)) {
            res.set_content(script->second, script->first);
        } else {
            res.status = httplib::StatusCode::NotFound_404;
        }
    });

    log::info(*logger, "Start server {}:{}", settings.host, settings.port);
    try {
        if (!server->listen(settings.host, settings.port)) {
            log::error(*logger, "Server stop unexpectedly");
        }
    } catch (const std::exception& e) {
        log::error(*logger, "Server failed with error {}", e.what());
    } catch (...) {
        log::error(*logger, "Server failed unknown error");
    }
    log::info(*logger, "Server shutdown");
}
