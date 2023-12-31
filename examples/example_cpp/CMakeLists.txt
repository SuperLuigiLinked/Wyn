# @file example_cpp/CMakeLists.txt

# ================================================================================================================================

add_executable(wyn_example_cpp WIN32 MACOSX_BUNDLE)
add_executable(wyn::example_cpp ALIAS wyn_example_cpp)

# ================================================================================================================================

target_compile_features(wyn_example_cpp PRIVATE ${WYN_STANDARD_CPP})
target_compile_options(wyn_example_cpp PRIVATE ${WYN_WARNINGS_CPP})

# ================================================================================================================================

target_sources(wyn_example_cpp
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/events.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/app.hpp"
)
target_link_libraries(wyn_example_cpp wyn::wyn wyn::wyt)

# ================================================================================================================================
