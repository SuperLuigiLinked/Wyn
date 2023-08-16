# @file wyg/CMakeLists.txt

# ================================================================================================================================

set(WYG_USE_GL ON CACHE BOOL "Controls whether or not OpenGL is available.")

# ================================================================================================================================

include(FetchContent)

FetchContent_Declare(KhronosGL
    GIT_REPOSITORY "https://github.com/KhronosGroup/OpenGL-Registry"
    GIT_TAG "main"
)
FetchContent_Declare(KhronosEGL
    GIT_REPOSITORY "https://github.com/KhronosGroup/EGL-Registry"
    GIT_TAG "main"
)
FetchContent_MakeAvailable(KhronosGL KhronosEGL)

FetchContent_GetProperties(KhronosGL SOURCE_DIR KhronosGL_SOURCE_DIR)
FetchContent_GetProperties(KhronosEGL SOURCE_DIR KhronosEGL_SOURCE_DIR)

find_package(OpenGL OPTIONAL_COMPONENTS EGL GLX OpenGL)

# ================================================================================================================================

add_library(wyg)
add_library(wyn::wyg ALIAS wyg)

# ================================================================================================================================

target_include_directories(wyg PUBLIC "include/")
target_include_directories(wyg PUBLIC "${KhronosGL_SOURCE_DIR}/api/")
target_include_directories(wyg PUBLIC "${KhronosEGL_SOURCE_DIR}/api/")

if (WYG_USE_GL)
    target_sources(wyg PUBLIC "include/wyg_gl.h")
    if (TARGET OpenGL::OpenGL)
        target_link_libraries(wyg PUBLIC OpenGL::OpenGL)
    else()
        target_link_libraries(wyg PUBLIC OpenGL::GL)
    endif()

    if (WIN32)
        target_sources(wyg PRIVATE "src/gl/wyg_wgl.c")
    elseif (APPLE)
        target_sources(wyg PRIVATE "src/gl/wyg_nsgl.m")
    elseif (UNIX)
        if (TARGET OpenGL::EGL)
            target_sources(wyg PRIVATE "src/gl/wyg_egl.c")
            target_link_libraries(wyg PUBLIC OpenGL::EGL)
        elseif (TARGET OpenGL::GLX)
            target_sources(wyg PRIVATE "src/gl/wyg_glx.c")
            target_link_libraries(wyg PUBLIC OpenGL::GLX)
        endif()
    endif()
endif()

# ================================================================================================================================

target_compile_features(wyg PRIVATE c_std_23)

target_compile_options(wyg
    PRIVATE
        "-Weverything"
        "-Wno-c++98-compat"
        "-Wno-c++17-extensions"
        "-Wno-pre-c2x-compat"
        "-Wno-fixed-enum-extension"
        "-Wno-microsoft-fixed-enum"
        "-Wno-declaration-after-statement"
        "-Wno-gnu-empty-initializer"
        "-Wno-unused-macros"
        "-Wno-padded"
)

# ================================================================================================================================
