cmake_minimum_required(VERSION 3.20)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(imm_macos LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "" FORCE)
endif()


if(NOT CMAKE_INSTALL_DOCDIR)
    set(CMAKE_INSTALL_DOCDIR "share/doc" CACHE STRING "" FORCE)
endif()
if(NOT CMAKE_INSTALL_MANDIR)
    set(CMAKE_INSTALL_MANDIR "share/man" CACHE STRING "" FORCE)
endif()

set(CODE_DIR ${CMAKE_CURRENT_LIST_DIR}/../..)
set(THIRDPARTY_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../thirdparty)

find_package(ZLIB REQUIRED)

set(PNG_SHARED OFF CACHE BOOL "" FORCE)
set(PNG_STATIC ON CACHE BOOL "" FORCE)
set(PNG_TESTS OFF CACHE BOOL "" FORCE)
set(PNG_ARM_NEON OFF CACHE BOOL "" FORCE)
set(PNG_ARM_NEON_OPT 0 CACHE STRING "" FORCE)
set(PNG_HARDWARE_OPTIMIZATIONS OFF CACHE BOOL "" FORCE)
set(SKIP_INSTALL_LIBRARIES ON CACHE BOOL "" FORCE)
set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
add_subdirectory(${THIRDPARTY_DIR}/lpng1637 ${CMAKE_BINARY_DIR}/libpng-build)

add_subdirectory(${THIRDPARTY_DIR}/ogg ${CMAKE_BINARY_DIR}/ogg-build)

set(ENABLE_SHARED OFF CACHE BOOL "" FORCE)
set(ENABLE_STATIC ON CACHE BOOL "" FORCE)
set(WITH_TURBOJPEG ON CACHE BOOL "" FORCE)
set(WITH_JAVA OFF CACHE BOOL "" FORCE)
add_subdirectory(${THIRDPARTY_DIR}/libjpeg-turbo/libjpeg-turbo ${CMAKE_BINARY_DIR}/libjpeg-turbo-build)

set(IMMCORE_SOURCES
    ${CODE_DIR}/libImmCore/src/libBasics/piArray.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/piPool.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/piString.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/piTick.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/piImage.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/piStr.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/piFile.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/piFileName.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/piTypes.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/macos/piLog.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/macos/piFileOS.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/macos/piTimer.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/macos/piMutex.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/macos/piThread.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/macos/piSystemInfo.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/macos/piWindow.mm
    ${CODE_DIR}/libImmCore/src/libCompression/basic/piBitInterlacer.cpp
    ${CODE_DIR}/libImmCore/src/libCompression/basic/piPredictors.cpp
    ${CODE_DIR}/libImmCore/src/libCompression/basic/piQuantize.cpp
    ${CODE_DIR}/libImmCore/src/libCompression/basic/piTransforms.cpp
    ${CODE_DIR}/libImmCore/src/libMesh/piMesh.cpp
    ${CODE_DIR}/libImmCore/src/libMesh/piRenderMesh.cpp
    ${CODE_DIR}/libImmCore/src/libRender/piRenderer.cpp
    ${CODE_DIR}/libImmCore/src/libRender/opengl4x/piGL4X_Renderer.cpp
    ${CODE_DIR}/libImmCore/src/libRender/opengl4x/piGL4X_Ext.cpp
    ${CODE_DIR}/libImmCore/src/libRender/opengl4x/macos/piGL4X_RenderContextOS.cpp
    ${CODE_DIR}/libImmCore/src/libRender/metal/piMetal_Renderer.mm
    ${CODE_DIR}/libImmCore/src/libWave/piWave.cpp
    ${CODE_DIR}/libImmCore/src/libWave/formats/piWaveWAV.cpp
    ${CODE_DIR}/libImmCore/src/libWave/formats/piWaveOGG.cpp
    ${CODE_DIR}/libImmCore/src/libSound/piSound.cpp
    ${CODE_DIR}/libImmCore/src/libSound/piSoundEngineNULL.cpp
)

