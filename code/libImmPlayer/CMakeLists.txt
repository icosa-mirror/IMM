# libImmPlayer - CMakeLists.txt for Android NDK build
cmake_minimum_required(VERSION 3.4.1)

set(libImmPlayer-dir ${CMAKE_CURRENT_SOURCE_DIR})
set(shaderUtil-dir ${CMAKE_CURRENT_SOURCE_DIR})

add_library(
    libImmPlayer
    STATIC

    ${libImmPlayer-dir}/src/player.cpp
    ${libImmPlayer-dir}/src/document.cpp
    ${libImmPlayer-dir}/src/blue_noise.cpp
    ${libImmPlayer-dir}/src/mngrPlayer.cpp
    ${libImmPlayer-dir}/src/layerRenderers/layerRendererPaint/static/layerRendererPaintStatic.cpp
    ${libImmPlayer-dir}/src/layerRenderers/layerRendererPaint/pretessellated/layerRendererPaintPretessellated.cpp
    ${libImmPlayer-dir}/src/layerRenderers/layerRendererPicture/layerRendererPicture.cpp
    ${libImmPlayer-dir}/src/layerRenderers/layerRendererSound/layerRendererSound.cpp
    ${libImmPlayer-dir}/src/layerRenderers/layerRendererModel/layerRendererModel.cpp
)

# Define ANDROID macro for conditional compilation
target_compile_definitions(libImmPlayer PRIVATE ANDROID)

# Shader transformation - convert GLES shaders to C headers
# Model shaders are in gles subdirectory
add_custom_target(
    ShadersModel
    COMMAND ${CMAKE_COMMAND}
    -D INPUT=${libImmPlayer-dir}/src/layerRenderers/layerRendererModel/gles
    -D OUTPUT=${libImmPlayer-dir}/src/layerRenderers/layerRendererModel/tmp/shader_model_GLES_glsl.h
    -D NAMESPACE=INVALID
    -P ${shaderUtil-dir}/create_shader.cmake
)
add_dependencies(libImmPlayer ShadersModel)

# Pretessellated shaders are in main directory (not gles subdirectory)
add_custom_target(
    ShadersPretessellated
    COMMAND ${CMAKE_COMMAND}
    -D INPUT=${libImmPlayer-dir}/src/layerRenderers/layerRendererPaint/pretessellated
    -D OUTPUT=${libImmPlayer-dir}/src/layerRenderers/layerRendererPaint/pretessellated/tmp/shader_pretessellated_brush_GLES_glsl.h
    -D NAMESPACE=INVALID
    -P ${shaderUtil-dir}/create_shader.cmake
)
add_dependencies(libImmPlayer ShadersPretessellated)

# Picture shaders are already C-string wrapped in .es.glsl files (included directly)

# Add libImmCore dependency
set(core-dir ${CMAKE_CURRENT_SOURCE_DIR}/../libImmCore)
set(core-build-dir ${core-dir}/build)
file(MAKE_DIRECTORY ${core-build-dir})

include_directories(
    ${core-dir}/..
)

add_library(libImmCore STATIC IMPORTED)
set_target_properties(libImmCore PROPERTIES IMPORTED_LOCATION
    ${core-build-dir}/libs/${ANDROID_ABI}/liblibImmCore.a)

# Add libImmImporter dependency
set(imm-importer-dir ${CMAKE_CURRENT_SOURCE_DIR}/../libImmImporter)
set(imm-importer-build-dir ${imm-importer-dir}/build)
file(MAKE_DIRECTORY ${imm-importer-build-dir})

add_library(libImmImporter STATIC IMPORTED)
set_target_properties(libImmImporter PROPERTIES IMPORTED_LOCATION
    ${imm-importer-build-dir}/libs/${ANDROID_ABI}/liblibImmImporter.a)

# Android system libraries
find_library(android-lib android)
find_library(log-lib log)
find_library(gl3-lib GLESv3)
find_library(z-lib z)

# Link libraries
target_link_libraries(
    libImmPlayer
    libImmImporter
    libImmCore
    ${android-lib}
    ${gl3-lib}
    ${log-lib}
    ${z-lib}
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/libs/${ANDROID_ABI})
set_target_properties(libImmPlayer PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/libs/${ANDROID_ABI})
