cmake_minimum_required(VERSION 3.25)
project(wyn_c)

# ================================================================ #

set(WYN_USE_STUBS ON CACHE BOOL "Controls whether or not Default Stubs for Wyn Callbacks are defined.")

# ================================================================ #

add_subdirectory(wyn)
add_subdirectory(wyt)
add_subdirectory(example)

# ================================================================ #
