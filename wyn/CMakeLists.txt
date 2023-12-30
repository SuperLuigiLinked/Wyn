# @file wyn/CMakeLists.txt

# ================================================================================================================================

option(WYN_BACKEND_WIN32 "Enables Wyn Win32 backend")
option(WYN_BACKEND_COCOA "Enables Wyn Cocoa backend")
option(WYN_BACKEND_XLIB  "Enables Wyn Xlib backend")

if (NOT (WYN_BACKEND_WIN32 OR WYN_BACKEND_COCOA OR WYN_BACKEND_XLIB))
    if (WIN32)
        set(WYN_BACKEND_WIN32 ON)
    elseif (APPLE)
        set(WYN_BACKEND_COCOA ON)
    elseif (UNIX)
        set(WYN_BACKEND_XLIB ON)
    endif()
endif()

# ================================================================================================================================

option(WYN_FEATURE_STUBS "Enables default stubs for Wyn callback functions" ON)

if (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    set(WYN_FEATURE_STUBS OFF)
endif()

# ================================================================================================================================

if (WYN_FEATURE_STUBS AND NOT WIN32)
    add_library(wyn OBJECT)
else()
    add_library(wyn)
endif()
add_library(wyn::wyn ALIAS wyn)

# ================================================================================================================================

target_include_directories(wyn PUBLIC "include/")
target_sources(wyn PUBLIC "include/wyn.h" "include/wyc.h")

if (WYN_FEATURE_STUBS)
    target_sources(wyn PRIVATE "src/wyn_stubs.c")
    target_compile_definitions(wyn PUBLIC "WYN_STUBS")
endif()

if (WYN_BACKEND_WIN32)
    target_sources(wyn PRIVATE "src/wyn_win32.c")
    target_link_libraries(wyn PRIVATE "kernel32" "user32")
    target_compile_definitions(wyn PUBLIC "WYN_WIN32")
elseif (WYN_BACKEND_COCOA)
    target_sources(wyn PRIVATE "src/wyn_cocoa.m")
    target_link_libraries(wyn PRIVATE "-framework Cocoa")
    target_compile_definitions(wyn PUBLIC "WYN_COCOA")
elseif (WYN_BACKEND_XLIB)
    target_sources(wyn PRIVATE "src/wyn_xlib.c")
    target_link_libraries(wyn PRIVATE "X11" "Xrandr")
    target_compile_definitions(wyn PUBLIC "WYN_XLIB")
else()
    message(FATAL_ERROR "No Wyn backend selected!")
endif()

# ================================================================================================================================

if (c_std_23 IN_LIST CMAKE_C_COMPILE_FEATURES)
    target_compile_features(wyn PRIVATE c_std_23)
elseif (c_std_17 IN_LIST CMAKE_C_COMPILE_FEATURES)
    target_compile_features(wyn PRIVATE c_std_17)
else()
    target_compile_features(wyn PRIVATE c_std_11)
endif()

# ================================================================================================================================

if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(wyn
        PRIVATE
            "-Weverything"
            "-Wno-c++98-compat"
            "-Wno-pre-c2x-compat"
            "-Wno-declaration-after-statement"
            "-Wno-gnu-empty-initializer"
            "-Wno-unused-macros"
            "-Wno-padded"
            "-Wno-float-equal"
            "-Wno-unsafe-buffer-usage"
    )
elseif (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(wyn
        PRIVATE
            "-Wall"
            "-Wextra"
            "-Wpedantic"
    )
elseif (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(wyn
        PRIVATE
            "/W4"
    )
endif()

# ================================================================================================================================
