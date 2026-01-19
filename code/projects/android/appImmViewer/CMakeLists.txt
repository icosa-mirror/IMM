# appImmViewer - CMakeLists.txt for Android NDK build (NativeActivity)
cmake_minimum_required(VERSION 3.4.1)
project(appImmViewer)

set(IMM_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)
set(APP_VIEWER_DIR ${IMM_ROOT}/code/appImmViewer)
set(THIRDPARTY_DIR ${IMM_ROOT}/thirdparty)

if (IMM_ANDROID_NON_VR)
    set(APP_VIEWER_ANDROID_SRC
        ${APP_VIEWER_DIR}/src/android/cpp/NonVrApp.cpp
    )
else()
    file(GLOB APP_VIEWER_ANDROID_SRC
        ${APP_VIEWER_DIR}/src/android/cpp/*.cpp
    )
    list(FILTER APP_VIEWER_ANDROID_SRC EXCLUDE REGEX "NonVrApp.cpp$")
endif()
file(GLOB APP_VIEWER_CORE_SRC
    ${APP_VIEWER_DIR}/src/viewer/*.cpp
)

add_library(
    appImmPlayer
    SHARED
    ${APP_VIEWER_ANDROID_SRC}
    ${APP_VIEWER_CORE_SRC}
    ${APP_VIEWER_DIR}/src/settings.cpp
    ${APP_VIEWER_DIR}/src/simpleJSON/JSON.cpp
    ${APP_VIEWER_DIR}/src/simpleJSON/JSONValue.cpp
)

target_include_directories(appImmPlayer PRIVATE
    ${IMM_ROOT}/code
    ${APP_VIEWER_DIR}/src
    ${APP_VIEWER_DIR}/src/android/cpp
)

# NativeAppGlue for android_native_app_glue.h/android_native_app_glue.c
set(NATIVE_APP_GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)
target_include_directories(appImmPlayer PRIVATE
    ${NATIVE_APP_GLUE_DIR}
)
target_sources(appImmPlayer PRIVATE
    ${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c
)

if (IMM_ANDROID_NON_VR)
    target_compile_definitions(appImmPlayer PRIVATE IMM_ANDROID_NON_VR=1)
endif()

# libImmCore dependency
set(LIB_IMM_CORE_DIR ${IMM_ROOT}/code/libImmCore)
set(LIB_IMM_CORE_BUILD_DIR ${LIB_IMM_CORE_DIR}/build)

add_library(libImmCore STATIC IMPORTED)
set_target_properties(libImmCore PROPERTIES IMPORTED_LOCATION
    ${LIB_IMM_CORE_BUILD_DIR}/libs/${ANDROID_ABI}/liblibImmCore.a)

# libImmImporter dependency
set(LIB_IMM_IMPORTER_DIR ${IMM_ROOT}/code/libImmImporter)
set(LIB_IMM_IMPORTER_BUILD_DIR ${LIB_IMM_IMPORTER_DIR}/build)

add_library(libImmImporter STATIC IMPORTED)
set_target_properties(libImmImporter PROPERTIES IMPORTED_LOCATION
    ${LIB_IMM_IMPORTER_BUILD_DIR}/libs/${ANDROID_ABI}/liblibImmImporter.a)

# libImmPlayer dependency
set(LIB_IMM_PLAYER_DIR ${IMM_ROOT}/code/libImmPlayer)
set(LIB_IMM_PLAYER_BUILD_DIR ${LIB_IMM_PLAYER_DIR}/build)

target_include_directories(appImmPlayer PRIVATE
    ${LIB_IMM_PLAYER_DIR}/src
)

add_library(libImmPlayer STATIC IMPORTED)
set_target_properties(libImmPlayer PROPERTIES IMPORTED_LOCATION
    ${LIB_IMM_PLAYER_BUILD_DIR}/libs/${ANDROID_ABI}/liblibImmPlayer.a)

# OGG/Vorbis from thirdparty
set(OGG_THIRDPARTY_DIR ${THIRDPARTY_DIR}/ogg)
target_include_directories(appImmPlayer PRIVATE
    ${OGG_THIRDPARTY_DIR}/include
    ${OGG_THIRDPARTY_DIR}/libvorbis-1.3.5/include
)

# libpng (built by libImmCore)
target_include_directories(appImmPlayer PRIVATE
    ${THIRDPARTY_DIR}/lpng1637
)

add_library(png-lib STATIC IMPORTED)
set_target_properties(png-lib PROPERTIES IMPORTED_LOCATION
    ${LIB_IMM_CORE_BUILD_DIR}/libs/${ANDROID_ABI}/libpng16d.a)

# libjpeg-turbo
add_library(jpeg-lib STATIC IMPORTED)
set_target_properties(jpeg-lib PROPERTIES IMPORTED_LOCATION
    ${THIRDPARTY_DIR}/libjpeg-turbo/lib/${ANDROID_ABI}/libturbojpeg.a)

# Vorbis libraries (built by libImmCore)
add_library(vorbis-lib STATIC IMPORTED)
set_target_properties(vorbis-lib PROPERTIES IMPORTED_LOCATION
    ${LIB_IMM_CORE_BUILD_DIR}/libs/${ANDROID_ABI}/libvorbis.a)

add_library(ogg-lib STATIC IMPORTED)
set_target_properties(ogg-lib PROPERTIES IMPORTED_LOCATION
    ${LIB_IMM_CORE_BUILD_DIR}/libs/${ANDROID_ABI}/libogg.a)

# Oculus Mobile SDK (VrApi) and Platform SDK (VR builds only)
if (NOT IMM_ANDROID_NON_VR)
    if (NOT OCULUS_MOBILE_SDK_DIR AND DEFINED ENV{OCULUS_MOBILE_SDK_DIR})
        set(OCULUS_MOBILE_SDK_DIR $ENV{OCULUS_MOBILE_SDK_DIR})
    endif()
    if (NOT OCULUS_MOBILE_SDK_DIR)
        set(OCULUS_MOBILE_SDK_DIR ${THIRDPARTY_DIR}/oculus-mobile-sdk)
    endif()
    if (NOT EXISTS ${OCULUS_MOBILE_SDK_DIR})
        message(FATAL_ERROR "Oculus Mobile SDK not found at ${OCULUS_MOBILE_SDK_DIR}")
    endif()

    target_include_directories(appImmPlayer PRIVATE
        ${OCULUS_MOBILE_SDK_DIR}/VrApi/Include
    )

    add_library(vrapi SHARED IMPORTED)
    set(OVR_VRAPI_BASE ${OCULUS_MOBILE_SDK_DIR}/VrApi/Libs/Android/${ANDROID_ABI})
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(OVR_VRAPI_PATH ${OVR_VRAPI_BASE}/Debug/libvrapi.so)
    else()
        set(OVR_VRAPI_PATH ${OVR_VRAPI_BASE}/Release/libvrapi.so)
    endif()
    if (NOT EXISTS ${OVR_VRAPI_PATH})
        set(OVR_VRAPI_PATH ${OVR_VRAPI_BASE}/libvrapi.so)
    endif()
    if (NOT EXISTS ${OVR_VRAPI_PATH})
        message(FATAL_ERROR "VrApi libvrapi.so not found at ${OVR_VRAPI_BASE}")
    endif()
    set_target_properties(vrapi PROPERTIES IMPORTED_LOCATION
        ${OVR_VRAPI_PATH})

    set(OVR_PLATFORM_SDK_DIR ${THIRDPARTY_DIR}/ovr-platform-sdk)
    if (NOT EXISTS ${OVR_PLATFORM_SDK_DIR})
        message(FATAL_ERROR "Oculus Platform SDK not found at ${OVR_PLATFORM_SDK_DIR}")
    endif()

    target_include_directories(appImmPlayer PRIVATE
        ${OVR_PLATFORM_SDK_DIR}/Include
    )

    if (EXISTS ${OVR_PLATFORM_SDK_DIR}/Android/${ANDROID_ABI}/libovrplatformloader.so)
        add_library(ovrplatformloader SHARED IMPORTED)
        set_target_properties(ovrplatformloader PROPERTIES IMPORTED_LOCATION
            ${OVR_PLATFORM_SDK_DIR}/Android/${ANDROID_ABI}/libovrplatformloader.so)
    endif()
endif()

# Android system libraries
find_library(android-lib android)
find_library(log-lib log)
find_library(egl-lib EGL)
find_library(gl3-lib GLESv3)
find_library(opensles-lib OpenSLES)
find_library(mediandk-lib mediandk)
find_library(z-lib z)

target_link_libraries(
    appImmPlayer
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
)

if (NOT IMM_ANDROID_NON_VR)
    target_link_libraries(appImmPlayer vrapi)
    if (TARGET ovrplatformloader)
        target_link_libraries(appImmPlayer ovrplatformloader)
    endif()
endif()
