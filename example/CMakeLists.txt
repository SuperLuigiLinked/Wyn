# @file wyn_example/CMakeLists.txt

# ================================================================================================================================

add_executable(wyn_example WIN32 MACOSX_BUNDLE)
add_executable(wyn::example ALIAS wyn_example)

# ================================================================================================================================

target_sources(wyn_example
    PRIVATE
        "src/main.c"
        "src/events.c"
        "src/app.h"
)
target_link_libraries(wyn_example wyn::wyn wyn::wyt)

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
