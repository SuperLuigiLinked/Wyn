# @file examples/example_cpp/CMakeLists.txt

# ================================================================================================================================

add_executable(wyn_example_cpp WIN32 MACOSX_BUNDLE)
add_executable(wyn::example_cpp ALIAS wyn_example_cpp)

# ================================================================================================================================

target_sources(wyn_example_cpp
    PRIVATE
        "src/main.cpp"
        "src/events.cpp"
        "src/app.hpp"
)
target_link_libraries(wyn_example_cpp wyn::wyn wyn::wyt)

# ================================================================================================================================

target_compile_features(wyn_example_cpp PRIVATE cxx_std_20)

# ================================================================================================================================

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(wyn_example_cpp
        PRIVATE
            "-Weverything"
            "-Wno-c++98-compat"
            "-Wno-c++98-compat-pedantic"
            "-Wno-zero-as-null-pointer-constant"
            "-Wno-old-style-cast"
            "-Wno-unsafe-buffer-usage"
    )
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(wyn_example_cpp
        PRIVATE
            "-Wall"
            "-Wextra"
            "-Wpedantic"
    )
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(wyn_example_cpp
        PRIVATE
            "/W4"
    )
endif()

# ================================================================================================================================
