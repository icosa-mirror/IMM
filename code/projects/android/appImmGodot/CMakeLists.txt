# appImmGodot - Android native Godot plugin/GDExtension build
cmake_minimum_required(VERSION 3.22.1)
project(appImmGodotAndroid)

set(IMM_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)
set(CODE_DIR ${IMM_ROOT}/code)
set(THIRDPARTY_DIR ${IMM_ROOT}/thirdparty)
set(SAMPLE_DIR ${CODE_DIR}/ImmGodotSampleProject)

if (NOT GODOT_CPP_PATH)
    set(GODOT_CPP_PATH ${IMM_ROOT}/thirdparty/godot-cpp)
endif()

if (NOT EXISTS ${GODOT_CPP_PATH})
    message(FATAL_ERROR "godot-cpp checkout not found at ${GODOT_CPP_PATH}. Pass -PimmGodotCppPath=... or bootstrap thirdparty/godot-cpp.")
endif()

if (NOT GODOT_CPP_LIB)
    file(GLOB GODOT_CPP_LIB_CANDIDATES
        ${GODOT_CPP_PATH}/bin/libgodot-cpp.android.template_debug.arm64.a
        ${GODOT_CPP_PATH}/bin/libgodot-cpp.android.template_debug.dev.arm64.a
        ${GODOT_CPP_PATH}/bin/libgodot-cpp.android.debug.arm64.a
        ${GODOT_CPP_PATH}/bin/libgodot-cpp.android.template_release.arm64.a
        ${GODOT_CPP_PATH}/bin/libgodot-cpp.android.template_release.dev.arm64.a
        ${GODOT_CPP_PATH}/bin/libgodot-cpp.android.release.arm64.a
    )
    list(LENGTH GODOT_CPP_LIB_CANDIDATES GODOT_CPP_LIB_CANDIDATE_COUNT)
    if (GODOT_CPP_LIB_CANDIDATE_COUNT GREATER 0)
        list(GET GODOT_CPP_LIB_CANDIDATES 0 GODOT_CPP_LIB)
    endif()
endif()

if (NOT GODOT_CPP_LIB OR NOT EXISTS ${GODOT_CPP_LIB})
    message(FATAL_ERROR "godot-cpp Android library not found. Build godot-cpp for platform=android arch=arm64 or pass -PimmGodotCppLib=...")
endif()

if (NOT IMM_GODOT_VARIANT)
    string(TOLOWER "${CMAKE_BUILD_TYPE}" IMM_GODOT_VARIANT)
endif()
if (NOT IMM_GODOT_VARIANT)
    set(IMM_GODOT_VARIANT debug)
endif()

set(IMM_GODOT_OUTPUT_DIR ${SAMPLE_DIR}/addons/imm_viewer/bin/android/${IMM_GODOT_VARIANT})
file(MAKE_DIRECTORY ${IMM_GODOT_OUTPUT_DIR})

add_library(godot-cpp STATIC IMPORTED)
set_target_properties(godot-cpp PROPERTIES IMPORTED_LOCATION ${GODOT_CPP_LIB})

add_library(libImmCore STATIC IMPORTED)
set_target_properties(libImmCore PROPERTIES IMPORTED_LOCATION
    ${CODE_DIR}/libImmCore/build/libs/${ANDROID_ABI}/liblibImmCore.a)

add_library(libImmImporter STATIC IMPORTED)
set_target_properties(libImmImporter PROPERTIES IMPORTED_LOCATION
    ${CODE_DIR}/libImmImporter/build/libs/${ANDROID_ABI}/liblibImmImporter.a)

add_library(libImmPlayer STATIC IMPORTED)
set_target_properties(libImmPlayer PROPERTIES IMPORTED_LOCATION
    ${CODE_DIR}/libImmPlayer/build/libs/${ANDROID_ABI}/liblibImmPlayer.a)

add_library(png-lib STATIC IMPORTED)
set_target_properties(png-lib PROPERTIES IMPORTED_LOCATION
    ${CODE_DIR}/libImmCore/build/libs/${ANDROID_ABI}/libpng16d.a)

add_library(vorbis-lib STATIC IMPORTED)
set_target_properties(vorbis-lib PROPERTIES IMPORTED_LOCATION
    ${CODE_DIR}/libImmCore/build/libs/${ANDROID_ABI}/libvorbis.a)

add_library(ogg-lib STATIC IMPORTED)
set_target_properties(ogg-lib PROPERTIES IMPORTED_LOCATION
    ${CODE_DIR}/libImmCore/build/libs/${ANDROID_ABI}/libogg.a)

add_library(jpeg-lib STATIC IMPORTED)
set_target_properties(jpeg-lib PROPERTIES IMPORTED_LOCATION
    ${THIRDPARTY_DIR}/libjpeg-turbo/lib/${ANDROID_ABI}/libturbojpeg.a)

find_library(android-lib android)
find_library(log-lib log)
find_library(egl-lib EGL)
find_library(gl3-lib GLESv3)
find_library(opensles-lib OpenSLES)
find_library(mediandk-lib mediandk)
find_library(z-lib z)
find_library(dl-lib dl)

add_library(ImmGodotPlugin SHARED
    ${CODE_DIR}/appImmGodot/src/main.cpp
    ${CODE_DIR}/appImmShared/src/imm_engine_bridge.cpp
)
target_include_directories(ImmGodotPlugin PRIVATE
    ${IMM_ROOT}
    ${CODE_DIR}
    ${CODE_DIR}/libImmPlayer/src
    ${THIRDPARTY_DIR}/ogg/include
    ${THIRDPARTY_DIR}/ogg/libvorbis-1.3.5/include
    ${THIRDPARTY_DIR}/lpng1637
    ${THIRDPARTY_DIR}/libjpeg-turbo/include/android
)
target_compile_definitions(ImmGodotPlugin PRIVATE ANDROID ANDROID_ENABLED)
target_link_libraries(ImmGodotPlugin
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
    ${opensles-lib}
    ${mediandk-lib}
    ${z-lib}
    ${dl-lib}
)
set_target_properties(ImmGodotPlugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${IMM_GODOT_OUTPUT_DIR}
)

add_library(imm_godot_extension SHARED
    ${CODE_DIR}/appImmGodotGDExtension/src/imm_viewer_compositor_effect.cpp
    ${CODE_DIR}/appImmGodotGDExtension/src/imm_viewer_vulkan_frame.cpp
    ${CODE_DIR}/appImmGodotGDExtension/src/imm_viewer_node.cpp
    ${CODE_DIR}/appImmGodotGDExtension/src/register_types.cpp
)
target_include_directories(imm_godot_extension PRIVATE
    ${IMM_ROOT}
    ${CODE_DIR}
    ${GODOT_CPP_PATH}/include
    ${GODOT_CPP_PATH}/gen/include
    ${GODOT_CPP_PATH}/gdextension
)
target_compile_definitions(imm_godot_extension PRIVATE ANDROID ANDROID_ENABLED TYPED_METHOD_BIND)
target_link_libraries(imm_godot_extension
    godot-cpp
    ImmGodotPlugin
    ${android-lib}
    ${log-lib}
    ${dl-lib}
)
set_target_properties(imm_godot_extension PROPERTIES
    OUTPUT_NAME "imm_godot_extension.arm64"
    LIBRARY_OUTPUT_DIRECTORY ${IMM_GODOT_OUTPUT_DIR}
)
