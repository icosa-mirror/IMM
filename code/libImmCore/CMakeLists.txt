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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libSound/android/piSoundEngineAndroid.cpp
    # piVR.cpp excluded - uses Oculus PC SDK, not available on Android (Quest uses OpenXR)
)

# Thirdparty root (relative to repo root, not code/)
set(THIRDPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty)

# OGG/Vorbis from thirdparty
set(ogg-third-party ${THIRDPARTY_DIR}/ogg)
add_subdirectory(${ogg-third-party} ${CMAKE_BINARY_DIR}/ogg-build)

include_directories(
    ${ogg-third-party}/include
    ${ogg-third-party}/libvorbis-1.3.5/include
    ${ogg-third-party}/libvorbis-1.3.5/include/vorbis
    ${ogg-third-party}/libvorbis-1.3.5/lib
)

# libpng from thirdparty (build from source)
# Set ZLIB for Android NDK (zlib is included in the sysroot)
set(ZLIB_LIBRARY z)
set(ZLIB_INCLUDE_DIR "${CMAKE_SYSROOT}/usr/include")
set(ZLIB_FOUND TRUE)
set(PNG_BUILD_ZLIB OFF CACHE BOOL "Use system zlib" FORCE)
set(PNG_SHARED OFF CACHE BOOL "Build shared lib" FORCE)
set(PNG_STATIC ON CACHE BOOL "Build static lib" FORCE)
set(PNG_TESTS OFF CACHE BOOL "Build libpng tests" FORCE)
# Skip install targets to avoid POST_BUILD copy conflicts with custom output directory
set(SKIP_INSTALL_LIBRARIES ON CACHE BOOL "Skip libpng install" FORCE)
set(SKIP_INSTALL_ALL ON CACHE BOOL "Skip all libpng install" FORCE)
add_subdirectory(${THIRDPARTY_DIR}/lpng1637 ${CMAKE_BINARY_DIR}/libpng-build)

include_directories(
    ${THIRDPARTY_DIR}/lpng1637
    ${CMAKE_BINARY_DIR}/libpng-build  # For pnglibconf.h
)

# libjpeg-turbo from thirdparty
include_directories(
    ${THIRDPARTY_DIR}/libjpeg-turbo/libjpeg-turbo/
    ${THIRDPARTY_DIR}/libjpeg-turbo/include/android
)

add_library(jpeg-lib STATIC IMPORTED)
set_target_properties(
    jpeg-lib PROPERTIES IMPORTED_LOCATION
    ${THIRDPARTY_DIR}/libjpeg-turbo/lib/${ANDROID_ABI}/libturbojpeg.a
)

# Android system libraries
find_library(android-lib android)
find_library(log-lib log)
find_library(egl-lib EGL)
find_library(gl3-lib GLESv3)
find_library(opensles-lib OpenSLES)
find_library(mediandk-lib mediandk)

# Link libraries
target_link_libraries(
    libImmCore
    vorbis
    png_static
    jpeg-lib
    ${log-lib}
    ${egl-lib}
    ${gl3-lib}
    ${opensles-lib}
    ${mediandk-lib}
    ${android-lib}
)

# Output directories for all libraries
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/libs/${ANDROID_ABI})
set_target_properties(libImmCore PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/libs/${ANDROID_ABI})

# Also copy ogg/vorbis/png libs to predictable output location
set_target_properties(ogg PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/libs/${ANDROID_ABI})
set_target_properties(vorbis PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/libs/${ANDROID_ABI})
set_target_properties(png_static PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/libs/${ANDROID_ABI})
