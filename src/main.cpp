#include <vcpkg-cache-server/settings.hpp>
#include <vcpkg-cache-server/store.hpp>
#include <vcpkg-cache-server/site.hpp>
#include <vcpkg-cache-server/functional.hpp>
#include <vcpkg-cache-server/database.hpp>

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

std::pair<std::string, std::string> requestUserToken(const httplib::Request& req,
                                                     const Authorization& auth) {
    const auto authHeader = fp::mGet(req.headers, "Authorization");
    const auto [scheme, token] =
        authHeader.transform(parseAuthHeader)
            .value_or(std::pair<std::string_view, std::string_view>{"-", "-"});

    const auto user = mGet(auth.write, token).value_or("-");
    return {user, std::string{token}};
}

std::string logCache(const httplib::Request& req, const Info& info, const Authorization& auth) {
    const auto [user, token] = requestUserToken(req, auth);

    return fmt::format(
        "{:5} {:15} {:30} v{:<11} {:15} Size: {:10} Created: {:%Y-%m-%d %H:%M} "
        "Sha: {} Auth {} User {}",
        req.method, fp::mGet(req.headers, "REMOTE_ADDR").value_or("?.?.?.?"), info.package,
        info.version, info.arch, ByteSize{info.size}, info.time, info.sha, token, user);
}

size_t removeCache(const db::Cache& cache, db::Database& db, std::vector<std::string>& toDelete,
                   std::shared_ptr<spdlog::logger> log) {
    using namespace sqlite_orm;

    log->info(
        "[Maintain]    Removing cache: {} used: {:%Y-%m-%d %H:%M} "
        "created: {:%Y-%m-%d %H:%M} size: {:>10}",
        cache.sha, Time{Duration{cache.lastUsed}}, Time{Duration{cache.created}},
        ByteSize{cache.size});

    db.update_all(set(c(&db::Cache::deleted) = true), where(c(&db::Cache::id) == cache.id));

    toDelete.push_back(cache.sha);

    return cache.size;
}

void maintain(Store& store, db::Database& db, const Maintenance& maintenance,
              std::shared_ptr<spdlog::logger> log, Time now) {

    std::vector<std::string> toDelete;

    db.begin_transaction();

    size_t allRemoved{};

    log->info("[Maintain] Running Maintenance");

    allRemoved += maintenance.maxAge
                      .and_then([&](Duration maxAge) -> std::optional<size_t> {
                          using namespace sqlite_orm;
                          const auto cutoff = now - maxAge;

                          log->info("[Maintain] Looking for packages created before: {} ({})",
                                    cutoff, maxAge);
                          size_t removed{};
                          for (auto& cache : db.iterate<db::Cache>(where(and_(
                                   c(&db::Cache::deleted) == false,
                                   c(&db::Cache::created) < cutoff.time_since_epoch().count())))) {

                              removed += removeCache(cache, db, toDelete, log);
                          }
                          return removed;
                      })
                      .value_or(size_t{0});

    allRemoved += maintenance.maxUnused
                      .and_then([&](Duration maxUnused) -> std::optional<size_t> {
                          using namespace sqlite_orm;
                          const auto cutoff = now - maxUnused;

                          log->info("[Maintain] Looking for packages not used after: {} ({})",
                                    cutoff, maxUnused);
                          size_t removed{};
                          for (auto& cache : db.iterate<db::Cache>(where(and_(
                                   c(&db::Cache::deleted) == false,
                                   c(&db::Cache::lastUsed) < cutoff.time_since_epoch().count())))) {

                              removed += removeCache(cache, db, toDelete, log);
                          }
                          return removed;
                      })
                      .value_or(size_t{0});

    allRemoved +=
        maintenance.maxPackageSize
            .and_then([&](ByteSize max) -> std::optional<size_t> {
                using namespace sqlite_orm;
                const auto sizes = db.select(
                    columns(as<colalias_a>(total(&db::Cache::size)), &db::Package::name,
                            &db::Package::id),
                    inner_join<db::Package>(on(and_(c(&db::Cache::package) == &db::Package::id,
                                                    c(&db::Cache::deleted) == false))),
                    group_by(&db::Cache::package)
                        .having(greater_than(get<colalias_a>(), std::to_underlying(max))));

                size_t totalRemoved{};
                for (const auto& [size, name, pid] : sizes) {
                    log->info("[Maintain] Package: {:20} size {:>10} exceeds given max size {:>10}",
                              name, ByteSize{static_cast<size_t>(size)}, max);

                    const auto overflow = size - std::to_underlying(max);
                    size_t removed{};
                    for (auto& cache :
                         db.iterate<db::Cache>(where(and_(c(&db::Cache::package) == pid,
                                                          c(&db::Cache::deleted) == false)),
                                               multi_order_by(order_by(&db::Cache::lastUsed),
                                                              order_by(&db::Cache::created)))) {
                        removed += removeCache(cache, db, toDelete, log);
                        if (removed > overflow) break;
                    }
                    totalRemoved += removed;
                }
                return totalRemoved;
            })
            .value_or(size_t{0});

    allRemoved += maintenance.maxTotalSize
                      .and_then([&](ByteSize max) -> std::optional<size_t> {
                          using namespace sqlite_orm;
                          const auto totalSize =
                              db.total(&db::Cache::size, where(c(&db::Cache::deleted) == false));

                          if (totalSize > std::to_underlying(max)) {
                              const auto overflow = totalSize - std::to_underlying(max);
                              log->info("[Maintain] Total Cache size {} exceeds given max {} by {}",
                                        ByteSize{static_cast<size_t>(totalSize)}, ByteSize{max},
                                        ByteSize(overflow));
                              return overflow;
                          } else {
                              return std::nullopt;
                          }
                      })
                      .and_then([&](size_t overflow) -> std::optional<size_t> {
                          using namespace sqlite_orm;

                          size_t removed{};
                          for (auto& cache : db.iterate<db::Cache>(
                                   where(c(&db::Cache::deleted) == false),
                                   multi_order_by(order_by(&db::Cache::lastUsed),
                                                  order_by(&db::Cache::created)))) {
                              removed += removeCache(cache, db, toDelete, log);
                              if (removed > overflow) break;
                          }
                          return removed;
                      })
                      .value_or(size_t{0});

    if (allRemoved > 0) {
        log->info("[Maintain] Remove a total of {}", ByteSize{allRemoved});
    }

    if (maintenance.dryrun) {
        db.rollback();
        log->info("[Maintain] changes discarded, dry run mode");
    } else {
        db.commit();
        for (const auto& sha : toDelete) {
            store.remove(sha);
        }
    }
    log->info("[Maintain] Maintenance finished");
}

}  // namespace vcache

