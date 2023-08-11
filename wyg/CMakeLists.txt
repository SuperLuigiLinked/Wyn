# @file wyg/CMakeLists.txt

# ================================================================================================================================

set(WYG_USE_GL ON CACHE BOOL "Controls whether or not OpenGL is available.")

# ================================================================================================================================

add_library(wyg)
add_library(wyn::wyg ALIAS wyg)

# ================================================================================================================================

target_include_directories(wyg PUBLIC "include/")

if (WYG_USE_GL)
    target_sources(wyg PUBLIC "include/wyg_gl.h")

    if (WIN32)
        target_sources(wyg PRIVATE "src/gl/wyg_wgl.c")
    elseif (APPLE)
        target_sources(wyg PRIVATE "src/gl/wyg_nsgl.m")
    elseif (UNIX)
        target_sources(wyg PRIVATE "src/gl/wyg_glx.c")
        # target_sources(wyg PRIVATE "src/gl/wyg_egl.c")
    endif()
endif()

# ================================================================================================================================

target_compile_features(wyg PRIVATE c_std_23)

target_compile_options(wyg
    PRIVATE
        "-Weverything"
        "-Wno-c++98-compat"
        "-Wno-c++17-extensions"
        "-Wno-pre-c2x-compat"
        "-Wno-fixed-enum-extension"
        "-Wno-declaration-after-statement"
        "-Wno-gnu-empty-initializer"
        "-Wno-unused-macros"
        "-Wno-padded"
)

if (NOT WIN32)
    target_compile_options(wyg PRIVATE "-Wno-unsafe-buffer-usage")
endif()

# ================================================================================================================================