add_library(libImmCore STATIC ${IMMCORE_SOURCES})
target_include_directories(libImmCore
    PUBLIC
        ${CODE_DIR}
        ${THIRDPARTY_DIR}/ogg/include
        ${THIRDPARTY_DIR}/ogg/libvorbis-1.3.5/include
        ${THIRDPARTY_DIR}/ogg/libvorbis-1.3.5/include/vorbis
        ${THIRDPARTY_DIR}/ogg/libvorbis-1.3.5/lib
        ${THIRDPARTY_DIR}/lpng1637
        ${CMAKE_BINARY_DIR}/libpng-build
        ${THIRDPARTY_DIR}/libjpeg-turbo/libjpeg-turbo
        ${THIRDPARTY_DIR}/libjpeg-turbo/libjpeg-turbo/include
        ${CMAKE_BINARY_DIR}/libjpeg-turbo-build
)
target_link_libraries(libImmCore
    PUBLIC
        ogg
        vorbis
        png_static
        turbojpeg-static
        ZLIB::ZLIB
        "-framework Metal"
        "-framework MetalKit"
        "-framework QuartzCore"
        "-framework Foundation"
        "-framework OpenGL"
        "-framework CoreGraphics"
        "-framework Cocoa"
)

# ----------------------------------------------------------------------------
# Minimal core (no rendering/window/audio) for stroke reader and iOS
# ----------------------------------------------------------------------------

set(IMMCORE_MIN_SOURCES
    ${CODE_DIR}/libImmCore/src/libBasics/piArray.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/piPool.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/piString.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/piTick.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/piImage.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/piStr.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/piFile.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/piFileName.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/piTypes.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/macos/piLog.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/macos/piFileOS.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/macos/piTimer.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/macos/piMutex.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/macos/piThread.cpp
    ${CODE_DIR}/libImmCore/src/libBasics/ios/piSystemInfo.cpp
    ${CODE_DIR}/libImmCore/src/libCompression/basic/piBitInterlacer.cpp
    ${CODE_DIR}/libImmCore/src/libCompression/basic/piPredictors.cpp
    ${CODE_DIR}/libImmCore/src/libCompression/basic/piQuantize.cpp
    ${CODE_DIR}/libImmCore/src/libCompression/basic/piTransforms.cpp
    ${CODE_DIR}/libImmCore/src/libMesh/piMesh.cpp
    ${CODE_DIR}/libImmCore/src/libWave/piWave.cpp
    ${CODE_DIR}/libImmCore/src/libWave/formats/piWaveWAV.cpp
    ${CODE_DIR}/libImmCore/src/libWave/formats/piWaveOGG.cpp
)

add_library(libImmCoreMinimal STATIC ${IMMCORE_MIN_SOURCES})
target_include_directories(libImmCoreMinimal
    PUBLIC
        ${CODE_DIR}
        ${THIRDPARTY_DIR}/ogg/include
        ${THIRDPARTY_DIR}/ogg/libvorbis-1.3.5/include
        ${THIRDPARTY_DIR}/ogg/libvorbis-1.3.5/include/vorbis
        ${THIRDPARTY_DIR}/ogg/libvorbis-1.3.5/lib
        ${THIRDPARTY_DIR}/lpng1637
        ${CMAKE_BINARY_DIR}/libpng-build
        ${THIRDPARTY_DIR}/libjpeg-turbo/libjpeg-turbo
        ${THIRDPARTY_DIR}/libjpeg-turbo/libjpeg-turbo/include
        ${CMAKE_BINARY_DIR}/libjpeg-turbo-build
)
target_link_libraries(libImmCoreMinimal
    PUBLIC
        ogg
        vorbis
        png_static
        turbojpeg-static
        ZLIB::ZLIB
)

