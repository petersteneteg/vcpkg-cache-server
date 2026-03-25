#include <vcpkg-cache-server/settings.hpp>

#include <argparse/argparse.hpp>

#include <yaml-cpp/yaml.h>

#include <fmt/std.h>

namespace vcache {

namespace {

std::string formatByteSizeForYaml(ByteSize size) {
    const auto bytes = std::to_underlying(size);
    if (bytes % 1'000'000'000'000 == 0 && bytes >= 1'000'000'000'000) {
        return fmt::format("{}TB", bytes / 1'000'000'000'000);
    } else if (bytes % 1'000'000'000 == 0 && bytes >= 1'000'000'000) {
        return fmt::format("{}GB", bytes / 1'000'000'000);
    } else if (bytes % 1'000'000 == 0 && bytes >= 1'000'000) {
        return fmt::format("{}MB", bytes / 1'000'000);
    } else if (bytes % 1'000 == 0 && bytes >= 1'000) {
        return fmt::format("{}kB", bytes / 1'000);
    } else {
        return fmt::format("{}", bytes);
    }
}

std::string formatDurationForYaml(Duration dur) {
    using namespace std::chrono;
    namespace c = std::chrono;

    const auto years = duration_cast<c::years>(dur);
    dur -= years;
    const auto days = duration_cast<c::days>(dur);
    dur -= days;
    const auto hours = duration_cast<c::hours>(dur);
    dur -= hours;
    const auto minutes = duration_cast<c::minutes>(dur);
    dur -= minutes;
    const auto seconds = duration_cast<c::seconds>(dur);

    std::string res;
    const auto append = [&](auto count, char unit) {
        if (count != 0) {
            if (!res.empty()) res += ' ';
            res += fmt::format("{}{}", count, unit);
        }
    };
    append(years.count(), 'y');
    append(days.count(), 'd');
    append(hours.count(), 'h');
    append(minutes.count(), 'm');
    append(seconds.count(), 's');

    return res.empty() ? "0s" : res;
}

}  // namespace

std::string generateConfigYaml(const Settings& settings) {
    std::string out;

    out += "# vcpkg-cache-server configuration file\n";
    out += "# Use with: vcpkg-cache-server --config <path-to-this-file>\n";
    out += "\n";

    // cache_dir
    out += "# Directory where cache zip files are stored and served from (required)\n";
    if (!settings.cacheDir.empty()) {
        out += fmt::format("cache_dir: {}\n", settings.cacheDir.generic_string());
    } else {
        out += "# cache_dir: /path/to/cache\n";
    }
    out += "\n";

    // port
    out +=
        "# Port to listen on; when omitted the server defaults to 80 (HTTP) or 443 (HTTPS with "
        "SSL)\n";
    if (settings.port >= 0) {
        out += fmt::format("port: {}\n", settings.port);
    } else {
        out += "# port: 8080\n";
    }
    out += "\n";

    // host
    out += "# Host/IP address to bind to (0.0.0.0 binds to all available interfaces)\n";
    out += fmt::format("host: {}\n", settings.host);
    out += "\n";

    // verbosity
    out +=
        "# Log verbosity level: 0=trace, 1=debug, 2=info, 3=warn, 4=error, 5=critical, 6=off\n";
    out += fmt::format("verbosity: {}\n", static_cast<int>(settings.logLevel));
    out += "\n";

    // log_file
    out += "# Path to a log file (optional; always logs at trace level regardless of verbosity)\n";
    if (settings.logFile) {
        out += fmt::format("log_file: {}\n", settings.logFile->generic_string());
    } else {
        out += "# log_file: /path/to/server.log\n";
    }
    out += "\n";

    // db_file
    out += "# SQLite database file used to track download statistics (required)\n";
    if (!settings.dbFile.empty()) {
        out += fmt::format("db_file: {}\n", settings.dbFile.generic_string());
    } else {
        out += "# db_file: /path/to/cache.db\n";
    }
    out += "\n";

    // ssl
    out +=
        "# SSL/TLS certificate and private key files; both must be provided together to enable "
        "HTTPS\n";
    if (settings.certAndKey) {
        out += "ssl:\n";
        out += fmt::format("  cert: {}\n", settings.certAndKey->first.generic_string());
        out += fmt::format("  key: {}\n", settings.certAndKey->second.generic_string());
    } else {
        out += "# ssl:\n";
        out += "#   cert: /path/to/cert.pem\n";
        out += "#   key: /path/to/key.pem\n";
    }
    out += "\n";

    // auth
    out += "# Bearer tokens that grant write access to the cache\n";
    out +=
        "# Format: <token>: <username>  (token is checked against the Authorization header, "
        "username is used in logs)\n";
    if (!settings.auth.write.empty()) {
        out += "auth:\n";
        for (const auto& [token, user] : settings.auth.write) {
            out += fmt::format("  {}: {}\n", token, user);
        }
    } else {
        out += "# auth:\n";
        out += "#   my-secret-token: User 1\n";
    }
    out += "\n";

    // maintenance
    out += "# Cache maintenance settings: automatically evict old or oversized entries\n";
    out += "maintenance:\n";
    out += "\n";
    out += "  # When true, only report what would be removed without actually deleting anything\n";
    out += fmt::format("  dry_run: {}\n", settings.maintenance.dryrun ? "true" : "false");
    out += "\n";
    out += "  # Maximum total size of the entire cache (supports suffixes: TB, GB, MB, kB)\n";
    if (settings.maintenance.maxTotalSize) {
        out += fmt::format("  max_total_size: {}\n",
                           formatByteSizeForYaml(*settings.maintenance.maxTotalSize));
    } else {
        out += "  # max_total_size: 100GB\n";
    }
    out += "\n";
    out += "  # Maximum size allowed for a single package cache entry\n";
    if (settings.maintenance.maxPackageSize) {
        out += fmt::format("  max_package_size: {}\n",
                           formatByteSizeForYaml(*settings.maintenance.maxPackageSize));
    } else {
        out += "  # max_package_size: 1GB\n";
    }
    out += "\n";
    out +=
        "  # Remove entries older than this age (format: <N>y <N>d <N>h <N>m <N>s, e.g. 1y, "
        "30d, 24h)\n";
    if (settings.maintenance.maxAge) {
        out += fmt::format("  max_age: {}\n", formatDurationForYaml(*settings.maintenance.maxAge));
    } else {
        out += "  # max_age: 1y\n";
    }
    out += "\n";
    out += "  # Remove entries that have not been accessed for longer than this duration\n";
    if (settings.maintenance.maxUnused) {
        out += fmt::format("  max_unused: {}\n",
                           formatDurationForYaml(*settings.maintenance.maxUnused));
    } else {
        out += "  # max_unused: 30d\n";
    }
    out += "\n";

    // thread_pool
    out += "# Thread pool settings for the HTTP server\n";
    out += "thread_pool:\n";
    out += "\n";
    out += "  # Number of base (permanent) worker threads\n";
    if (settings.threadPool.baseThreads) {
        out += fmt::format("  base_threads: {}\n", *settings.threadPool.baseThreads);
    } else {
        out += "  # base_threads: 12\n";
    }
    out += "\n";
    out += "  # Maximum number of dynamic worker threads (0 = unlimited)\n";
    if (settings.threadPool.maxThreads) {
        out += fmt::format("  max_threads: {}\n", *settings.threadPool.maxThreads);
    } else {
        out += "  # max_threads: 0\n";
    }
    out += "\n";
    out += "  # Maximum number of queued requests (0 = unlimited)\n";
    if (settings.threadPool.maxQueuedRequests) {
        out += fmt::format("  max_queued_requests: {}\n", *settings.threadPool.maxQueuedRequests);
    } else {
        out += "  # max_queued_requests: 18\n";
    }

    return out;
}

void parseConfig(const std::filesystem::path& configFile, Settings& settings) {
    YAML::Node config = YAML::LoadFile(configFile.generic_string());

    if (config["cache_dir"]) {
        settings.cacheDir = std::filesystem::path{config["cache_dir"].as<std::string>()};
    }

    if (config["port"]) {
        settings.port = config["port"].as<int>();
    }

    if (config["host"]) {
        settings.host = config["host"].as<std::string>();
    }

    if (config["verbosity"]) {
        settings.logLevel = static_cast<spdlog::level::level_enum>(config["verbosity"].as<int>());
    }

    if (config["log_file"]) {
        settings.logFile = std::filesystem::path{config["log_file"].as<std::string>()};
    }

    if (config["db_file"]) {
        settings.dbFile = std::filesystem::path{config["db_file"].as<std::string>()};
    }

    if (config["ssl"]) {
        const auto ssl = config["ssl"];
        if (ssl["cert"] && ssl["key"]) {
            settings.certAndKey = {std::filesystem::path{ssl["cert"].as<std::string>()},
                                   std::filesystem::path{ssl["key"].as<std::string>()}};
        } else {
            throw std::runtime_error(
                "Error parsing config file: cert and key has to be passed together");
        }
    }

    if (config["auth"]) {
        const auto auth = config["auth"];
        for (auto&& item : auth) {
            settings.auth.write[item.first.as<std::string>()] = item.second.as<std::string>();
        }
    }

    if (config["maintenance"]) {
        const auto maintenance = config["maintenance"];
        if (maintenance["max_total_size"]) {
            settings.maintenance.maxTotalSize = maintenance["max_total_size"].as<ByteSize>();
        }

        if (maintenance["max_package_size"]) {
            settings.maintenance.maxPackageSize = maintenance["max_package_size"].as<ByteSize>();
        }

        if (maintenance["max_age"]) {
            settings.maintenance.maxAge = maintenance["max_age"].as<Duration>();
        }

        if (maintenance["max_unused"]) {
            settings.maintenance.maxUnused = maintenance["max_unused"].as<Duration>();
        }

        if (maintenance["dry_run"]) {
            settings.maintenance.dryrun = maintenance["dry_run"].as<bool>();
        }
    }

    if (config["thread_pool"]) {
        const auto threadPool = config["thread_pool"];
        if (threadPool["base_threads"]) {
            settings.threadPool.baseThreads = threadPool["base_threads"].as<size_t>();
        }
        if (threadPool["max_threads"]) {
            settings.threadPool.maxThreads = threadPool["max_threads"].as<size_t>();
        }
        if (threadPool["max_queued_requests"]) {
            settings.threadPool.maxQueuedRequests =
                threadPool["max_queued_requests"].as<size_t>();
        }
    }
}

Settings parseArgs(int argc, char* argv[]) {
    Settings settings{};

    argparse::ArgumentParser args("vcpkg_cache_server");

    args.add_argument("--cache_dir").help("Directory where to read and write cache").metavar("DIR");
    args.add_argument("--port")
        .scan<'d', int>()
        .help("Port to listen to, defaults to 80 or 443")
        .metavar("PORT");
    args.add_argument("--host")
        .default_value(settings.host)
        .help("Host to listen to")
        .metavar("HOST");
    args.add_argument("--verbosity")
        .scan<'d', int>()
        .default_value(static_cast<int>(settings.logLevel))
        .help("Verbosity level 0 (All) to 6 (Off)")
        .metavar("LEVEL");
    args.add_argument("--log_file")
        .help("Log file, will write with log level 0 (All)")
        .metavar("FILE");
    args.add_argument("--db_file").help("Db file").metavar("FILE");
    args.add_argument("--config").help("Config file to read settings from").metavar("FILE");
    args.add_argument("--auth")
        .default_value(std::vector<std::string>{})
        .nargs(argparse::nargs_pattern::at_least_one)
        .help("List of authentication tokens for write access");
    args.add_argument("--cert").help("Cert File").metavar("FILE");
    args.add_argument("--key").help("Key File").metavar("FILE");
    args.add_argument("--generate_config")
        .default_value(false)
        .implicit_value(true)
        .help(
            "Print a YAML configuration file template to stdout and exit. "
            "Any other options provided on the command line will be reflected in the output.");

    try {
        args.parse_args(argc, argv);
        if (args.is_used("--config")) {
            const auto configFile = std::filesystem::path{args.get<std::string>("--config")};
            try {
                parseConfig(configFile, settings);
            } catch (const YAML::Exception& e) {
                fmt::println("Error parsing config file {} {}", configFile, e.what());
                std::exit(1);
            }
        }

        if (args.is_used("--cache_dir")) {
            settings.cacheDir = std::filesystem::path{args.get<std::string>("--cache_dir")};
        }

        if (args.is_used("--verbosity")) {
            settings.logLevel =
                static_cast<spdlog::level::level_enum>(args.get<int>("--verbosity"));
        }

        for (size_t i = 1; auto& token : args.get<std::vector<std::string>>("--auth")) {
            settings.auth.write[token] = fmt::format("User {}", i++);
        }

        if ((args.is_used("--cert") && !args.is_used("--key")) ||
            (!args.is_used("--cert") && args.is_used("--key"))) {
            throw std::runtime_error("Error: --cert and --key has to be passed together");
        } else if (args.is_used("--cert") && args.is_used("--key")) {
            settings.certAndKey = {std::filesystem::path{args.get<std::string>("--cert")},
                                   std::filesystem::path{args.get<std::string>("--key")}};
        }

        if (args.is_used("--port")) {
            settings.port = args.get<int>("--port");
        }

        if (args.is_used("--host")) {
            settings.host = args.get<std::string>("--host");
        }

        if (args.is_used("--log_file")) {
            settings.logFile = std::filesystem::path{args.get<std::string>("--log_file")};
        }

        if (args.is_used("--db_file")) {
            settings.dbFile = std::filesystem::path{args.get<std::string>("--db_file")};
        }

        if (settings.port < 0) {
            settings.port = settings.certAndKey ? 443 : 80;
        }

        if (args.get<bool>("--generate_config")) {
            fmt::print("{}", generateConfigYaml(settings));
            std::exit(0);
        }

        if (settings.cacheDir.empty()) {
            fmt::println("A cache dir must be provided\n{}", args.help().str());
            std::exit(1);
        }

        if (settings.dbFile.empty()) {
            fmt::println("A db file must be provided\n{}", args.help().str());
            std::exit(1);
        }

    } catch (const std::exception& err) {
        fmt::println("{}\n{}", err.what(), args.help().str());
        std::exit(1);
    }

    return settings;
}

}  // namespace vcache
