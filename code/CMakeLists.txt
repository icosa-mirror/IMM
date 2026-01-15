cmake_minimum_required(VERSION 3.18)
project(IMM VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Build options
option(IMM_BUILD_UNITY_PLUGIN "Build the Unity plugin" ON)
option(IMM_BUILD_VIEWER "Build the standalone viewer (Windows only)" OFF)
option(IMM_USE_NULL_AUDIO "Use NULL audio backend (no spatial audio)" OFF)

# Platform detection
if(ANDROID)
    set(IMM_PLATFORM "ANDROID")
    set(IMM_USE_NULL_AUDIO ON CACHE BOOL "Force NULL audio on Android" FORCE)
    add_compile_definitions(ANDROID)
elseif(WIN32)
    set(IMM_PLATFORM "WINDOWS")
    add_compile_definitions(WINDOWS WIN32 NOMINMAX STRICT _CRT_SECURE_NO_WARNINGS UNICODE _UNICODE)
elseif(APPLE)
    set(IMM_PLATFORM "MACOS")
    add_compile_definitions(MACOS)
else()
    set(IMM_PLATFORM "LINUX")
    add_compile_definitions(LINUX)
endif()

message(STATUS "Building for platform: ${IMM_PLATFORM}")
message(STATUS "NULL audio backend: ${IMM_USE_NULL_AUDIO}")

# Thirdparty dependencies
set(THIRDPARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty")

# Find or build thirdparty libraries
if(ANDROID)
    # For Android, we build thirdparty from source using FetchContent/ExternalProject
    include(FetchContent)
    include(ExternalProject)

    # zlib - usually available in NDK, but we can build it
    find_package(ZLIB QUIET)
    if(NOT ZLIB_FOUND)
        FetchContent_Declare(
            zlib
            GIT_REPOSITORY https://github.com/madler/zlib.git
            GIT_TAG v1.3.1
        )
        FetchContent_MakeAvailable(zlib)
        set(ZLIB_INCLUDE_DIRS ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
        set(ZLIB_LIBRARIES zlibstatic)
    endif()

    # libpng
    set(PNG_BUILD_ZLIB ON CACHE BOOL "" FORCE)
    set(PNG_SHARED OFF CACHE BOOL "" FORCE)
    set(PNG_STATIC ON CACHE BOOL "" FORCE)
    set(PNG_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        libpng
        GIT_REPOSITORY https://github.com/glennrp/libpng.git
        GIT_TAG v1.6.43
    )
    FetchContent_MakeAvailable(libpng)
    set(PNG_INCLUDE_DIRS ${libpng_SOURCE_DIR} ${libpng_BINARY_DIR})
    set(PNG_LIBRARIES png_static)

    # libjpeg-turbo - requires ExternalProject due to their CMake restrictions
    set(JPEG_INSTALL_DIR ${CMAKE_BINARY_DIR}/libjpeg-turbo-install)
    ExternalProject_Add(
        libjpeg-turbo-external
        GIT_REPOSITORY https://github.com/libjpeg-turbo/libjpeg-turbo.git
        GIT_TAG 3.0.3
        CMAKE_ARGS
            -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
            -DANDROID_ABI=${ANDROID_ABI}
            -DANDROID_PLATFORM=${ANDROID_PLATFORM}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_INSTALL_PREFIX=${JPEG_INSTALL_DIR}
            -DENABLE_SHARED=OFF
            -DENABLE_STATIC=ON
            -DWITH_TURBOJPEG=OFF
        BUILD_BYPRODUCTS ${JPEG_INSTALL_DIR}/lib/libjpeg.a
    )
    # Create imported target for libjpeg
    add_library(jpeg-imported STATIC IMPORTED GLOBAL)
    set_target_properties(jpeg-imported PROPERTIES
        IMPORTED_LOCATION ${JPEG_INSTALL_DIR}/lib/libjpeg.a
    )
    add_dependencies(jpeg-imported libjpeg-turbo-external)
    set(JPEG_INCLUDE_DIRS ${JPEG_INSTALL_DIR}/include)
    set(JPEG_LIBRARIES jpeg-imported)

    # ogg
    FetchContent_Declare(
        ogg
        GIT_REPOSITORY https://github.com/xiph/ogg.git
        GIT_TAG v1.3.5
    )
    FetchContent_MakeAvailable(ogg)
    set(OGG_INCLUDE_DIRS ${ogg_SOURCE_DIR}/include ${ogg_BINARY_DIR}/include)
    set(OGG_LIBRARIES ogg)

    # Create alias target and set variables for vorbis to find ogg
    if(NOT TARGET Ogg::ogg)
        add_library(Ogg::ogg ALIAS ogg)
    endif()
    set(OGG_FOUND TRUE CACHE BOOL "" FORCE)
    set(OGG_INCLUDE_DIR "${ogg_SOURCE_DIR}/include" CACHE PATH "" FORCE)
    set(OGG_LIBRARY Ogg::ogg CACHE STRING "" FORCE)

    # vorbis
    FetchContent_Declare(
        vorbis
        GIT_REPOSITORY https://github.com/xiph/vorbis.git
        GIT_TAG v1.3.7
    )
    FetchContent_MakeAvailable(vorbis)
    set(VORBIS_INCLUDE_DIRS ${vorbis_SOURCE_DIR}/include)
    set(VORBIS_LIBRARIES vorbis vorbisenc vorbisfile)

    # opus
    set(OPUS_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        opus
        GIT_REPOSITORY https://github.com/xiph/opus.git
        GIT_TAG v1.5.2
    )
    FetchContent_MakeAvailable(opus)
    set(OPUS_INCLUDE_DIRS ${opus_SOURCE_DIR}/include)
    set(OPUS_LIBRARIES opus)

    # libopusenc - needs opus to be built first
    FetchContent_Declare(
        opusenc
        GIT_REPOSITORY https://github.com/xiph/libopusenc.git
        GIT_TAG v0.2.1
    )
    FetchContent_MakeAvailable(opusenc)
    set(OPUSENC_INCLUDE_DIRS ${opusenc_SOURCE_DIR}/include)
    set(OPUSENC_LIBRARIES opusenc)

else()
    # For Windows/Desktop, use prebuilt libraries from thirdparty folder
    set(ZLIB_INCLUDE_DIRS "${THIRDPARTY_DIR}/zlib/include")
    set(ZLIB_LIBRARY_DIRS "${THIRDPARTY_DIR}/zlib/lib")
    set(ZLIB_LIBRARIES zlib)

    set(PNG_INCLUDE_DIRS "${THIRDPARTY_DIR}/libpng/include")
    set(PNG_LIBRARY_DIRS "${THIRDPARTY_DIR}/libpng/lib")
    set(PNG_LIBRARIES libpng16)

    set(JPEG_INCLUDE_DIRS "${THIRDPARTY_DIR}/libjpeg-turbo/include")
    set(JPEG_LIBRARY_DIRS "${THIRDPARTY_DIR}/libjpeg-turbo/lib")
    set(JPEG_LIBRARIES jpeg)

    set(OGG_INCLUDE_DIRS "${THIRDPARTY_DIR}/libogg/include" "${THIRDPARTY_DIR}/libogg/include/ogg")
    set(OGG_LIBRARY_DIRS "${THIRDPARTY_DIR}/libogg/lib")
    set(OGG_LIBRARIES ogg)

    set(VORBIS_INCLUDE_DIRS "${THIRDPARTY_DIR}/libvorbis/include" "${THIRDPARTY_DIR}/libvorbis/include/vorbis")
    set(VORBIS_LIBRARY_DIRS "${THIRDPARTY_DIR}/libvorbis/lib")
    set(VORBIS_LIBRARIES vorbis vorbisenc vorbisfile)

    set(OPUS_INCLUDE_DIRS "${THIRDPARTY_DIR}/opus/include/opus")
    set(OPUS_LIBRARY_DIRS "${THIRDPARTY_DIR}/opus/lib")
    set(OPUS_LIBRARIES opus)

    set(OPUSENC_INCLUDE_DIRS "${THIRDPARTY_DIR}/libopusenc/include/opus")
    set(OPUSENC_LIBRARY_DIRS "${THIRDPARTY_DIR}/libopusenc/lib")
    set(OPUSENC_LIBRARIES opusenc)

    # Audio360 (Windows only, when not using NULL audio)
    if(NOT IMM_USE_NULL_AUDIO)
        set(AUDIO360_INCLUDE_DIRS "${THIRDPARTY_DIR}/audio360-sdk/Audio360/include")
        set(AUDIO360_LIBRARY_DIRS "${THIRDPARTY_DIR}/audio360-sdk/Audio360/Windows/x64")
        set(AUDIO360_LIBRARIES Audio360)
    endif()

    # Oculus SDK (Windows only)
    set(OCULUS_INCLUDE_DIRS
        "${THIRDPARTY_DIR}/ovr-sdk/LibOVR/Include"
        "${THIRDPARTY_DIR}/ovr-platform-sdk/Include"
    )
    set(OCULUS_LIBRARY_DIRS "${THIRDPARTY_DIR}/ovr-sdk/LibOVR/Lib/Windows/x64/Release/VS2017")
    set(OCULUS_LIBRARIES LibOVR)
endif()

# Add subdirectories for libraries
add_subdirectory(libImmCore)
add_subdirectory(libImmImporter)
add_subdirectory(libImmExporter)
add_subdirectory(libImmPlayer)

if(IMM_BUILD_UNITY_PLUGIN)
    add_subdirectory(appImmUnity)
endif()
