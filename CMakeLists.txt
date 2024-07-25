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
    PRIVATE
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


target_include_directories(vcpkg-cache-server PUBLIC "include")
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
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    target_compile_options(vcpkg-cache-server PUBLIC -fexperimental-library)
endif()

# External libraries
find_package(httplib CONFIG REQUIRED)
find_package(libzippp CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
target_link_libraries(vcpkg-cache-server PUBLIC httplib::httplib libzippp::libzippp spdlog::spdlog yaml-cpp::yaml-cpp)
set_target_properties(vcpkg-cache-server PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

install(TARGETS vcpkg-cache-server
    RUNTIME        # DLLs, Exes
)
