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
    target_link_libraries(wyt PRIVATE "kernel32" "winmm" "synchronization")
    target_compile_definitions(wyt PUBLIC "WYT_WIN32")
elseif (WYT_USE_PTHREADS)
    target_sources(wyt PRIVATE "src/wyt_pthreads.c")
    target_link_libraries(wyt "pthread")
    target_compile_definitions(wyt PUBLIC "WYT_PTHREADS")
else()
    message(FATAL_ERROR "No Wyt backend selected!")
endif()

# ================================================================================================================================

target_compile_features(wyt PRIVATE c_std_23)

target_compile_options(wyt
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
