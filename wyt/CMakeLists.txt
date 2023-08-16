# @file wyt/CMakeLists.txt

# ================================================================================================================================

add_library(wyt)
add_library(wyn::wyt ALIAS wyt)

# ================================================================================================================================

target_include_directories(wyt PUBLIC "include/")
target_sources(wyt PUBLIC "include/wyt.h")

if (WIN32)
    target_sources(wyt PRIVATE "src/wyt_win32.c")
    target_link_libraries(wyt PRIVATE "kernel32" "winmm" "synchronization")
    target_compile_definitions(wyt PUBLIC "WYT_WIN32")
elseif (UNIX)
    target_sources(wyt PRIVATE "src/wyt_pthreads.c")
    target_link_libraries(wyt "pthread")
    target_compile_definitions(wyt PUBLIC "WYT_PTHREADS")
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
