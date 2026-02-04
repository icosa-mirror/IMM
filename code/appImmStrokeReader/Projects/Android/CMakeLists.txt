cmake_minimum_required(VERSION 3.4.1)

set(appImmStrokeReader-dir ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(CODE_DIR ${appImmStrokeReader-dir}/..)

add_library(
    ImmStrokeReader
    SHARED
    ${appImmStrokeReader-dir}/src/main.cpp
    ${appImmStrokeReader-dir}/src/strokeStore.cpp
)

include_directories(
    ${CODE_DIR}
    ${CODE_DIR}/libImmCore/src/libBasics
)

set(core-dir ${CODE_DIR}/libImmCore)
set(core-build-dir ${core-dir}/build)

add_library(libImmCore STATIC IMPORTED)
set_target_properties(libImmCore PROPERTIES IMPORTED_LOCATION
    ${core-build-dir}/libs/${ANDROID_ABI}/liblibImmCore.a)

set(imm-importer-dir ${CODE_DIR}/libImmImporter)
set(imm-importer-build-dir ${imm-importer-dir}/build)

add_library(libImmImporter STATIC IMPORTED)
set_target_properties(libImmImporter PROPERTIES IMPORTED_LOCATION
    ${imm-importer-build-dir}/libs/${ANDROID_ABI}/liblibImmImporter.a)


find_library(android-lib android)
find_library(log-lib log)
find_library(gl3-lib GLESv3)
find_library(z-lib z)

set(THIRDPARTY_DIR ${CODE_DIR}/../thirdparty)
add_library(png-lib STATIC IMPORTED)
set_target_properties(png-lib PROPERTIES IMPORTED_LOCATION
    ${core-build-dir}/libs/${ANDROID_ABI}/libpng16d.a)

add_library(jpeg-lib STATIC IMPORTED)
set_target_properties(jpeg-lib PROPERTIES IMPORTED_LOCATION
    ${THIRDPARTY_DIR}/libjpeg-turbo/lib/${ANDROID_ABI}/libturbojpeg.a)

add_library(vorbis-lib STATIC IMPORTED)
set_target_properties(vorbis-lib PROPERTIES IMPORTED_LOCATION
    ${core-build-dir}/libs/${ANDROID_ABI}/libvorbis.a)

add_library(ogg-lib STATIC IMPORTED)
set_target_properties(ogg-lib PROPERTIES IMPORTED_LOCATION
    ${core-build-dir}/libs/${ANDROID_ABI}/libogg.a)

target_link_libraries(
    ImmStrokeReader
    libImmImporter
    libImmCore
    vorbis-lib
    ogg-lib
    png-lib
    jpeg-lib
    ${android-lib}
    ${log-lib}
    ${gl3-lib}
    ${z-lib}
)
