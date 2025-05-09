#include <vcpkg-cache-server/store.hpp>

#include <libzippp.h>

#include <fmt/format.h>
#include <fmt/std.h>

#include <numeric>
#include <ranges>
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
    {
        std::shared_lock lock{smtx};
        if (auto it = infos.find(sha); it != infos.end() && it->second.first == InfoState::Valid) {
            return &it->second.second;
        }
    }

    const auto path = shaToPath(sha);
    if (std::filesystem::is_regular_file(path)) {
        const auto info = extractInfo(path);

        std::scoped_lock lock{smtx};
        auto [it, inserted] = infos.try_emplace(info.sha, InfoState::Valid, info);
        return &it->second.second;
    }

    return nullptr;
}
const Info* Store::info(std::string_view sha) const {
    std::shared_lock<std::shared_mutex> lock{smtx};
    if (auto it = infos.find(sha); it != infos.end() && it->second.first == InfoState::Valid) {
        return &it->second.second;
    } else {
        return nullptr;
    }
}

std::shared_ptr<StoreReader> Store::read(std::string_view sha) {
    std::shared_lock<std::shared_mutex> lock{smtx};

    if (auto it = infos.find(sha); it != infos.end() && it->second.first == InfoState::Valid) {
        return std::make_shared<StoreReader>(*this, it->second, Token{});
    } else {
        return nullptr;
    }
}

std::shared_ptr<StoreWriter> Store::write(std::string_view sha) {
    std::scoped_lock lock{smtx};

    if (auto it = infos.find(sha); it != infos.end()) {
        if (it->second.first == InfoState::Valid || it->second.first == InfoState::Writing) {
            return nullptr;
        } else if (it->second.first == InfoState::Deleted) {
            it->second.first = InfoState::Writing;
            return std::make_shared<StoreWriter>(*this, it->second, shaToPath(sha), Token{});
        }
    }

    const auto path = shaToPath(sha);
    if (std::filesystem::is_regular_file(path)) {
        auto info = extractInfo(path);
        infos.try_emplace(info.sha, InfoState::Valid, info);
        return nullptr;
    }

    auto [it, inserted] = infos.try_emplace(std::string{sha}, InfoState::Valid, Info{});

    return std::make_shared<StoreWriter>(*this, it->second, path, Token{});
}

std::string Store::statistics() const {
    const auto diskSize = std::ranges::fold_left(allInfos() | std::views::transform(&Info::size),
                                                 size_t{0}, std::plus<>{});
    const auto packages =
        fp::fromRange<std::set<std::string>>(allInfos() | std::views::transform(&Info::package));

    return fmt::format("Found {} caches of {} packages. Using {}", infos.size(), packages.size(),
                       ByteSize{diskSize});
}

std::filesystem::path Store::shaToPath(std::string_view sha) const {
    return root / sha.substr(0, 2) / fmt::format("{}.zip", sha);
}

void Store::remove(std::string_view sha) {
    std::scoped_lock lock{smtx};
    if (auto it = infos.find(sha); it != infos.end()) {
        if (it->second.first == InfoState::Valid) {
            it->second.first = InfoState::Deleted;
            const auto path = shaToPath(sha);

            log->info("Deleting: {}", path);
            std::filesystem::remove(path);
        }
    }
}

fp::UnorderedStringMap<std::pair<InfoState, Info>> scan(const std::filesystem::path& path,
                                                        std::shared_ptr<spdlog::logger> log) {
    return fp::fromRange<fp::UnorderedStringMap<std::pair<InfoState, Info>>>(
        std::filesystem::recursive_directory_iterator(path) | std::views::filter(fp::isZipFile) |
        fp::tryTransform(
            [&](const auto& entry) {
                log->debug("scan: {}", entry.path().stem().generic_string());
                return extractInfo(entry.path());
            },
            [&](const auto& entry) {
                log->error("error scaning {} : {}, removing entry", entry.path(),
                           fp::exceptionToString());
                std::filesystem::remove(entry.path());
            }) |
        std::views::transform([&](auto&& info) {
            return std::pair<const std::string, std::pair<InfoState, Info>>{
                info.sha, {InfoState::Valid, info}};
        }));
}

Info extractInfo(const std::filesystem::path& path) {
    libzippp::ZipArchive zf{path.generic_string()};
    if (!zf.open(libzippp::ZipArchive::ReadOnly)) {
        throw std::runtime_error(fmt::format("Unable to open file {}", path));
    }

    auto ctrl = zf.getEntry("CONTROL");
    if (ctrl.isNull()) {
        throw std::runtime_error{"missing CONTROL file"};
    }
    auto ctrlMap = fp::fromRange<std::map<std::string, std::string>>(ctrl.readAsText() |
                                                                     fp::splitIntoPairs('\n', ':'));

    auto abi = zf.getEntry(
        fmt::format("share/{}/vcpkg_abi_info.txt", fp::mGet(ctrlMap, "Package").value_or("?")));
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
    auto abiMap = fp::fromRange<std::map<std::string, std::string>>(abi.readAsText() |
                                                                    fp::splitIntoPairs('\n', ' '));

    return {.package = fp::mGet(ctrlMap, "Package").value_or("?"),
            .version = fp::mGet(ctrlMap, "Version").value_or("?"),
            .arch = fp::mGet(ctrlMap, "Architecture").value_or("?"),
            .sha = path.stem().generic_string(),
            .ctrl = ctrlMap,
            .abi = abiMap,
            .time = std::filesystem::last_write_time(path),
            .size = std::filesystem::file_size(path)};
}

StoreWriter::StoreWriter(Store& store, std::pair<InfoState, Info>& infoItem,
                         const std::filesystem::path& path, typename Store::Token)
    : store{store}, infoItem{infoItem}, path{path}, stream{[&]() {
        std::filesystem::create_directories(path.parent_path());
        return std::ofstream{path, std::ios_base::out | std::ios_base::binary};
    }()} {

    if (!stream.good()) {
        throw std::runtime_error(fmt::format("Unable to open file for writing {}", path));
    }
}

StoreWriter::~StoreWriter() {
    try {
        stream.close();
        if (!stream.good()) {
            throw std::runtime_error(fmt::format("Unable to close file {}", path));
        }
        infoItem.second = extractInfo(path);
        {
            std::scoped_lock lock{store.smtx};
            infoItem.first = InfoState::Valid;
        }
    } catch (const std::exception& e) {
        store.log->error("Unable to close writer of: {} due to: {}", path, e.what());
    }
}

}  // namespace vcache