set(IMMIMPORTER_SOURCES
    ${CODE_DIR}/libImmImporter/src/document/layerPaint/element.cpp
    ${CODE_DIR}/libImmImporter/src/document/layerPaint/drawingPretessellated.cpp
    ${CODE_DIR}/libImmImporter/src/document/layerPaint/drawingStatic.cpp
    ${CODE_DIR}/libImmImporter/src/document/sequence.cpp
    ${CODE_DIR}/libImmImporter/src/document/layer.cpp
    ${CODE_DIR}/libImmImporter/src/document/behaviors/keepalive.cpp
    ${CODE_DIR}/libImmImporter/src/document/layerModel3d.cpp
    ${CODE_DIR}/libImmImporter/src/document/layerPicture.cpp
    ${CODE_DIR}/libImmImporter/src/document/layerSound.cpp
    ${CODE_DIR}/libImmImporter/src/document/layerInstance.cpp
    ${CODE_DIR}/libImmImporter/src/document/layerPaintStatic.cpp
    ${CODE_DIR}/libImmImporter/src/document/layerPaintPretessellated.cpp
    ${CODE_DIR}/libImmImporter/src/document/layerSpawnArea.cpp
    ${CODE_DIR}/libImmImporter/src/fromImmersive/fromImmersive.cpp
    ${CODE_DIR}/libImmImporter/src/fromImmersive/fromImmersiveLayer.cpp
    ${CODE_DIR}/libImmImporter/src/fromImmersive/fromImmersiveLayerPaint.cpp
    ${CODE_DIR}/libImmImporter/src/fromImmersive/fromImmersiveLayerModel.cpp
    ${CODE_DIR}/libImmImporter/src/fromImmersive/fromImmersiveLayerSound.cpp
    ${CODE_DIR}/libImmImporter/src/fromImmersive/fromImmersiveLayerPicture.cpp
    ${CODE_DIR}/libImmImporter/src/fromImmersive/fromImmersiveLayerSpawnArea.cpp
    ${CODE_DIR}/libImmImporter/src/fromImmersive/fromImmersiveUtils.cpp
)

add_library(libImmImporter STATIC ${IMMIMPORTER_SOURCES})
target_include_directories(libImmImporter PUBLIC ${CODE_DIR})
target_link_libraries(libImmImporter PUBLIC libImmCore)

add_library(libImmImporterMinimal STATIC ${IMMIMPORTER_SOURCES})
target_include_directories(libImmImporterMinimal PUBLIC ${CODE_DIR})
target_link_libraries(libImmImporterMinimal PUBLIC libImmCoreMinimal)

set(IMMPLAYER_SOURCES
    ${CODE_DIR}/libImmPlayer/src/player.cpp
    ${CODE_DIR}/libImmPlayer/src/document.cpp
    ${CODE_DIR}/libImmPlayer/src/blue_noise.cpp
    ${CODE_DIR}/libImmPlayer/src/mngrPlayer.cpp
    ${CODE_DIR}/libImmPlayer/src/layerRenderers/layerRendererPaint/static/layerRendererPaintStatic.cpp
    ${CODE_DIR}/libImmPlayer/src/layerRenderers/layerRendererPaint/pretessellated/layerRendererPaintPretessellated.cpp
    ${CODE_DIR}/libImmPlayer/src/layerRenderers/layerRendererPicture/layerRendererPicture.cpp
    ${CODE_DIR}/libImmPlayer/src/layerRenderers/layerRendererSound/layerRendererSound.cpp
    ${CODE_DIR}/libImmPlayer/src/layerRenderers/layerRendererModel/layerRendererModel.cpp
)

add_library(libImmPlayer STATIC ${IMMPLAYER_SOURCES})
target_include_directories(libImmPlayer PUBLIC ${CODE_DIR})
target_link_libraries(libImmPlayer PUBLIC libImmImporter libImmCore)

add_custom_target(
    ShadersModel
    COMMAND ${CMAKE_COMMAND}
    -D INPUT=${CODE_DIR}/libImmPlayer/src/layerRenderers/layerRendererModel/gles
    -D OUTPUT=${CODE_DIR}/libImmPlayer/src/layerRenderers/layerRendererModel/tmp/shader_model_GLES_glsl.h
    -D NAMESPACE=INVALID
    -P ${CODE_DIR}/libImmPlayer/create_shader.cmake
)
add_custom_target(
    ShadersPretessellated
    COMMAND ${CMAKE_COMMAND}
    -D INPUT=${CODE_DIR}/libImmPlayer/src/layerRenderers/layerRendererPaint/pretessellated
    -D OUTPUT=${CODE_DIR}/libImmPlayer/src/layerRenderers/layerRendererPaint/pretessellated/tmp/shader_pretessellated_brush_GLES_glsl.h
    -D NAMESPACE=INVALID
    -P ${CODE_DIR}/libImmPlayer/create_shader.cmake
)
add_dependencies(libImmPlayer ShadersModel ShadersPretessellated)

