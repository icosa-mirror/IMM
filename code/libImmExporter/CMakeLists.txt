# libImmExporter - CMakeLists.txt for Android NDK build
cmake_minimum_required(VERSION 3.4.1)

set(libImm-dir ${CMAKE_CURRENT_SOURCE_DIR})

add_library(
    libImmExporter
    STATIC

    ${libImm-dir}/src/document/sequence.cpp
    ${libImm-dir}/src/document/layer.cpp
    ${libImm-dir}/src/document/behaviors/keepalive.cpp
    ${libImm-dir}/src/document/layerModel3d.cpp
    ${libImm-dir}/src/document/layerPicture.cpp
    ${libImm-dir}/src/document/layerSound.cpp
    ${libImm-dir}/src/document/layerSpawnArea.cpp
    ${libImm-dir}/src/document/layerPaint.cpp
    ${libImm-dir}/src/document/layerPaint/drawing.cpp
    ${libImm-dir}/src/document/layerPaint/element.cpp
    ${libImm-dir}/src/toImmersive/toImmersive.cpp
    ${libImm-dir}/src/toImmersive/toImmersiveLayer.cpp
    ${libImm-dir}/src/toImmersive/toImmersiveLayerModel.cpp
    ${libImm-dir}/src/toImmersive/toImmersiveLayerPaint.cpp
    ${libImm-dir}/src/toImmersive/toImmersiveLayerPicture.cpp
    ${libImm-dir}/src/toImmersive/toImmersiveLayerSound.cpp
    ${libImm-dir}/src/toImmersive/toImmersiveLayerSpawnArea.cpp
    ${libImm-dir}/src/toImmersive/toImmersiveUtils.cpp
)

# Add libImmCore dependency (built by the libImmCore gradle module first)
set(core-dir ${CMAKE_CURRENT_SOURCE_DIR}/../libImmCore)
set(core-build-dir ${core-dir}/build)
file(MAKE_DIRECTORY ${core-build-dir})

include_directories(
    ${core-dir}/..
)

add_library(libImmCore STATIC IMPORTED)
set_target_properties(libImmCore PROPERTIES IMPORTED_LOCATION
    ${core-build-dir}/libs/${ANDROID_ABI}/liblibImmCore.a)

# Android system libraries
find_library(android-lib android)
find_library(log-lib log)
find_library(gl3-lib GLESv3)
find_library(z-lib z)

target_link_libraries(
    libImmExporter
    libImmCore
    ${android-lib}
    ${gl3-lib}
    ${log-lib}
    ${z-lib}
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/libs/${ANDROID_ABI})
set_target_properties(libImmExporter PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/libs/${ANDROID_ABI})
