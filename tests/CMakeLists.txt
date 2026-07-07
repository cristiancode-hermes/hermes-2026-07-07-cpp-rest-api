cmake_minimum_required(VERSION 3.28)
project(cpp_rest_api_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Catch2 (FetchContent)
include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.8.0
)
FetchContent_MakeAvailable(Catch2)

# Test target
add_executable(test_api
  test_main.cpp
  ../../src/main.cpp
  ../../include/api/server.hpp
)

target_include_directories(test_api PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${Catch2_SOURCE_DIR}/src
)

target_link_libraries(test_api PRIVATE
  Catch2::Catch2WithMain
  httplib::httplib
  nlohmann_json::nlohmann_json
  spdlog::spdlog
  SQLite::SQLite3
)

# Register tests
include(CTest)
include(Catch)
catch_discover_tests(test_api)
