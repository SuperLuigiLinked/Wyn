cmake_minimum_required(VERSION 3.23)
project(wyn)

set(CMAKE_C_STANDARD 23)

# ================================================================ #

add_library(wyn)
target_sources(wyn INTERFACE "src/wyn.h")

if (WIN32)
    target_sources(wyn PRIVATE "src/wyn_win32.c")
elseif (APPLE)
    target_sources(wyn PRIVATE "src/wyn_cocoa.c")
elseif (UNIX)
    target_sources(wyn PRIVATE "src/wyn_xlib.c")
endif()

target_include_directories(wyn INTERFACE "src/")

# ================================================================ #

add_executable(example MACOSX_BUNDLE)
target_sources(example PRIVATE "example/main.c")
target_link_libraries(example wyn)

# ================================================================ #
