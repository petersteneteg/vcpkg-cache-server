#pragma once

#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <source_location>

namespace vcache {

namespace log {

template <typename... Args>
struct report {
    explicit report(spdlog::logger& log, spdlog::level::level_enum lvl,
                    fmt::format_string<Args...> format, Args&&... args,
                    std::source_location context = std::source_location::current()) {
        log.log(spdlog::source_loc{context.file_name(), static_cast<int>(context.line()),
                                   context.function_name()},
                lvl, format, std::forward<Args>(args)...);
    }
};
template <typename... Args>
report(spdlog::logger&, spdlog::level::level_enum, fmt::format_string<Args...>, Args&&...)
    -> report<Args...>;

template <typename... Args>
struct trace {
    explicit trace(spdlog::logger& log, fmt::format_string<Args...> format, Args&&... args,
                   std::source_location context = std::source_location::current()) {
        log.log(spdlog::source_loc{context.file_name(), static_cast<int>(context.line()),
                                   context.function_name()},
                spdlog::level::trace, format, std::forward<Args>(args)...);
    }
};
template <typename... Args>
trace(spdlog::logger&, fmt::format_string<Args...>, Args&&...) -> trace<Args...>;

template <typename... Args>
struct debug {
    explicit debug(spdlog::logger& log, fmt::format_string<Args...> format, Args&&... args,
                   std::source_location context = std::source_location::current()) {
        log.log(spdlog::source_loc{context.file_name(), static_cast<int>(context.line()),
                                   context.function_name()},
                spdlog::level::debug, format, std::forward<Args>(args)...);
    }
};
template <typename... Args>
debug(spdlog::logger&, fmt::format_string<Args...>, Args&&...) -> debug<Args...>;

template <typename... Args>
struct info {
    explicit info(spdlog::logger& log, fmt::format_string<Args...> format, Args&&... args,
                  std::source_location context = std::source_location::current()) {
        log.log(spdlog::source_loc{context.file_name(), static_cast<int>(context.line()),
                                   context.function_name()},
                spdlog::level::info, format, std::forward<Args>(args)...);
    }
};
template <typename... Args>
info(spdlog::logger&, fmt::format_string<Args...>, Args&&...) -> info<Args...>;

template <typename... Args>
struct warn {
    explicit warn(spdlog::logger& log, fmt::format_string<Args...> format, Args&&... args,
                  std::source_location context = std::source_location::current()) {
        log.log(spdlog::source_loc{context.file_name(), static_cast<int>(context.line()),
                                   context.function_name()},
                spdlog::level::warn, format, std::forward<Args>(args)...);
    }
};
template <typename... Args>
warn(spdlog::logger&, fmt::format_string<Args...>, Args&&...) -> warn<Args...>;

template <typename... Args>
struct error {
    explicit error(spdlog::logger& log, fmt::format_string<Args...> format, Args&&... args,
                   std::source_location context = std::source_location::current()) {
        log.log(spdlog::source_loc{context.file_name(), static_cast<int>(context.line()),
                                   context.function_name()},
                spdlog::level::err, format, std::forward<Args>(args)...);
    }
};
template <typename... Args>
error(spdlog::logger&, fmt::format_string<Args...>, Args&&...) -> error<Args...>;

template <typename... Args>
struct critical {
    explicit critical(spdlog::logger& log, fmt::format_string<Args...> format, Args&&... args,
                      std::source_location context = std::source_location::current()) {
        log.log(spdlog::source_loc{context.file_name(), static_cast<int>(context.line()),
                                   context.function_name()},
                spdlog::level::err, format, std::forward<Args>(args)...);
    }
};
template <typename... Args>
critical(spdlog::logger&, fmt::format_string<Args...>, Args&&...) -> critical<Args...>;

}  // namespace log

}  // namespace vcache

