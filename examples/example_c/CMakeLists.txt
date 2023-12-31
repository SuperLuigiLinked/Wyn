# @file example_c/CMakeLists.txt

# ================================================================================================================================

add_executable(wyn_example_c WIN32 MACOSX_BUNDLE)
add_executable(wyn::example_c ALIAS wyn_example_c)

# ================================================================================================================================

target_compile_features(wyn_example_c PRIVATE ${WYN_STANDARD_C})
target_compile_options(wyn_example_c PRIVATE ${WYN_WARNINGS_C})

# ================================================================================================================================

target_sources(wyn_example_c
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/events.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/app.h"
)
target_link_libraries(wyn_example_c wyn::wyn wyn::wyt)

# ================================================================================================================================
