# @file wyt/CMakeLists.txt

# ================================================================================================================================

add_library(wyt)
add_library(wyn::wyt ALIAS wyt)

# ================================================================================================================================

target_compile_features(wyt PRIVATE ${WYN_STANDARD_C})
target_compile_options(wyt PRIVATE ${WYN_WARNINGS_C})

# ================================================================================================================================

target_include_directories(wyt PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/")
target_sources(wyt PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/wyt.h")

if (WYT_BACKEND_WIN32)
    target_sources(wyt PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/wyt_win32.c")
    target_link_libraries(wyt PRIVATE "kernel32")
    target_compile_definitions(wyt PUBLIC "WYT_WIN32")
elseif (WYT_BACKEND_PTHREADS)
    target_sources(wyt PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/wyt_pthreads.c")
    target_link_libraries(wyt "pthread")
    target_compile_definitions(wyt PUBLIC "WYT_PTHREADS")
else()
    message(FATAL_ERROR "No Wyt backend selected!")
endif()

# ================================================================================================================================
