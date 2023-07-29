add_executable(example MACOSX_BUNDLE)
add_executable(wyn::example ALIAS example)

target_sources(example PRIVATE "main.c")
target_link_libraries(example wyn::wyn wyn::wyt)

target_compile_features(example PRIVATE c_std_23)

target_compile_options(example
    PRIVATE
        "-Weverything"
        "-Wno-c++98-compat"
        "-Wno-pre-c2x-compat"
        "-Wno-declaration-after-statement"
        "-Wno-gnu-empty-initializer"
)