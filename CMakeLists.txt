cmake_minimum_required(VERSION 3.28)
project(cpp_rest_api VERSION 1.0.0 LANGUAGES CXX)

# ── C++ Standard ───────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies ───────────────────────────────────────────
include(FetchContent)

# cpp-httplib — single-header HTTP library
FetchContent_Declare(
  cpp-httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG        v0.18.2
)
set(HTTPLIB_USE_NONBLOCKING_IF_OPENSSL OFF CACHE BOOL "")
FetchContent_MakeAvailable(cpp-httplib)

# nlohmann_json
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# spdlog
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.15.1
)
FetchContent_MakeAvailable(spdlog)

# SQLite3 (system)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(SQLITE3 sqlite3)
endif()
if(NOT SQLITE3_FOUND)
  find_package(SQLite3 REQUIRED)
endif()

# ── Sources ────────────────────────────────────────────────
file(GLOB_RECURSE API_SOURCES src/*.cpp)
file(GLOB_RECURSE API_HEADERS include/*.hpp include/*.h)

add_executable(cpp-rest-api
  ${API_SOURCES}
  ${API_HEADERS}
)

target_include_directories(cpp-rest-api PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${HTTPLIB_INCLUDE_DIRS}
)

target_link_libraries(cpp-rest-api PRIVATE
  httplib::httplib
  nlohmann_json::nlohmann_json
  spdlog::spdlog
    SQLite::SQLite3
)

# ── Tests ──────────────────────────────────────────────────
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# ── Install ────────────────────────────────────────────────
install(TARGETS cpp-rest-api DESTINATION bin)
