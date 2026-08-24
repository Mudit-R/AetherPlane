cmake_minimum_required(VERSION 3.20)
project(OpenPathX VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler optimization flags
if (MSVC)
    add_compile_options(/W4 /O2 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O3 -march=native)
endif()

include_directories(include)

# Source files
set(SOURCES
    src/core/packet.cpp
    src/core/lpm_trie.cpp
    src/core/filter_engine.cpp
    src/core/xdp_hook.cpp
    src/core/virtual_netdev.cpp
    src/qos/flow_classifier.cpp
    src/qos/smart_scheduler.cpp
    src/telemetry/metrics_collector.cpp
)

# Core library
add_library(openpath_core STATIC ${SOURCES})

# Main executable
add_executable(openpath_daemon src/main.cpp)
target_link_libraries(openpath_daemon PRIVATE openpath_core)

if (WIN32)
    target_link_libraries(openpath_daemon PRIVATE ws2_32)
endif()
