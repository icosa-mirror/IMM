# libImmCore - CMakeLists.txt for Android NDK build
cmake_minimum_required(VERSION 3.4.1)

add_library(
    libImmCore
    STATIC

    ${CMAKE_CURRENT_SOURCE_DIR}/src/libBasics/android/piLog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libBasics/android/piFileOS.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libBasics/android/piTimer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libBasics/android/piMutex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libBasics/android/piCStr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libBasics/piArray.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libBasics/piPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libBasics/piString.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libBasics/piTick.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libBasics/piImage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libBasics/piStr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libBasics/piFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libBasics/piFileName.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libBasics/piTypes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libCompression/basic/piBitInterlacer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libCompression/basic/piPredictors.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libCompression/basic/piQuantize.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libCompression/basic/piTransforms.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libMesh/piMesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libMesh/piRenderMesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libRender/piRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libRender/opengles/piGLES_Renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libWave/piWave.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libWave/formats/piWaveWAV.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libWave/formats/piWaveOGG.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libSound/piSound.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libSound/piSoundEngineNULL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libVR/piVR.cpp
)

# OGG/Vorbis from thirdparty
set(ogg-third-party ${CMAKE_SOURCE_DIR}/../thirdparty/ogg)
add_subdirectory(${ogg-third-party} ${CMAKE_BINARY_DIR}/ogg-build)

include_directories(
    ${ogg-third-party}/include
    ${ogg-third-party}/libvorbis-1.3.5/include
    ${ogg-third-party}/libvorbis-1.3.5/lib
)

# libpng from thirdparty
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/lpng1637
)

add_library(png-lib STATIC IMPORTED)
set_target_properties(
    png-lib PROPERTIES IMPORTED_LOCATION
    ${CMAKE_SOURCE_DIR}/../thirdparty/lpng1637/projects/androidstudio/pngandroid/build/intermediates/cmake/debug/obj/${ANDROID_ABI}/libpng16d.a
)

# libjpeg-turbo from thirdparty
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/libjpeg-turbo/libjpeg-turbo/
    ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/libjpeg-turbo/include/android
)

add_library(jpeg-lib STATIC IMPORTED)
set_target_properties(
    jpeg-lib PROPERTIES IMPORTED_LOCATION
    ${CMAKE_SOURCE_DIR}/../thirdparty/libjpeg-turbo/lib/${ANDROID_ABI}/libturbojpeg.a
)

# Android system libraries
find_library(android-lib android)
find_library(log-lib log)
find_library(egl-lib EGL)
find_library(gl3-lib GLESv3)

# Link libraries
target_link_libraries(
    libImmCore
    vorbis
    png-lib
    jpeg-lib
    ${log-lib}
    ${egl-lib}
    ${gl3-lib}
    ${android-lib}
)

# Output directory
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/libs/${ANDROID_ABI})
set_target_properties(libImmCore PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/libs/${ANDROID_ABI})
