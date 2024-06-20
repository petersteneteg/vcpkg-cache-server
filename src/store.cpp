#include <vcpkg-cache-server/store.hpp>

#include <libzippp.h>

#include <fmt/std.h>

#include <numeric>
#include <ranges>
#include <format>
#include <set>

namespace vcache {

Store::Store(const std::filesystem::path& aRoot, std::shared_ptr<spdlog::logger> aLog)
    : log{aLog}, root{aRoot}, infos{} {
    log->info("Start scan");
    infos = scan(root, log);
    log->info("Scan finished");
    log->info(statistics());
}

bool Store::exists(std::string_view sha) const {
    return std::filesystem::is_regular_file(shaToPath(sha));
}

const Info* Store::info(std::string_view sha) {
    if (auto it = infos.find(sha); it != infos.end()) {
        return &it->second;
    } else {
        const auto path = shaToPath(sha);
        if (std::filesystem::is_regular_file(path)) {
            auto info = extractInfo(path);
            return &infos.emplace(info.sha, info).first->second;
        }
        return nullptr;
    }
}
const Info* Store::info(std::string_view sha) const {
    if (auto it = infos.find(sha); it != infos.end()) {
        return &it->second;
    } else {
        return nullptr;
    }
}

std::shared_ptr<std::ifstream> Store::read(std::string_view sha) {
    return std::make_shared<std::ifstream>(shaToPath(sha),
                                           std::ios_base::in | std::ios_base::binary);
}

std::shared_ptr<std::ofstream> Store::write(std::string_view sha) {
    return std::make_shared<std::ofstream>(shaToPath(sha),
                                           std::ios_base::out | std::ios_base::binary);
}

std::string Store::statistics() const {
    const auto range = infos | std::views::values | std::views::transform(&Info::size);
    const auto diskSize =
        std::accumulate(std::begin(range), std::end(range), size_t{0}, std::plus<>{});
    const auto packages = infos | std::views::values | std::views::transform(&Info::package) |
                          std::ranges::to<std::set>();

    return std::format("Found {} caches of {} packages. Using {}", infos.size(), packages.size(),
                       ByteSize{diskSize});
}

std::filesystem::path Store::shaToPath(std::string_view sha) const {
    return root / sha.substr(0, 2) / std::format("{}.zip", sha);
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

Info extractInfo(const std::filesystem::path& path) {
    libzippp::ZipArchive zf{path.generic_string()};
    zf.open(libzippp::ZipArchive::ReadOnly);

    auto ctrl = zf.getEntry("CONTROL");
    if (ctrl.isNull()) {
        throw std::runtime_error{"missing CONTROL file"};
    }
    auto ctrlMap = ctrl.readAsText() | fp::splitIntoPairs('\n', ':') | std::ranges::to<std::map>();

    auto abi = zf.getEntry(
        std::format("share/{}/vcpkg_abi_info.txt", fp::mGet(ctrlMap, "Package").value_or("?")));
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

    return {.package = fp::mGet(ctrlMap, "Package").value_or("?"),
            .version = fp::mGet(ctrlMap, "Version").value_or("?"),
            .arch = fp::mGet(ctrlMap, "Architecture").value_or("?"),
            .sha = path.stem().generic_string(),
            .ctrl = ctrlMap,
            .abi = abiMap,
            .time = std::filesystem::last_write_time(path),
            .size = std::filesystem::file_size(path)};
}

}  // namespace vcache
