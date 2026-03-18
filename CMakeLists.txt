cmake_minimum_required(VERSION 3.26.0...3.29)
project(vcpkg-cache-server VERSION 0.0.1 DESCRIPTION "Http binary cache server for vcpkg" LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(vcpkg-cache-server)
target_sources(vcpkg-cache-server
    PUBLIC FILE_SET HEADERS TYPE HEADERS BASE_DIRS include FILES
        include/vcpkg-cache-server/settings.hpp
        include/vcpkg-cache-server/functional.hpp
        include/vcpkg-cache-server/store.hpp
        include/vcpkg-cache-server/site.hpp
        include/vcpkg-cache-server/database.hpp
    PRIVATE
        src/database.cpp
        src/settings.cpp
        src/functional.cpp
        src/store.cpp
        src/site.cpp
        src/main.cpp
        utf.manifest
        vcpkg.json
        CMakePresets.json
        README.md
)

file(DOWNLOAD https://cdn.jsdelivr.net/npm/bootstrap@5.3.8/dist/css/bootstrap.min.css
    ${CMAKE_CURRENT_BINARY_DIR}/resources/bootstrap.min.css
    EXPECTED_HASH SHA256=d85327d99c7a3ee1f9b5d0500d1370acea3ad2db39c163c2f51f232baedbdede)
file(DOWNLOAD https://cdn.jsdelivr.net/npm/htmx.org@2.0.8/dist/htmx.min.js
    ${CMAKE_CURRENT_BINARY_DIR}/resources/htmx.min.js
    EXPECTED_HASH SHA256=22283ef68cb7545914f0a88a1bdedc7256a703d1d580c1d255217d0a50d31313)

target_include_directories(vcpkg-cache-server PUBLIC "resources")
target_compile_options(vcpkg-cache-server PUBLIC
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall -Wextra>
)
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    target_compile_options(vcpkg-cache-server PUBLIC /utf-8)
    target_compile_definitions(vcpkg-cache-server PRIVATE
        UNICODE
        _UNICODE
        _USE_MATH_DEFINES
    )
else()
    target_compile_options(vcpkg-cache-server PRIVATE
        "--embed-dir=${CMAKE_CURRENT_BINARY_DIR}/resources/"
        -Wno-c23-extensions
    )
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    find_library(CFNETWORK_FRAMEWORK CFNetwork)
    target_link_libraries(vcpkg-cache-server PRIVATE ${CFNETWORK_FRAMEWORK})

    target_compile_options(vcpkg-cache-server PUBLIC -fexperimental-library)
endif()

# External libraries
find_package(httplib CONFIG REQUIRED)
find_package(libzippp CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(SqliteOrm CONFIG REQUIRED)

target_link_libraries(vcpkg-cache-server PUBLIC httplib::httplib libzippp::libzippp spdlog::spdlog yaml-cpp::yaml-cpp sqlite_orm::sqlite_orm)
set_target_properties(vcpkg-cache-server PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

install(TARGETS vcpkg-cache-server
    RUNTIME        # DLLs, Exes
)
