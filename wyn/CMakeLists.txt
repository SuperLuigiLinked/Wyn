# @file wyn/CMakeLists.txt

# ================================================================================================================================

set(WYN_USE_STUBS ON CACHE BOOL "Controls whether or not Default Stubs for Wyn Callbacks are defined.")

# ================================================================================================================================

add_library(wyn)
add_library(wyn::wyn ALIAS wyn)

# ================================================================================================================================

target_include_directories(wyn PUBLIC "include/")
target_sources(wyn PUBLIC "include/wyn.h")

if (WYN_USE_STUBS)
    # message(STATUS "Using `wyn_stubs.c`.")
    target_sources(wyn PRIVATE "src/wyn_stubs.c")
endif()

if (WIN32)
    target_sources(wyn PRIVATE "src/wyn_win32.c")
    target_link_libraries(wyn PRIVATE "kernel32" "user32")
elseif (APPLE)
    target_sources(wyn PRIVATE "src/wyn_cocoa.m")
    target_link_libraries(wyn "-framework Cocoa")
elseif (UNIX)
    target_sources(wyn PRIVATE "src/wyn_xlib.c")
    target_link_libraries(wyn "X11")
endif()

# ================================================================================================================================

target_compile_features(wyn PRIVATE c_std_23)

target_compile_options(wyn
    PRIVATE
        "-Weverything"
        "-Wno-c++98-compat"
        "-Wno-c++17-extensions"
        "-Wno-pre-c2x-compat"
        "-Wno-fixed-enum-extension"
        "-Wno-microsoft-fixed-enum"
        "-Wno-declaration-after-statement"
        "-Wno-gnu-empty-initializer"
        "-Wno-unused-macros"
        "-Wno-padded"
)

# ================================================================================================================================
