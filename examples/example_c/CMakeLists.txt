# @file examples/example_c/CMakeLists.txt

# ================================================================================================================================

add_executable(wyn_example_c WIN32 MACOSX_BUNDLE)
add_executable(wyn::example_c ALIAS wyn_example_c)

# ================================================================================================================================

target_sources(wyn_example_c
    PRIVATE
        "src/main.c"
        "src/events.c"
        "src/app.h"
)
target_link_libraries(wyn_example_c wyn::wyn wyn::wyt)

# ================================================================================================================================

if (c_std_23 IN_LIST CMAKE_C_COMPILE_FEATURES)
    target_compile_features(wyn_example_c PRIVATE c_std_23)
elseif (c_std_17 IN_LIST CMAKE_C_COMPILE_FEATURES)
    target_compile_features(wyn_example_c PRIVATE c_std_17)
else()
    target_compile_features(wyn_example_c PRIVATE c_std_11)
endif()

# ================================================================================================================================

if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(wyn_example_c
        PRIVATE
            "-Weverything"
            "-Wno-c++98-compat"
            "-Wno-c++17-extensions"
            "-Wno-c99-compat"
            "-Wno-pre-c2x-compat"
            "-Wno-fixed-enum-extension"
            "-Wno-microsoft-fixed-enum"
            "-Wno-declaration-after-statement"
            "-Wno-gnu-empty-initializer"
            "-Wno-unused-macros"
            "-Wno-padded"
            "-Wno-unsafe-buffer-usage"
    )
elseif (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(wyn_example_c
        PRIVATE
            "-Wall"
            "-Wextra"
            "-Wpedantic"
    )
elseif (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(wyn_example_c
        PRIVATE
            "/W4"
    )
endif()

# ================================================================================================================================
