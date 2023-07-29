add_library(wyt)
add_library(wyn::wyt ALIAS wyt)

target_include_directories(wyt PUBLIC "include/")
target_sources(wyt PUBLIC "include/wyt.h")

if (WIN32)
    target_sources(wyt PRIVATE "src/wyt_win32.c")
elseif (UNIX)
    target_sources(wyt PRIVATE "src/wyt_pthreads.c")
    target_link_libraries(wyt "pthread")
endif()

target_compile_features(wyt PRIVATE c_std_23)

target_compile_options(wyt
    PRIVATE
        "-Weverything"
        # "-Wno-padded"
        # "-Wno-c++98-compat"
        # "-Wno-c++17-extensions"
        # "-Wno-declaration-after-statement"
        # "-Wno-gnu-empty-initializer"
)
