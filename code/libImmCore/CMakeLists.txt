# libImmCore - Core utilities library for IMM

# Common source files (cross-platform)
set(IMMCORE_COMMON_SOURCES
    src/libBasics/piArray.cpp
    src/libBasics/piFile.cpp
    src/libBasics/piFileName.cpp
    src/libBasics/piImage.cpp
    src/libBasics/piPool.cpp
    src/libBasics/piStr.cpp
    src/libBasics/piString.cpp
    src/libBasics/piTick.cpp
    src/libBasics/piTypes.cpp
    src/libCompression/basic/piBitInterlacer.cpp
    src/libCompression/basic/piPredictors.cpp
    src/libCompression/basic/piQuantize.cpp
    src/libCompression/basic/piTransforms.cpp
    src/libMesh/piMesh.cpp
    src/libMesh/piRenderMesh.cpp
    src/libRender/piRenderer.cpp
    src/libSound/piSound.cpp
    src/libSound/piSoundEngineNULL.cpp
    src/libVR/piVR.cpp
    src/libWave/piWave.cpp
    src/libWave/formats/piWaveOGG.cpp
    src/libWave/formats/piWaveOPUS.cpp
    src/libWave/formats/piWaveWAV.cpp
)

# Platform-specific sources
if(ANDROID)
    set(IMMCORE_PLATFORM_SOURCES
        src/libBasics/android/piCStr.cpp
        src/libBasics/android/piFileOS.cpp
        src/libBasics/android/piLog.cpp
        src/libBasics/android/piMutex.cpp
        src/libBasics/android/piTimer.cpp
        src/libRender/opengles/piGLES_Renderer.cpp
    )
elseif(WIN32)
    set(IMMCORE_PLATFORM_SOURCES
        src/libBasics/windows/piDebug.cpp
        src/libBasics/windows/piFileOS.cpp
        src/libBasics/windows/piLog.cpp
        src/libBasics/windows/piMutex.cpp
        src/libBasics/windows/piSystemInfo.cpp
        src/libBasics/windows/piThread.cpp
        src/libBasics/windows/piTimer.cpp
        src/libBasics/windows/piWindow.cpp
        src/libRender/directx11/piDX11_Renderer.cpp
        src/libRender/opengl4x/piGL4X_Ext.cpp
        src/libRender/opengl4x/piGL4X_Renderer.cpp
        src/libRender/opengl4x/windows/piGL4X_RenderContextOS.cpp
        src/libVR/oculus/piOculus.cpp
        src/libVR/vive/piVive.cpp
    )
    # Audio backends (Windows only)
    if(NOT IMM_USE_NULL_AUDIO)
        list(APPEND IMMCORE_PLATFORM_SOURCES
            src/libSound/windows/piSoundEngineAudioSDKBackend.cpp
            src/libSound/windows/piSoundEngineDS.cpp
        )
    endif()
endif()

# Create static library
add_library(ImmCore STATIC
    ${IMMCORE_COMMON_SOURCES}
    ${IMMCORE_PLATFORM_SOURCES}
)

# Include directories
target_include_directories(ImmCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..  # For libImmCore/src/... includes from dependent targets
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libBasics
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libCompression
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libMesh
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libRender
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libSound
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libVR
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libWave
)

# Thirdparty include directories
target_include_directories(ImmCore PUBLIC
    ${ZLIB_INCLUDE_DIRS}
    ${PNG_INCLUDE_DIRS}
    ${JPEG_INCLUDE_DIRS}
    ${OGG_INCLUDE_DIRS}
    ${VORBIS_INCLUDE_DIRS}
    ${OPUS_INCLUDE_DIRS}
    ${OPUSENC_INCLUDE_DIRS}
)

# Platform-specific includes and definitions
if(ANDROID)
    target_compile_definitions(ImmCore PUBLIC IMM_NULL_AUDIO)
elseif(WIN32)
    target_include_directories(ImmCore PUBLIC ${OCULUS_INCLUDE_DIRS})
    if(NOT IMM_USE_NULL_AUDIO)
        target_include_directories(ImmCore PUBLIC ${AUDIO360_INCLUDE_DIRS})
    else()
        target_compile_definitions(ImmCore PUBLIC IMM_NULL_AUDIO)
    endif()
endif()

# Link directories (for prebuilt libs on Windows)
if(WIN32)
    target_link_directories(ImmCore PUBLIC
        ${ZLIB_LIBRARY_DIRS}
        ${PNG_LIBRARY_DIRS}
        ${JPEG_LIBRARY_DIRS}
        ${OGG_LIBRARY_DIRS}
        ${VORBIS_LIBRARY_DIRS}
        ${OPUS_LIBRARY_DIRS}
        ${OPUSENC_LIBRARY_DIRS}
        ${OCULUS_LIBRARY_DIRS}
    )
    if(NOT IMM_USE_NULL_AUDIO)
        target_link_directories(ImmCore PUBLIC ${AUDIO360_LIBRARY_DIRS})
    endif()
endif()

# Link libraries
if(ANDROID)
    target_link_libraries(ImmCore PUBLIC
        ${ZLIB_LIBRARIES}
        ${PNG_LIBRARIES}
        ${JPEG_LIBRARIES}
        ${OGG_LIBRARIES}
        ${VORBIS_LIBRARIES}
        ${OPUS_LIBRARIES}
        android
        log
        EGL
        GLESv3
    )
elseif(WIN32)
    target_link_libraries(ImmCore PUBLIC
        ${ZLIB_LIBRARIES}
        ${PNG_LIBRARIES}
        ${JPEG_LIBRARIES}
        ${OGG_LIBRARIES}
        ${VORBIS_LIBRARIES}
        ${OPUS_LIBRARIES}
        ${OPUSENC_LIBRARIES}
        ${OCULUS_LIBRARIES}
        d3d11
        opengl32
        ws2_32
        winmm
    )
    if(NOT IMM_USE_NULL_AUDIO)
        target_link_libraries(ImmCore PUBLIC ${AUDIO360_LIBRARIES})
    endif()
endif()
