# @file wyn_example/CMakeLists.txt

# ================================================================================================================================

set(WYN_EXAMPLE_GL ON CACHE BOOL "Controls whether or not Wyn-Example uses OpenGL.")

# ================================================================================================================================

add_executable(wyn_example MACOSX_BUNDLE)
add_executable(wyn::example ALIAS wyn_example)

# ================================================================================================================================

target_sources(wyn_example PRIVATE
    "src/main.c"  "src/common.c" "src/events.c" "src/update.c"
    "src/utils.h" "src/common.h" "src/events.h" "src/update.h" "src/render.h"
)
target_link_libraries(wyn_example wyn::wyn wyn::wyt wyn::wyg)

if (WYN_EXAMPLE_GL)
    target_sources(wyn_example PRIVATE "src/render_gl.c")
    target_compile_definitions(wyn_example PRIVATE "WYN_EXAMPLE_GL")
endif()

# ================================================================================================================================

target_compile_features(wyn_example PRIVATE c_std_23)
target_compile_features(wyn_example PRIVATE cxx_std_23)

target_compile_options(wyn_example
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
