cmake_minimum_required(VERSION 3.20)

project(thread_pool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23 REQUIRED)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(thread_pool thread_pool.cpp)
target_link_libraries(thread_pool CATCH2)