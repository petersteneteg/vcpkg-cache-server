# Copilot Coding Agent Instructions

## Project Overview

**vcpkg-cache-server** is an HTTPS binary cache server for [vcpkg](https://vcpkg.io), the C++ package manager. It implements the [vcpkg HTTP binary caching protocol](https://learn.microsoft.com/en-us/vcpkg/users/binarycaching#http) and serves as a remote binary cache to speed up C++ builds by reusing compiled binaries.

Key features:
- Cache storage compatible with vcpkg's default file provider (zip files stored as `<cache_dir>/<sha[0:2]>/<sha>.zip`)
- Bearer token authentication for write access over HTTPS
- Web API for querying cache contents (`/match`, `/list`, `/find/<package>`, `/package/<sha>`)
- YAML configuration file support
- SQLite database for tracking downloads and usage statistics

## Language & Standards

- **Language:** C++23 (with `CMAKE_CXX_STANDARD 23`, no extensions)
- **Build System:** CMake 3.26+
- **Package Manager:** vcpkg (dependencies declared in `vcpkg.json`)

## Project Structure

```
vcpkg-cache-server/
├── include/vcpkg-cache-server/    # Public headers
│   ├── resources/                 # Embedded web resources (bootstrap CSS, htmx JS)
│   │   ├── bootstrap.css.hpp
│   │   └── htmx.js.hpp
│   ├── database.hpp               # SQLite ORM schema (Package, Cache, Download tables)
│   ├── functional.hpp             # Utility functions, string ops, YAML converters, fmt formatters
│   ├── settings.hpp               # Configuration structures (Settings, Authorization, Maintenance)
│   ├── site.hpp                   # Web API / HTML rendering interface
│   └── store.hpp                  # Cache storage interface (Store, StoreReader, StoreWriter)
├── src/                           # Implementation files
│   ├── main.cpp                   # Entry point, HTTP server setup, route handlers
│   ├── site.cpp                   # Web API and HTML page rendering
│   ├── store.cpp                  # Cache storage operations (scan, read, write zip files)
│   ├── settings.cpp               # CLI argument parsing and YAML config parsing
│   ├── database.cpp               # Database operations (minimal)
│   └── functional.cpp             # Utility function implementations (minimal)
├── CMakeLists.txt                 # Build configuration
├── CMakePresets.json              # Build presets for MSVC, Xcode, Ninja
├── vcpkg.json                     # vcpkg dependency manifest
├── .clang-format                  # Code formatting (Google-based, 4-space indent, 100 col)
├── .clang-tidy                    # Static analysis configuration
├── .gitignore
├── README.md
├── LICENSE                        # BSD 3-Clause
└── utf.manifest                   # Windows UTF-8 encoding manifest
```

## Dependencies (vcpkg.json)

| Dependency | Purpose |
|------------|---------|
| cpp-httplib | HTTP/HTTPS server (with brotli, openssl, zlib features) |
| openssl | SSL/TLS support (with tools) |
| libzippp | Zip file reading/writing for cache entries |
| spdlog | Logging framework |
| argparse | Command-line argument parsing |
| yaml-cpp | YAML configuration file parsing |
| sqlite-orm | SQLite ORM for download tracking database |
| rapidfuzz | Fuzzy string matching |
| fmt | String formatting |
| fast-float | Fast floating-point parsing |
| catch2 | Testing framework (no tests implemented yet) |

## Building

### Prerequisites
- CMake 3.26+
- A C++23-capable compiler (GCC 14+, Clang 18+, MSVC 2022+)
- vcpkg installed and accessible
- Ninja build system (for Linux preset)

### Build Commands

The project uses CMake presets defined in `CMakePresets.json`. The presets expect vcpkg to be cloned as a sibling directory (`../vcpkg/` relative to the project root).

**Linux (Ninja preset):**
```bash
cmake --preset ninja-developer
cmake --build ../builds/vcpkg-cache-server-ninja-developer
```

**If vcpkg is installed elsewhere**, configure manually:
```bash
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-linux \
  -DBUILD_SHARED_LIBS=OFF
cmake --build build
```

**Other presets:**
- `msvc-developer` — Visual Studio 2022, x64-windows-static
- `xcode-developer-arm64` — Xcode for ARM64 macOS
- `xcode-developer-x64` — Xcode for x64 macOS

### Known Build Issues

1. **vcpkg toolchain path:** The CMake presets reference `${sourceParentDir}/vcpkg/scripts/buildsystems/vcpkg.cmake`, which expects vcpkg cloned as a sibling directory. If vcpkg is installed elsewhere (e.g., `/usr/local/share/vcpkg`), you must set `CMAKE_TOOLCHAIN_FILE` manually or symlink vcpkg to the expected location.

2. **Network-dependent dependency installation:** vcpkg downloads and builds all dependencies during the CMake configure step. This requires internet access. In sandboxed or air-gapped environments, the configure step will fail with download errors (e.g., `curl operation failed with error code 6`). Pre-install dependencies or use a vcpkg binary cache to work around this.

3. **C++23 requirement:** Some compilers may not fully support C++23 features used in this project (e.g., `std::ranges::to`, `std::to_underlying`). The code includes a workaround for `ranges::to` in `functional.hpp` (`fp::fromRange` helper for GCC 14 compatibility).

4. **Clang `__int128_t` streaming:** The `database.hpp` file includes a custom `operator<<` for `__int128_t` because clang's standard library doesn't provide one. This operator must be defined before including `sqlite_orm/sqlite_orm.h`.

## Testing

- **Framework:** Catch2 is listed as a dependency, but **no tests are currently implemented** in the repository.
- There is no `tests/` directory, no test source files, and no CTest configuration.
- When adding tests, follow the Catch2 convention. Create test files in a `tests/` directory and add them to `CMakeLists.txt` with `add_executable` and link against `Catch2::Catch2WithMain`.

## Code Style & Linting

### Formatting (clang-format)
- Based on Google style with modifications
- 4-space indentation, 100-column limit
- Pointer alignment: Left (`int* ptr`)
- Qualifier alignment: Left
- No automatic include sorting
- Run: `clang-format -i src/*.cpp include/vcpkg-cache-server/*.hpp`

### Static Analysis (clang-tidy)
- Extensive checks enabled: bugprone, cppcoreguidelines, google, llvm, misc, modernize, performance, portability, readability
- Notable disabled checks: `modernize-use-nodiscard`, `modernize-use-trailing-return-type`, `readability-magic-numbers`, `readability-identifier-length`
- Run: `clang-tidy src/*.cpp -- -std=c++23 -Iinclude $(pkg-config-flags)`

### Code Conventions
- All project code is in the `vcache` namespace
- Utility/functional helpers are in `vcache::fp` namespace
- Site/web API code is in `vcache::site` namespace
- Database types/functions are in `vcache::db` namespace
- Headers use `#pragma once` include guards
- `using Time = std::filesystem::file_time_type` is defined in multiple headers (consistent alias)
- Custom `fmt::formatter` specializations are used extensively for domain types
- YAML converters are defined in the `YAML` namespace for custom types (`ByteSize`, durations)
- Transparent hash maps (`UnorderedStringMap`) use `StringHash` with `is_transparent` for heterogeneous lookup

## CI/CD

There are currently **no CI/CD workflows** configured (no `.github/workflows/` directory). The project has no automated build, test, or lint pipelines.

## Architecture Notes

### HTTP Server (main.cpp)
- Uses cpp-httplib (`httplib::SSLServer` or `httplib::Server`)
- Routes: `GET/HEAD/PUT /cache/:sha` for vcpkg binary cache protocol
- Web routes: `/`, `/match`, `/list`, `/find/:package`, `/package/:sha`, `/downloads`
- Bearer token authentication on PUT requests
- Background maintenance thread for cache cleanup

### Cache Storage (store.hpp/store.cpp)
- Thread-safe with `std::shared_mutex` for concurrent read/write
- Zip files contain package metadata (ABI info, control files)
- `Store::allInfos()` returns a lock-wrapped range view

### Database (database.hpp)
- SQLite via sqlite_orm, schema defined inline in `database.hpp`
- Three tables: `packages`, `caches`, `downloads`
- `Database` type is `decltype(create({}))` — the type is inferred from the storage definition

### Configuration (settings.hpp/settings.cpp)
- CLI arguments via argparse library
- YAML config file support with custom type converters
- Settings include: cache directory, port, host, SSL cert/key, auth tokens, log settings, maintenance policies
