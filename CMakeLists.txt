cmake_minimum_required(VERSION 3.23)
project(wyn)

set(CMAKE_C_STANDARD 23)

# ================================================================ #

add_library(wyn)
target_sources(wyn PUBLIC "include/wyn.h")

if (WIN32)
    target_sources(wyn PRIVATE "src/wyn_win32.c")
elseif (APPLE)
    target_sources(wyn PRIVATE "src/wyn_cocoa.m")
    target_link_libraries(wyn "-framework Cocoa")
elseif (UNIX)
    target_sources(wyn PRIVATE "src/wyn_xlib.c")
    target_link_libraries(wyn "X11")
endif()

target_include_directories(wyn PUBLIC "include/")

# ================================================================ #

add_executable(example MACOSX_BUNDLE)
target_sources(example PRIVATE "example/main.c")
target_link_libraries(example wyn)

# ================================================================ #

target_compile_options(wyn
    PRIVATE
        "-Weverything"
        "-Wno-padded"
        "-Wno-c++98-compat"
        "-Wno-declaration-after-statement"
        "-Wno-gnu-empty-initializer"
)

# ================================================================ #
