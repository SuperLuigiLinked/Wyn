add_executable(wyn_example MACOSX_BUNDLE)
add_executable(wyn::example ALIAS wyn_example)

target_sources(wyn_example PRIVATE "main.c" "test.cpp")
target_link_libraries(wyn_example wyn::wyn wyn::wyt)

target_compile_features(wyn_example PRIVATE c_std_23)
target_compile_features(wyn_example PRIVATE cxx_std_23)

target_compile_options(wyn_example
    PRIVATE
        "-Weverything"
        "-Wno-c++98-compat"
        "-Wno-c++17-attribute-extensions"
        "-Wno-pre-c2x-compat"
        "-Wno-declaration-after-statement"
        "-Wno-gnu-empty-initializer"
)