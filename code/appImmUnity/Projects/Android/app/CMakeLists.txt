# appImmUnity - CMakeLists.txt for Android NDK build (Unity plugin)
cmake_minimum_required(VERSION 3.4.1)

set(appImmUnity-dir ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

add_library(
    ImmUnityPlugin
    SHARED
    ${appImmUnity-dir}/src/main.cpp
)

# Add libImmCore dependency
set(core-dir ${appImmUnity-dir}/../libImmCore)
set(core-build-dir ${core-dir}/build)

include_directories(
    ${core-dir}/..
)

add_library(libImmCore STATIC IMPORTED)
set_target_properties(libImmCore PROPERTIES IMPORTED_LOCATION
    ${core-build-dir}/libs/${ANDROID_ABI}/liblibImmCore.a)

# Add libImmImporter dependency
set(imm-importer-dir ${appImmUnity-dir}/../libImmImporter)
set(imm-importer-build-dir ${imm-importer-dir}/build)

add_library(libImmImporter STATIC IMPORTED)
set_target_properties(libImmImporter PROPERTIES IMPORTED_LOCATION
    ${imm-importer-build-dir}/libs/${ANDROID_ABI}/liblibImmImporter.a)

# Add libImmPlayer dependency
set(imm-player-dir ${appImmUnity-dir}/../libImmPlayer)
set(imm-player-build-dir ${imm-player-dir}/build)

include_directories(
    ${imm-player-dir}/src
)

add_library(libImmPlayer STATIC IMPORTED)
set_target_properties(libImmPlayer PROPERTIES IMPORTED_LOCATION
    ${imm-player-build-dir}/libs/${ANDROID_ABI}/liblibImmPlayer.a)

# OGG/Vorbis from thirdparty
set(ogg-third-party ${appImmUnity-dir}/../thirdparty/ogg)
include_directories(
    ${ogg-third-party}/include
    ${ogg-third-party}/libvorbis-1.3.5/include
)

# libpng
include_directories(
    ${appImmUnity-dir}/../thirdparty/lpng1637
)

add_library(png-lib STATIC IMPORTED)
set_target_properties(png-lib PROPERTIES IMPORTED_LOCATION
    ${appImmUnity-dir}/../thirdparty/lpng1637/projects/androidstudio/pngandroid/build/intermediates/cmake/debug/obj/${ANDROID_ABI}/libpng16d.a)

# libjpeg-turbo
add_library(jpeg-lib STATIC IMPORTED)
set_target_properties(jpeg-lib PROPERTIES IMPORTED_LOCATION
    ${appImmUnity-dir}/../thirdparty/libjpeg-turbo/lib/${ANDROID_ABI}/libturbojpeg.a)

# Vorbis libraries (built from thirdparty/ogg)
add_library(vorbis-lib STATIC IMPORTED)
set_target_properties(vorbis-lib PROPERTIES IMPORTED_LOCATION
    ${core-build-dir}/../ogg-build/libvorbis.a)

add_library(ogg-lib STATIC IMPORTED)
set_target_properties(ogg-lib PROPERTIES IMPORTED_LOCATION
    ${core-build-dir}/../ogg-build/libogg.a)

# Android system libraries
find_library(android-lib android)
find_library(log-lib log)
find_library(egl-lib EGL)
find_library(gl3-lib GLESv3)
find_library(z-lib z)

# Link libraries (order matters for static libraries)
target_link_libraries(
    ImmUnityPlugin
    libImmPlayer
    libImmImporter
    libImmCore
    vorbis-lib
    ogg-lib
    png-lib
    jpeg-lib
    ${android-lib}
    ${log-lib}
    ${egl-lib}
    ${gl3-lib}
    ${z-lib}
)
