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
        settings.auth = {.write = fp::UnorderedStringSet{
                             std::from_range, args.get<std::vector<std::string>>("--auth")}};
        settings.cert = std::filesystem::path{args.get<std::string>("--cert")};
        settings.key = std::filesystem::path{args.get<std::string>("--key")};

    } catch (const std::exception& err) {
        std::println("{}\n{}", err.what(), args.help().str());
        std::exit(1);
    }

    return settings;
}


}

