# @file CMakeLists.txt

# ================================================================================================================================

cmake_minimum_required(VERSION 3.25)
project(Wyn)

# ================================================================================================================================

option(WYN_BUILD_WYN "Build Wyn library" ON)
option(WYN_BUILD_WYT "Build Wyt library" ON)
option(WYN_BUILD_EXAMPLES "Build Wyn example programs" OFF)

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(WYN_BUILD_EXAMPLES ON)
endif()

# ================================================================================================================================

if (WYN_BUILD_WYN)
    add_subdirectory(wyn)
endif()

if (WYN_BUILD_WYT)
    add_subdirectory(wyt)
endif()

if (WYN_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ================================================================================================================================
