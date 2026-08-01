cmake_minimum_required(VERSION 3.20)

project(thread_pool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23 REQUIRED)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(CATCH2
  GIT_REPOSITORY  https://github.com/catchorg/Catch2.git
  GIT_TAG 8b08d4d79514f45f7e4ce2a607ac9c94e920d1bb
)
FetchContent_MakeAvailable(CATCH2)

add_executable(thread_pool thread_pool.cpp)
target_link_libraries(thread_pool CATCH2)