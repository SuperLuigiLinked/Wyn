# @file CMakeLists.txt

# ================================================================================================================================

cmake_minimum_required(VERSION 3.25)
project(Wyn)

# ================================================================================================================================

# Build options
option(WYN_BUILD_WYN "Build Wyn library" ON)
option(WYN_BUILD_WYT "Build Wyt library" ON)
option(WYN_BUILD_EXAMPLES "Build Wyn example programs" OFF)

# Example options
option(WYN_EXAMPLE_C "Enables Wyn C example program" ON)
option(WYN_EXAMPLE_CPP "Enables Wyn C++ example program" ON)

# Wyn backends [Select one or less]
option(WYN_BACKEND_WIN32 "Enables Wyn Win32 backend")
option(WYN_BACKEND_COCOA "Enables Wyn Cocoa backend")
option(WYN_BACKEND_XLIB "Enables Wyn Xlib backend")

# Wyt backends [Select one or less]
option(WYT_BACKEND_WIN32 "Enables Wyt Win32 backend")
option(WYT_BACKEND_PTHREADS "Enables Wyt Pthreads backend")

# Wyn options
option(WYN_FEATURE_STUBS "Enables default stubs for Wyn callback functions" ON)

# --------------------------------------------------------------------------------------------------------------------------------

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(WYN_BUILD_EXAMPLES ON)
endif()

if (NOT (WYN_BACKEND_WIN32 OR WYN_BACKEND_COCOA OR WYN_BACKEND_XLIB))
    if (WIN32)
        set(WYN_BACKEND_WIN32 ON)
    elseif (APPLE)
        set(WYN_BACKEND_COCOA ON)
    else()
        set(WYN_BACKEND_XLIB ON)
    endif()
endif()

if (NOT (WYT_BACKEND_WIN32 OR WYT_BACKEND_PTHREADS))
    if (WIN32)
        set(WYT_BACKEND_WIN32 ON)
    else()
        set(WYT_BACKEND_PTHREADS ON)
    endif()
endif()

if (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    set(WYN_FEATURE_STUBS OFF)
endif()

# ================================================================================================================================

if (c_std_23 IN_LIST CMAKE_C_COMPILE_FEATURES)
    set(WYN_STANDARD_C c_std_23)
elseif (c_std_17 IN_LIST CMAKE_C_COMPILE_FEATURES)
    set(WYN_STANDARD_C c_std_17)
else()
    set(WYN_STANDARD_C c_std_11)
endif()

# --------------------------------------------------------------------------------------------------------------------------------

if (cxx_std_23 IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    set(WYN_STANDARD_CPP cxx_std_23)
elseif (cxx_std_20 IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    set(WYN_STANDARD_CPP cxx_std_20)
elseif (cxx_std_17 IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    set(WYN_STANDARD_CPP cxx_std_17)
elseif (cxx_std_14 IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    set(WYN_STANDARD_CPP cxx_std_14)
else()
    set(WYN_STANDARD_CPP cxx_std_11)
endif()

# --------------------------------------------------------------------------------------------------------------------------------

# set(WYN_STANDARD_C c_std_11)
# set(WYN_STANDARD_CPP cxx_std_11)

# ================================================================================================================================

if (CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(WYN_WARNINGS_C
        "-Weverything"
        "-Wno-c++98-compat"
        "-Wno-declaration-after-statement"
        "-Wno-float-equal"
        "-Wno-padded"
        "-Wno-unsafe-buffer-usage"
        "-Wno-unused-macros"
    )
elseif ((CMAKE_C_COMPILER_ID STREQUAL "GNU") OR (CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "GNU"))
    set(WYN_WARNINGS_C
        "-Wall"
        "-Wextra"
        "-Wshadow"
        "-Wpedantic"
        "-pedantic"
    )
elseif ((CMAKE_C_COMPILER_ID STREQUAL "MSVC") OR (CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"))
    set(WYN_WARNINGS_C
        "/W4"
    )
endif()

# --------------------------------------------------------------------------------------------------------------------------------

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(WYN_WARNINGS_CPP
        "-Weverything"
        "-Wno-c++98-compat"
        "-Wno-c++98-compat-pedantic"
        "-Wno-c++20-compat"
        "-Wno-unsafe-buffer-usage"
        "-Wno-unused-macros"
    )
elseif ((CMAKE_CXX_COMPILER_ID STREQUAL "GNU") OR (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU"))
    set(WYN_WARNINGS_CPP
        "-Wall"
        "-Wextra"
        "-Wshadow"
        "-Wpedantic"
        "-pedantic"
    )
elseif ((CMAKE_CXX_COMPILER_ID STREQUAL "MSVC") OR (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"))
    set(WYN_WARNINGS_CPP
        "/W4"
    )
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

if (0)
    include(CMakePrintHelpers)
    cmake_print_variables(WYN_BUILD_WYN WYN_BUILD_WYT WYN_BUILD_EXAMPLES)
    cmake_print_variables(WYN_EXAMPLE_C WYN_EXAMPLE_CPP)
    cmake_print_variables(WYN_BACKEND_WIN32 WYN_BACKEND_COCOA WYN_BACKEND_XLIB)
    cmake_print_variables(WYT_BACKEND_WIN32 WYT_BACKEND_PTHREADS)
    cmake_print_variables(WYN_FEATURE_STUBS)
    cmake_print_variables(WYN_STANDARD_C WYN_WARNINGS_C)
    cmake_print_variables(WYN_STANDARD_CPP WYN_WARNINGS_CPP)
    cmake_print_variables(CMAKE_C_COMPILER_ID CMAKE_C_COMPILER_FRONTEND_VARIANT)
    cmake_print_variables(CMAKE_CXX_COMPILER_ID CMAKE_CXX_COMPILER_FRONTEND_VARIANT)
endif()

# ================================================================================================================================
