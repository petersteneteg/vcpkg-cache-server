#include <vcpkg-cache-server/functional.hpp>

#include <fstream>
#include <string>

#if defined(__linux__)
#include <sys/resource.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach/mach_init.h>
#include <mach/task.h>
#include <sys/resource.h>
#include <unistd.h>
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <sys/resource.h>
#include <unistd.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <psapi.h>
#include <tlhelp32.h>
#include <windows.h>
#endif

namespace vcache::fp {

std::optional<size_t> openFileDescriptors() {
#if defined(__linux__)
    std::error_code ec;
    size_t count = 0;
    for ([[maybe_unused]] const auto& _ :
         std::filesystem::directory_iterator("/proc/self/fd", ec)) {
        ++count;
    }
    // The directory_iterator opens one fd itself, so subtract it from the total.
    return ec ? std::nullopt : std::optional<size_t>{count > 0 ? count - 1 : 0};
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    return static_cast<size_t>(::getdtablecount());
#elif defined(_WIN32)
    DWORD count = 0;
    if (::GetProcessHandleCount(::GetCurrentProcess(), &count)) {
        return static_cast<size_t>(count);
    }
    return std::nullopt;
#else
    return std::nullopt;
#endif
}

std::optional<size_t> threadCount() {
#if defined(__linux__)
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line)) {
        if (line.starts_with("Threads:")) {
            if (auto n = strToNum<size_t>(trim(std::string_view(line).substr(8)))) {
                return n;
            }
            break;
        }
    }
    return std::nullopt;
#elif defined(__APPLE__)
    thread_act_array_t thread_list = nullptr;
    mach_msg_type_number_t count = 0;
    if (task_threads(mach_task_self(), &thread_list, &count) == KERN_SUCCESS) {
        vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(thread_list),
                      sizeof(thread_act_t) * count);
        return static_cast<size_t>(count);
    }
    return std::nullopt;
#elif defined(_WIN32)
    const DWORD pid = ::GetCurrentProcessId();
    HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return std::nullopt;
    THREADENTRY32 te{};
    te.dwSize = sizeof(THREADENTRY32);
    size_t count = 0;
    if (::Thread32First(snapshot, &te)) {
        do {
            if (te.th32OwnerProcessID == pid) ++count;
        } while (::Thread32Next(snapshot, &te));
    }
    ::CloseHandle(snapshot);
    return count;
#else
    return std::nullopt;
#endif
}

std::optional<size_t> memoryUsageBytes() {
#if defined(__linux__)
    struct rusage usage {};
    if (::getrusage(RUSAGE_SELF, &usage) == 0) {
        return static_cast<size_t>(usage.ru_maxrss) * 1024ULL;  // Linux: kB → bytes (peak RSS)
    }
    return std::nullopt;
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    struct rusage usage {};
    if (::getrusage(RUSAGE_SELF, &usage) == 0) {
        return static_cast<size_t>(usage.ru_maxrss);  // macOS/BSD: bytes (peak RSS)
    }
    return std::nullopt;
#elif defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc{};
    if (::GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<size_t>(pmc.WorkingSetSize);
    }
    return std::nullopt;
#else
    return std::nullopt;
#endif
}

long processId() {
#if defined(_WIN32)
    return static_cast<long>(::GetCurrentProcessId());
#else
    return static_cast<long>(::getpid());
#endif
}

}  // namespace vcache::fp
