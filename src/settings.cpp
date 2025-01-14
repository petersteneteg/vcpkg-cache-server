#include <vcpkg-cache-server/settings.hpp>

#include <argparse/argparse.hpp>

#include <yaml-cpp/yaml.h>

#include <fmt/std.h>

namespace vcache {

void parseConfig(const std::filesystem::path& configFile, Settings& settings) {
    YAML::Node config = YAML::LoadFile(configFile);

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
