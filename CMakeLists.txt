cmake_minimum_required(VERSION 3.16)
project(RaylibLearning C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

set(RAYLIB_DIR ${CMAKE_SOURCE_DIR}/external/RayLib/include)

# --- raylib, built from the vendored source (external/RayLib/include) ---
add_library(raylib STATIC
        ${RAYLIB_DIR}/rcore.c
        ${RAYLIB_DIR}/rshapes.c
        ${RAYLIB_DIR}/rtextures.c
        ${RAYLIB_DIR}/rtext.c
        ${RAYLIB_DIR}/rmodels.c
        ${RAYLIB_DIR}/utils.c
        ${RAYLIB_DIR}/raudio.c
        ${RAYLIB_DIR}/rglfw.c
)

target_include_directories(raylib PUBLIC ${RAYLIB_DIR})
target_include_directories(raylib PRIVATE ${RAYLIB_DIR}/external/glfw/include)
target_compile_definitions(raylib PRIVATE PLATFORM_DESKTOP GRAPHICS_API_OPENGL_33)

if (APPLE)
    enable_language(OBJC)
    set_source_files_properties(${RAYLIB_DIR}/rglfw.c PROPERTIES LANGUAGE OBJC)
    target_link_libraries(raylib PUBLIC
            "-framework Cocoa"
            "-framework IOKit"
            "-framework CoreVideo"
            "-framework OpenGL"
            "-framework CoreAudio"
            "-framework AudioToolbox"
    )
elseif (UNIX)
    target_link_libraries(raylib PUBLIC GL m pthread dl rt X11)
elseif (WIN32)
    target_link_libraries(raylib PUBLIC winmm gdi32 opengl32)
endif ()

# --- game executable ---
add_executable(RaylibLearning
        RaylibLearning/main.cpp
        RaylibLearning/headers/Marble.cpp
        RaylibLearning/headers/NoiseMap.cpp
        RaylibLearning/headers/PhysicalObj.cpp
        RaylibLearning/headers/QuaternionR.cpp
        RaylibLearning/headers/SphereColBox.cpp
        RaylibLearning/headers/Terrain.cpp
        RaylibLearning/headers/Tree.cpp
)

target_link_libraries(RaylibLearning PRIVATE raylib)

# main.cpp loads assets via paths relative to the working directory
# (e.g. "textures/dora2.jpg"), so run the binary from RaylibLearning/.
set_target_properties(RaylibLearning PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/RaylibLearning
)
