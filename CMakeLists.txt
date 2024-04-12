cmake_minimum_required(VERSION 3.26.0...3.29)
project(vcpkg-cache-server VERSION 0.0.1 DESCRIPTION "Http binary cache server for vcpkg" LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(vcpkg-cache-server
    src/main.cpp
    utf.manifest
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

# External libraries
find_package(httplib CONFIG REQUIRED)
find_package(libzippp CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
target_link_libraries(vcpkg-cache-server PUBLIC httplib::httplib libzippp::libzippp spdlog::spdlog)
set_target_properties(vcpkg-cache-server PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
