# ============================================================================
# AuraCinemas — C++ Backend Build Configuration
# Uses: C++20, Crow (HTTP), SQLite3, nlohmann/json
# ============================================================================
cmake_minimum_required(VERSION 3.20)
project(AuraCinemas VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(AURA_BUILD_TESTS  "Build unit tests"        OFF)
option(AURA_ENABLE_ASAN  "Enable AddressSanitizer" OFF)

if(AURA_ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

# ---------------------------------------------------------------------------
# SQLite3
# ---------------------------------------------------------------------------
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
    message(STATUS "Using bundled sqlite3.c")
    add_library(sqlite3 STATIC ${CMAKE_SOURCE_DIR}/third_party/sqlite/sqlite3.c)
    target_include_directories(sqlite3 PUBLIC ${CMAKE_SOURCE_DIR}/third_party/sqlite)
    if(UNIX)
        target_link_libraries(sqlite3 PUBLIC pthread dl m)
    endif()
    add_library(SQLite::SQLite3 ALIAS sqlite3)
endif()

# ---------------------------------------------------------------------------
# Crow — header-only HTTP framework (single-header or include dir)
# Place crow_all.h in third_party/crow/
# ---------------------------------------------------------------------------
add_library(crow INTERFACE)
target_include_directories(crow INTERFACE ${CMAKE_SOURCE_DIR}/third_party/crow)

# Crow needs Boost.Asio OR standalone Asio. We use CROW_USE_BOOST=0 for
# Crow's built-in ASIO bundled in crow_all.h (Crow >= 1.0 bundles it).
target_compile_definitions(crow INTERFACE CROW_DISABLE_STATIC_DIR)

# ---------------------------------------------------------------------------
# nlohmann/json
# ---------------------------------------------------------------------------
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${CMAKE_SOURCE_DIR}/third_party/json)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
set(AURA_SOURCES src/main.cpp)

add_executable(aura_server ${AURA_SOURCES})

target_include_directories(aura_server PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(aura_server PRIVATE
    SQLite::SQLite3
    crow
    nlohmann_json
)

# Threading
find_package(Threads REQUIRED)
target_link_libraries(aura_server PRIVATE Threads::Threads)

if(WIN32)
    target_compile_definitions(aura_server PRIVATE
        _WIN32_WINNT=0x0A00
        NOMINMAX
    )
    target_link_libraries(aura_server PRIVATE ws2_32 wsock32)
endif()

# Warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(aura_server PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(aura_server PRIVATE /W4 /permissive-)
endif()

install(TARGETS aura_server RUNTIME DESTINATION bin)

if(AURA_BUILD_TESTS)
    enable_testing()
endif()
