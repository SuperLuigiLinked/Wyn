# @file wyt/CMakeLists.txt

# ================================================================================================================================

set(WYT_USE_WIN32    OFF CACHE BOOL "Win32 backend toggle.")
set(WYT_USE_PTHREADS OFF CACHE BOOL "Pthreads backend toggle.")

if (NOT (WYT_USE_WIN32 OR WYT_USE_PTHREADS))
    if (WIN32)
        set(WYT_USE_WIN32 ON)
    else()
        set(WYT_USE_PTHREADS ON)
    endif()
endif()

# ================================================================================================================================

add_library(wyt)
add_library(wyn::wyt ALIAS wyt)

# ================================================================================================================================

target_include_directories(wyt PUBLIC "include/")
target_sources(wyt PUBLIC "include/wyt.h")

if (WYT_USE_WIN32)
    target_sources(wyt PRIVATE "src/wyt_win32.c")
    target_link_libraries(wyt PRIVATE "kernel32")
    target_compile_definitions(wyt PUBLIC "WYT_WIN32")
elseif (WYT_USE_PTHREADS)
    target_sources(wyt PRIVATE "src/wyt_pthreads.c")
    target_link_libraries(wyt "pthread")
    target_compile_definitions(wyt PUBLIC "WYT_PTHREADS")
else()
    message(FATAL_ERROR "No Wyt backend selected!")
endif()

# ================================================================================================================================

if (c_std_23 IN_LIST CMAKE_C_COMPILE_FEATURES)
    target_compile_features(wyt PRIVATE c_std_23)
elseif (c_std_17 IN_LIST CMAKE_C_COMPILE_FEATURES)
    target_compile_features(wyt PRIVATE c_std_17)
else()
    target_compile_features(wyt PRIVATE c_std_11)
endif()

# ================================================================================================================================

if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(wyt
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
    target_compile_options(wyt
        PRIVATE
            "-Wall"
            "-Wextra"
            "-Wpedantic"
    )
elseif (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(wyt
        PRIVATE
            "/W4"
    )
endif()

# ================================================================================================================================