add_library(ImmUnity MODULE ${CODE_DIR}/appImmUnity/src/main.cpp)
target_include_directories(ImmUnity PUBLIC ${CODE_DIR})
target_link_libraries(ImmUnity PRIVATE libImmPlayer libImmImporter libImmCore)
set_target_properties(ImmUnity PROPERTIES
    BUNDLE TRUE
    OUTPUT_NAME "ImmUnityPlugin"
    PREFIX ""
)

set_target_properties(ImmUnity PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/unity
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/unity
)

# -----------------------------------------------------------------------------
# ImmGodot native plugin (macOS)
# -----------------------------------------------------------------------------

add_library(ImmGodotPlugin SHARED
    ${CODE_DIR}/appImmGodot/src/main.cpp
    ${CODE_DIR}/appImmShared/src/imm_engine_bridge.cpp
)
target_include_directories(ImmGodotPlugin PUBLIC ${CODE_DIR})
target_link_libraries(ImmGodotPlugin PRIVATE libImmPlayer libImmImporter libImmCore)
set_target_properties(ImmGodotPlugin PROPERTIES
    OUTPUT_NAME "ImmGodotPlugin"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/godot
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/godot
)

# -----------------------------------------------------------------------------
# ImmStrokeReader Unity plugin (macOS)
# -----------------------------------------------------------------------------

set(IMMSTROKEREADER_SOURCES
    ${CODE_DIR}/appImmStrokeReader/src/main.cpp
    ${CODE_DIR}/appImmStrokeReader/src/strokeStore.cpp
)

add_library(ImmStrokeReader SHARED ${IMMSTROKEREADER_SOURCES})
target_include_directories(ImmStrokeReader PUBLIC ${CODE_DIR})
target_include_directories(ImmStrokeReader PRIVATE ${CODE_DIR}/libImmCore/src/libBasics)
target_link_libraries(ImmStrokeReader PRIVATE libImmImporterMinimal libImmCoreMinimal)
set_target_properties(ImmStrokeReader PROPERTIES
    OUTPUT_NAME "ImmStrokeReader"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/stroke-reader
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/stroke-reader
)

option(IMM_BUILD_VIEWER "Build the macOS ImmViewer app" ON)

if(IMM_BUILD_VIEWER)
    set(IMMVIEWER_SOURCES
        ${CODE_DIR}/appImmViewer/src/macos/main.mm
        ${CODE_DIR}/appImmViewer/src/mymain.cpp
        ${CODE_DIR}/appImmViewer/src/resolve.cpp
        ${CODE_DIR}/appImmViewer/src/settings.cpp
        ${CODE_DIR}/appImmViewer/src/viewer/viewer.cpp
        ${CODE_DIR}/appImmViewer/src/viewer/piCameraD.cpp
        ${CODE_DIR}/appImmViewer/src/simpleJSON/JSON.cpp
        ${CODE_DIR}/appImmViewer/src/simpleJSON/JSONValue.cpp
    )

    add_executable(appImmViewer ${IMMVIEWER_SOURCES})
    target_include_directories(appImmViewer PUBLIC ${CODE_DIR})
    target_link_libraries(appImmViewer PRIVATE libImmPlayer libImmImporter libImmCore)
    target_compile_definitions(appImmViewer PRIVATE DISABLE_VR=1)
    set_target_properties(appImmViewer PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/viewer
    )
endif()

set(IMMUNITY_SAMPLE_DIR ${CODE_DIR}/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/OSX)
add_custom_command(TARGET ImmUnity POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${IMMUNITY_SAMPLE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_BUNDLE_DIR:ImmUnity>" "${IMMUNITY_SAMPLE_DIR}/ImmUnityPlugin.bundle"
)

set(IMMSTROKEREADER_UPM_MAC_DIR ${CODE_DIR}/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/macOS)
add_custom_command(TARGET ImmStrokeReader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${IMMSTROKEREADER_UPM_MAC_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:ImmStrokeReader>" "${IMMSTROKEREADER_UPM_MAC_DIR}/$<TARGET_FILE_NAME:ImmStrokeReader>"
)
