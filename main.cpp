#include <httplib.h>
#include <libzippp.h>

#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <string>
#include <string_view>
#include <ranges>
#include <format>
#include <print>

constexpr std::pair<std::string_view, std::string_view> splitByFirst(std::string_view str,
                                                                     char delimiter = ' ') {
    const auto pos = str.find(delimiter);
    return {str.substr(0, pos), pos != str.npos ? str.substr(pos + 1) : std::string_view{}};
}

// 00b98928f1cb8655797797428ad21a0aad56bec9d0613d6d0dfab20307b97faa

const std::filesystem::path base =
    "C:/Users/petst55.AD/OneDrive - Linköpings universitet/Cache/vcpkg-cache";

std::filesystem::path shaToPath(std::string_view sha) {
    return base / sha.substr(0, 2) / std::format("{}.zip", sha);
}

bool exists(std::string_view sha) { return std::filesystem::is_regular_file(shaToPath(sha)); }

std::unordered_map<std::string, std::string> controlInfo(const std::filesystem::path& path) {
    libzippp::ZipArchive zf{path.generic_string()};
    zf.open(libzippp::ZipArchive::ReadOnly);
    auto ctrl = zf.getEntry("CONTROL");
    if (ctrl.isNull()) {
        return {};
    }

    return ctrl.readAsText() | std::views::split('\n') | std::views::transform([](auto&& line) {
               auto [key, value] = splitByFirst(std::string_view(line), ':');
               return std::pair<const std::string, std::string>(key, value);
           }) |
           std::ranges::to<std::unordered_map>();
}

const auto logger = [](const httplib::Request& req, const httplib::Response& res) {
    std::println("================================");
    std::println("{} {} {}", req.method, req.version, req.path);
    for (auto it = req.params.begin(); it != req.params.end(); ++it) {
        std::println("{}{}={}", (it == req.params.begin()) ? '?' : '&', it->first, it->second);
    }
    for (const auto& [key, val] : req.headers) {
        std::println("{}: {}", key, val);
    }
    for (const auto& [name, file] : req.files) {
        std::println("name: %s\n", name);
        std::println("filename: %s\n", file.filename);
        std::println("content type: %s\n", file.content_type);
        std::println("text length: %zu\n", file.content.size());
        std::println("----------------");
    }
    std::println("--------------------------------");
    std::println("{}", res.status);
    for (const auto& [key, val] : res.headers) {
        std::println("{}: {}", key, val);
    }
};

int main(void) {
    // httplib::SSLServer server{};

    httplib::Server server{};

    // server.set_logger(logger);

    server.Get(R"(/cache/([0-9a-f]{64}))", [&](const httplib::Request& req,
                                               httplib::Response& res) {
        auto sha = req.matches[1].str();
        const auto file = shaToPath(sha);
        const auto exists = std::filesystem::is_regular_file(file);

        if (!exists) {
            res.status = httplib::StatusCode::NotFound_404;
            return;
        }

        auto info = controlInfo(file);
        const auto time = std::filesystem::last_write_time(file);
        const auto size = std::filesystem::file_size(file);
        std::println("{:5} Package: {:20} Version: {:7} Arch: {:15} Time: {:20} Size: {:10} Sha:{}",
                     req.method, info["Package"], info["Version"], info["Architecture"], time, size,
                     req.path);

        auto fstream =
            std::make_shared<std::fstream>(file, std::ios_base::in | std::ios_base::binary);

        std::println("Get {} {}", sha, size);

        res.set_content_provider(
            size, "application/zip",
            [size, fstream, buff = std::vector<char>(1024)](
                size_t offset, size_t length, httplib::DataSink& sink) mutable -> bool {
                std::println("Get callback {} {} {}", size, offset, length);
                buff.resize(length);
                fstream->seekg(offset);
                fstream->read(buff.data(), length);
                sink.write(buff.data(), length);
                return true;
            });
    });

    server.set_error_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            std::format("<p>Error Status: <span style='color:red;'>{}</span></p>", res.status),
            "text/html");
    });
    server.set_exception_handler(
        [](const httplib::Request& req, httplib::Response& res, std::exception_ptr ep) {
            try {
                std::rethrow_exception(ep);
            } catch (std::exception& e) {
                res.set_content(std::format("<h1>Error 500</h1><p>{}</p>", e.what()), "text/html");
            } catch (...) {  // See the following NOTE
                res.set_content("<h1>Error 500</h1>Unknown error</p>", "text/html");
            }
            res.status = httplib::StatusCode::InternalServerError_500;
        });

    /*
    // Capture the second segment of the request path as "id" path param
    svr.Get("/users/:id", [&](const Request& req, Response& res) {
      auto user_id = req.path_params.at("id");
      res.set_content(user_id, "text/plain");
    });

    // Extract values from HTTP headers and URL query params
    svr.Get("/body-header-param", [](const Request& req, Response& res) {
      if (req.has_header("Content-Length")) {
        auto val = req.get_header_value("Content-Length");
      }
      if (req.has_param("key")) {
        auto val = req.get_param_value("key");
      }
      res.set_content(req.body, "text/plain");
    });
    */

    server.Get("/stop",
               [&](const httplib::Request& req, httplib::Response& res) { server.stop(); });

    std::println("Start server");
    server.listen("localhost", 1234);
}
