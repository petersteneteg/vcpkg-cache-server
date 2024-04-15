#include <vcpkg-cache-server/settings.hpp>

#include <argparse/argparse.hpp>
#include <print>

namespace vcache {

Settings parseArgs(int argc, char* argv[]) {
    Settings settings{};

    argparse::ArgumentParser args("vcpkg_cache_server");

    args.add_argument("--cache_dir")
        .required()
        .help("Directory where to read and write cache")
        .metavar("DIR");
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
    args.add_argument("--auth")
        .default_value(std::vector<std::string>{})
        .nargs(argparse::nargs_pattern::at_least_one)
        .help("List of authentication tokens for write access");
    args.add_argument("--cert").help("Cert File").metavar("FILE");
    args.add_argument("--key").help("Key File").metavar("FILE");

    try {
        args.parse_args(argc, argv);
        settings.cacheDir = std::filesystem::path{args.get<std::string>("--cache_dir")};

        settings.logLevel = static_cast<spdlog::level::level_enum>(args.get<int>("--verbosity"));
        settings.auth = {.write = fp::UnorderedStringSet{
                             std::from_range, args.get<std::vector<std::string>>("--auth")}};

        if ((args.is_used("--cert") && !args.is_used("--key")) ||
            (!args.is_used("--cert") && args.is_used("--key"))) {
            throw std::runtime_error("Error: --cert and --key has to be passed together");
        } else if (args.is_used("--cert") && args.is_used("--key")) {
            settings.certAndKey = {std::filesystem::path{args.get<std::string>("--cert")},
                                   std::filesystem::path{args.get<std::string>("--key")}};
        }
        if (args.is_used("--port")) {
            settings.port = args.get<int>("--port");
        } else if (args.is_used("--cert") && args.is_used("--key")) {
            settings.port = 443;
        } else {
            settings.port = 80;
        }

        settings.host = args.get<std::string>("--host");

        if (args.is_used("--log_file")) {
            settings.logFile = std::filesystem::path{args.get<std::string>("--log_file")};
        }

    } catch (const std::exception& err) {
        std::println("{}\n{}", err.what(), args.help().str());
        std::exit(1);
    }

    return settings;
}

}  // namespace vcache
