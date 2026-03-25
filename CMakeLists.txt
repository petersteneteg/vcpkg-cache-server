cmake_minimum_required(VERSION 3.26.0...3.29)
project(vcpkg-cache-server VERSION 0.0.1 DESCRIPTION "Http binary cache server for vcpkg" LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(vcpkg-cache-server-lib OBJECT)
target_sources(vcpkg-cache-server-lib
    PUBLIC FILE_SET HEADERS TYPE HEADERS BASE_DIRS include FILES
        include/vcpkg-cache-server/database.hpp
        include/vcpkg-cache-server/functional.hpp
        include/vcpkg-cache-server/logging.hpp
        include/vcpkg-cache-server/maintenance.hpp
        include/vcpkg-cache-server/settings.hpp
        include/vcpkg-cache-server/site.hpp
        include/vcpkg-cache-server/store.hpp
    PRIVATE
        src/database.cpp
        src/functional.cpp
        src/logging.cpp
        src/maintenance.cpp
        src/settings.cpp
        src/site.cpp
        src/store.cpp
)

file(DOWNLOAD https://cdn.jsdelivr.net/npm/bootstrap@5.3.8/dist/css/bootstrap.min.css
    ${CMAKE_CURRENT_BINARY_DIR}/resources/bootstrap.min.css
    EXPECTED_HASH SHA256=d85327d99c7a3ee1f9b5d0500d1370acea3ad2db39c163c2f51f232baedbdede)
file(DOWNLOAD https://cdn.jsdelivr.net/npm/htmx.org@2.0.8/dist/htmx.min.js
    ${CMAKE_CURRENT_BINARY_DIR}/resources/htmx.min.js
    EXPECTED_HASH SHA256=22283ef68cb7545914f0a88a1bdedc7256a703d1d580c1d255217d0a50d31313)

target_compile_options(vcpkg-cache-server-lib PUBLIC
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall -Wextra>
)
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    target_compile_options(vcpkg-cache-server-lib PUBLIC /utf-8)
    target_compile_definitions(vcpkg-cache-server-lib PUBLIC
        UNICODE
        _UNICODE
        _USE_MATH_DEFINES
    )
    set_target_properties(vcpkg-cache-server-lib
        PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
else()
    target_compile_options(vcpkg-cache-server-lib PRIVATE
        "--embed-dir=${CMAKE_CURRENT_BINARY_DIR}/resources/"
        -Wno-c23-extensions
    )
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    find_library(CFNETWORK_FRAMEWORK CFNetwork)
    target_link_libraries(vcpkg-cache-server-lib PRIVATE ${CFNETWORK_FRAMEWORK})
    target_compile_options(vcpkg-cache-server-lib PUBLIC -fexperimental-library)
endif()

#External libraries
find_package(httplib CONFIG REQUIRED)
find_package(libzippp CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(SqliteOrm CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

target_link_libraries(vcpkg-cache-server-lib
    PUBLIC
        httplib::httplib
        libzippp::libzippp
        spdlog::spdlog
        yaml-cpp::yaml-cpp
        sqlite_orm::sqlite_orm
)

add_executable(vcpkg-cache-server)
target_sources(vcpkg-cache-server
    PRIVATE
        src/main.cpp
        utf.manifest
        vcpkg.json
        CMakePresets.json
        README.md
)

target_link_libraries(vcpkg-cache-server
    PUBLIC
        vcpkg-cache-server-lib
)

install(TARGETS vcpkg-cache-server
    RUNTIME        # DLLs, Exes
)

# Packaging
set(CPACK_GENERATOR "TGZ")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
if(APPLE)
    set(_cpack_os "macos")
elseif(WIN32)
    set(_cpack_os "windows")
else()
    string(TOLOWER "${CMAKE_SYSTEM_NAME}" _cpack_os)
endif()
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64|ARM64")
    set(CPACK_SYSTEM_NAME "${_cpack_os}-arm64")
else()
    set(CPACK_SYSTEM_NAME "${_cpack_os}-x64")
endif()
include(CPack)

#Testing
option(VCACHE_BUILD_TESTS "Build tests" ON)
if(VCACHE_BUILD_TESTS)
    enable_testing()
    find_package(Catch2 CONFIG REQUIRED)
    include(Catch)

    add_executable(vcpkg-cache-server-tests)
    target_sources(vcpkg-cache-server-tests
        PRIVATE
            tests/test_functional.cpp
            tests/test_yaml_converters.cpp
            tests/test_fmt_formatters.cpp
            tests/test_site_enums.cpp
            tests/test_database.cpp
            tests/test_settings.cpp
    )
    target_link_libraries(vcpkg-cache-server-tests
        PUBLIC
            vcpkg-cache-server-lib
    )

    target_link_libraries(vcpkg-cache-server-tests PRIVATE
        Catch2::Catch2WithMain
    )
    catch_discover_tests(vcpkg-cache-server-tests DISCOVERY_MODE PRE_TEST)

    add_test(
        NAME run-server
        COMMAND vcpkg-cache-server --help
    )
endif()
