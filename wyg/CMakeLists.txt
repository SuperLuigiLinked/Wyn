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

message(STATUS "GL: OPENGL_FOUND            = ${OPENGL_FOUND}")
message(STATUS "GL: OPENGL_XMESA_FOUND      = ${OPENGL_XMESA_FOUND}")
message(STATUS "GL: OPENGL_GLU_FOUND        = ${OPENGL_GLU_FOUND}")
message(STATUS "GL: OpenGL_OpenGL_FOUND     = ${OpenGL_OpenGL_FOUND}")
message(STATUS "GL: OpenGL_GLX_FOUND        = ${OpenGL_GLX_FOUND}")
message(STATUS "GL: OpenGL_EGL_FOUND        = ${OpenGL_EGL_FOUND}")
message(STATUS "GL: OPENGL_INCLUDE_DIR      = ${OPENGL_INCLUDE_DIR}")
message(STATUS "GL: OPENGL_EGL_INCLUDE_DIRS = ${OPENGL_EGL_INCLUDE_DIRS}")
message(STATUS "GL: OPENGL_LIBRARIES        = ${OPENGL_LIBRARIES}")
message(STATUS "GL: OPENGL_egl_LIBRARY      = ${OPENGL_egl_LIBRARY}")
message(STATUS "GL: OPENGL_glu_LIBRARY      = ${OPENGL_glu_LIBRARY}")
message(STATUS "GL: OPENGL_glx_LIBRARY      = ${OPENGL_glx_LIBRARY}")
message(STATUS "GL: OPENGL_opengl_LIBRARY   = ${OPENGL_opengl_LIBRARY}")
message(STATUS "GL: OPENGL_gl_LIBRARY       = ${OPENGL_gl_LIBRARY}")

# ================================================================================================================================

add_library(wyg)
add_library(wyn::wyg ALIAS wyg)

# ================================================================================================================================

target_include_directories(wyg PUBLIC "include/")
target_include_directories(wyg PUBLIC "${KhronosGL_SOURCE_DIR}/api/")
target_include_directories(wyg PUBLIC "${KhronosEGL_SOURCE_DIR}/api/")

if (WYG_USE_GL)
    target_sources(wyg PUBLIC "include/wyg_gl.h")

    if (WIN32)
        target_sources(wyg PRIVATE "src/gl/wyg_wgl.c")
    elseif (APPLE)
        target_sources(wyg PRIVATE "src/gl/wyg_nsgl.m")
    # elseif (UNIX)
    #     target_sources(wyg PRIVATE "src/gl/wyg_glx.c")
    else()
        target_sources(wyg PRIVATE "src/gl/wyg_egl.c")
        target_link_Libraries(wyg PUBLIC OpenGL::EGL)
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
