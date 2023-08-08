add_library(wyn)
add_library(wyn::wyn ALIAS wyn)

target_include_directories(wyn PUBLIC "include/")
target_sources(wyn PUBLIC "include/wyn.h")

if (WIN32)
    target_sources(wyn PRIVATE "src/wyn_win32.c")
elseif (APPLE)
    target_sources(wyn PRIVATE "src/wyn_cocoa.m")
    target_link_libraries(wyn "-framework Cocoa")
elseif (UNIX)
    target_sources(wyn PRIVATE "src/wyn_xlib.c")
    target_link_libraries(wyn "X11")
endif()

target_compile_features(wyn PRIVATE c_std_23)

target_compile_options(wyn
    PRIVATE
        "-Weverything"
        "-Wno-c++98-compat"
        # "-Wno-c++17-extensions"
        "-Wno-declaration-after-statement"
        "-Wno-gnu-empty-initializer"
        "-Wno-unsafe-buffer-usage"
        "-Wno-padded"
)
