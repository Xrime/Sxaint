cmake_minimum_required(VERSION 4.3)
project(Sxaint)

set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig REQUIRED)

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(mbedtls CONFIG REQUIRED)


pkg_check_modules(lz4 REQUIRED IMPORTED_TARGET liblz4)
add_library(sxaint_core STATIC main.cpp
        src/core/file_mapper.cpp
        src/core/thread_pool.cpp
)

target_include_directories(sxaint_core PUBLIC src)
target_compile_definitions(sxaint_core PUBLIC _WIN32_WINNT=0x0A00)
target_link_libraries(
        sxaint_core PUBLIC
        fmt::fmt
        spdlog::spdlog
        PkgConfig::lz4
        zstd::libzstd_shared
        mbedtls
)
add_executable(Sxaint main.cpp
        include/core/file_mapper.h
        src/core/file_mapper.cpp
        include/core/thread_pool.h
        src/core/thread_pool.cpp
        include/core/hasher.h
        include/core/chunker.h
        src/core/hasher.cpp
        src/core/chunker.cpp
        include/core/compressor.h
        src/core/compressor.cpp
        include/net/transport.h)
target_link_libraries(Sxaint PRIVATE sxaint_core)
