# @file wyn/CMakeLists.txt

# ================================================================================================================================

set(WYN_USE_STUBS ON CACHE BOOL "Controls whether or not Default Stubs for Wyn Callbacks are defined.")

set(WYN_USE_WIN32 OFF CACHE BOOL "Win32 backend toggle.")
set(WYN_USE_COCOA OFF CACHE BOOL "Cocoa backend toggle.")
set(WYN_USE_XLIB  OFF CACHE BOOL "Xlib backend toggle.")

if (NOT (WYN_USE_WIN32 OR WYN_USE_COCOA OR WYN_USE_XLIB))
    if (WIN32)
        set(WYN_USE_WIN32 ON)
    elseif (APPLE)
        set(WYN_USE_COCOA ON)
    elseif (UNIX)
        set(WYN_USE_XLIB ON)
    endif()
endif()

# ================================================================================================================================

if (WYN_USE_STUBS AND NOT WIN32)
    add_library(wyn OBJECT)
else()
    add_library(wyn)
endif()
add_library(wyn::wyn ALIAS wyn)

# ================================================================================================================================

target_include_directories(wyn PUBLIC "include/")
target_sources(wyn PUBLIC "include/wyn.h" "include/wyc.h")

if (WYN_USE_STUBS)
    target_sources(wyn PRIVATE "src/wyn_stubs.c")
    target_compile_definitions(wyn PUBLIC "WYN_STUBS")
endif()

if (WYN_USE_WIN32)
    target_sources(wyn PRIVATE "src/wyn_win32.c")
    target_link_libraries(wyn PRIVATE "kernel32" "user32")
    target_compile_definitions(wyn PUBLIC "WYN_WIN32")
elseif (WYN_USE_COCOA)
    target_sources(wyn PRIVATE "src/wyn_cocoa.m")
    target_link_libraries(wyn PRIVATE "-framework Cocoa")
    target_compile_definitions(wyn PUBLIC "WYN_COCOA")
elseif (WYN_USE_XLIB)
    target_sources(wyn PRIVATE "src/wyn_xlib.c")
    target_link_libraries(wyn PRIVATE "X11" "Xrandr")
    target_compile_definitions(wyn PUBLIC "WYN_XLIB")
else()
    message(FATAL_ERROR "No Wyn backend selected!")
endif()

if (UNIX AND NOT APPLE)
    target_link_libraries(wyn PRIVATE "m") # <math.h>
endif()

# ================================================================================================================================

target_compile_features(wyn PRIVATE c_std_23)

target_compile_options(wyn
    PRIVATE
        "-Weverything"
        "-Wno-c++98-compat"
        "-Wno-pre-c2x-compat"
        "-Wno-declaration-after-statement"
        "-Wno-gnu-empty-initializer"
        "-Wno-unused-macros"
        "-Wno-padded"
        "-Wno-unsafe-buffer-usage"
)

# ================================================================================================================================
