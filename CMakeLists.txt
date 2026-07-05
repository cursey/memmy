cmake_minimum_required(VERSION 3.20)
project(memmy C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

add_subdirectory(vendor)
add_subdirectory(base)
add_subdirectory(memmy)
add_subdirectory(memmy_dsl)
add_subdirectory(memmy_exec)
add_subdirectory(memmy_cli)
add_subdirectory(cmd)
add_subdirectory(unittest)
