# @file wyn/CMakeLists.txt

# ================================================================================================================================

if (WYN_FEATURE_STUBS AND NOT WIN32)
    add_library(wyn OBJECT)
else()
    add_library(wyn)
endif()
add_library(wyn::wyn ALIAS wyn)

# ================================================================================================================================

target_compile_features(wyn PRIVATE ${WYN_STANDARD_C})
target_compile_options(wyn PRIVATE ${WYN_WARNINGS_C})

# ================================================================================================================================

target_include_directories(wyn PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/")
target_sources(wyn PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/wyn.h" "${CMAKE_CURRENT_SOURCE_DIR}/include/wyc.h")

if (WYN_FEATURE_STUBS)
    target_sources(wyn PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/wyn_stubs.c")
    target_compile_definitions(wyn PUBLIC "WYN_STUBS")
endif()

if (WYN_BACKEND_WIN32)
    target_sources(wyn PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/wyn_win32.c")
    target_link_libraries(wyn PRIVATE "kernel32" "user32")
    target_compile_definitions(wyn PUBLIC "WYN_WIN32")
elseif (WYN_BACKEND_COCOA)
    target_sources(wyn PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/wyn_cocoa.m")
    target_link_libraries(wyn PRIVATE "-framework Cocoa")
    target_compile_definitions(wyn PUBLIC "WYN_COCOA")
elseif (WYN_BACKEND_XLIB)
    target_sources(wyn PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/wyn_xlib.c")
    target_link_libraries(wyn PRIVATE "X11" "Xrandr")
    target_compile_definitions(wyn PUBLIC "WYN_XLIB")
else()
    message(FATAL_ERROR "No Wyn backend selected!")
endif()

# ================================================================================================================================