// Add tests?

int main(int argc, char* argv[]) {
    using namespace vcache;

    const auto settings = parseArgs(argc, argv);
    auto log = createLog(settings.logLevel, settings.logFile);

    auto db = db::create(settings.dbFile);

    auto store = Store(settings.cacheDir, log);

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

    std::jthread maintenance{[log, &settings, &db, &store](std::stop_token token) {
        while (!token.stop_requested()) {
            maintain(store, db, settings.maintenance, log, Clock::now());

            std::mutex mutex;
            std::unique_lock lock(mutex);
            std::condition_variable_any().wait_for(lock, token, std::chrono::hours{1},
                                                   [] { return false; });
        }
    }};

    auto server = createServer(settings.certAndKey);
    server->set_logger(logger(log));

    server->Get(
        R"(/cache/([0-9a-f]{64}))", [&](const httplib::Request& req, httplib::Response& res) {
            const auto sha = req.matches[1].str();

            if (auto reader = store.read(sha)) {
                const auto& info = reader->getInfo();
                log->info(logCache(req, info, settings.auth));

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
                log->info(logCache(req, *info, settings.auth));

                db::addCache(
                    db, db::Cache{.sha = info->sha,
                                  .package = db::getOrAddPackageId(db, info->package),
                                  .created = info->time.time_since_epoch().count(),
                                  .ip = fp::mGet(req.headers, "REMOTE_ADDR").value_or("?.?.?.?"),
                                  .user = requestUserToken(req, settings.auth).first,
                                  .size = info->size});

            } else {
                log->warn("Expected to find a new package at {}", sha);
            }
        }));

    const auto mode = [](const httplib::Request& req) {
        return req.params.find("plain") != req.params.end() ? site::Mode::Plain : site::Mode::Full;
    };

    const auto sort = [](const httplib::Request& req) -> site::Sort {
        return fp::mGet(req.params, "sort")
            .and_then(enumTo<site::Sort>{})
            .value_or(site::Sort::Default);
    };
    const auto order = [](const httplib::Request& req) -> site::Order {
        return fp::mGet(req.params, "order")
            .and_then(enumTo<site::Order>{})
            .value_or(site::Order::Ascending);
    };

    const auto search = [](const httplib::Request& req) -> std::string {
        return fp::mGet(req.params, "search").value_or(std::string{});
    };

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

    server->set_error_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_content(
            fmt::format("<p>Error Status: <span style='color:red;'>{}</span></p>", res.status),
            "text/html");
    });
    server->set_exception_handler(
        [](const httplib::Request&, httplib::Response& res, std::exception_ptr ep) {
            res.set_content(fmt::format("<h1>Error 500</h1><p>{}</p>", fp::exceptionToString(ep)),
                            "text/html");
            res.status = httplib::StatusCode::InternalServerError_500;
        });

    log->info("Start server {}:{}", settings.host, settings.port);
    server->listen(settings.host, settings.port);
}
