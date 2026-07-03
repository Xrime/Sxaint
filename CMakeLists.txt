cmake_minimum_required(VERSION 4.3)
project(Sxaint)

set(CMAKE_CXX_STANDARD 20)

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(lz4 CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(MbedTLS CONFIG REQUIRED)


add_executable(Sxaint main.cpp)
