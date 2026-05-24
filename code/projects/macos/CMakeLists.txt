cmake_minimum_required(VERSION 3.20)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(imm_macos LANGUAGES C CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

enable_testing()

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

    add_executable(appImmViewerMetal MACOSX_BUNDLE
        ${CODE_DIR}/appImmViewer/src/macos/metal_player.mm
        ${CMAKE_CURRENT_SOURCE_DIR}/appImmViewerMetal-settings.json
        ${CODE_DIR}/appImmViewer/src/resolve.cpp
        ${CODE_DIR}/appImmViewer/src/settings.cpp
        ${CODE_DIR}/appImmViewer/src/viewer/viewer.cpp
        ${CODE_DIR}/appImmViewer/src/viewer/piCameraD.cpp
        ${CODE_DIR}/appImmViewer/src/simpleJSON/JSON.cpp
        ${CODE_DIR}/appImmViewer/src/simpleJSON/JSONValue.cpp
    )
    target_include_directories(appImmViewerMetal PUBLIC ${CODE_DIR})
    target_compile_definitions(appImmViewerMetal PRIVATE DISABLE_VR=1)
    target_link_libraries(appImmViewerMetal PRIVATE
        libImmPlayer
        libImmImporter
        libImmCore
        "-framework Cocoa"
        "-framework Metal"
        "-framework MetalKit"
        "-framework QuartzCore"
    )
    set_target_properties(appImmViewerMetal PROPERTIES
        BUNDLE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/viewer
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/viewer
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/appImmViewerMetal-Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME "IMM Metal Viewer"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.immersivefoundation.imm.metalviewer"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
        MACOSX_BUNDLE_BUNDLE_VERSION "0.1.0"
    )
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/appImmViewerMetal-settings.json PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources
    )

    add_custom_target(validateAppImmViewerMetalPlayback
        COMMAND ${CODE_DIR}/appImmViewer/scripts/validate_metal_standalone.sh
            "$<TARGET_FILE:appImmViewerMetal>"
            "${CODE_DIR}/../exampleImmFiles/sample1.imm"
        DEPENDS appImmViewerMetal
        WORKING_DIRECTORY "${CODE_DIR}/.."
        COMMENT "Validating standalone macOS Metal IMM playback"
    )

    add_custom_target(validateAppImmViewerMetalCliContract
        COMMAND ${CODE_DIR}/appImmViewer/scripts/validate_metal_standalone.sh
            --cli-contract
            "$<TARGET_FILE:appImmViewerMetal>"
            "${CODE_DIR}/../exampleImmFiles/sample1.imm"
        DEPENDS appImmViewerMetal
        WORKING_DIRECTORY "${CODE_DIR}/.."
        COMMENT "Validating standalone macOS Metal command-line contract"
    )

    add_custom_target(validateAppImmViewerMetalCapture
        COMMAND ${CMAKE_COMMAND}
            -E env "IMM_METAL_VALIDATE_CAPTURE_DIR=${CMAKE_BINARY_DIR}/metal-validation-captures"
            ${CODE_DIR}/appImmViewer/scripts/validate_metal_standalone.sh
            "$<TARGET_FILE:appImmViewerMetal>"
            "${CODE_DIR}/../exampleImmFiles/sample1.imm"
        DEPENDS appImmViewerMetal
        WORKING_DIRECTORY "${CODE_DIR}/.."
        COMMENT "Validating standalone macOS Metal capture output"
    )

    add_custom_target(validateAppImmViewerMetalNativeFrameFailure
        COMMAND ${CODE_DIR}/appImmViewer/scripts/validate_metal_standalone.sh
            --native-frame-failure-contract
            "$<TARGET_FILE:appImmViewerMetal>"
            "${CODE_DIR}/../exampleImmFiles/sample1.imm"
        DEPENDS appImmViewerMetal
        WORKING_DIRECTORY "${CODE_DIR}/.."
        COMMENT "Validating standalone macOS Metal native-frame failure handling"
    )

    add_custom_target(validateAppImmViewerMetalBundle
        COMMAND ${CODE_DIR}/appImmViewer/scripts/validate_metal_standalone.sh
            --bundle-contract
            "$<TARGET_FILE:appImmViewerMetal>"
            "${CODE_DIR}/../exampleImmFiles/sample1.imm"
        DEPENDS appImmViewerMetal
        WORKING_DIRECTORY "${CODE_DIR}/.."
        COMMENT "Validating standalone macOS Metal app bundle"
    )

    add_custom_target(validateAppImmViewerMetalContentOverride
        COMMAND ${CODE_DIR}/appImmViewer/scripts/validate_metal_standalone.sh
            --content-override-contract
            "$<TARGET_FILE:appImmViewerMetal>"
            "${CODE_DIR}/../exampleImmFiles/sample1.imm"
        DEPENDS appImmViewerMetal
        WORKING_DIRECTORY "${CODE_DIR}/.."
        COMMENT "Validating standalone macOS Metal content-path launch"
    )

    add_custom_target(validateAppImmViewerMetalRepeat
        COMMAND ${CODE_DIR}/appImmViewer/scripts/validate_metal_standalone.sh
            --repeat-contract
            "$<TARGET_FILE:appImmViewerMetal>"
            "${CODE_DIR}/../exampleImmFiles/sample1.imm"
        DEPENDS appImmViewerMetal
        WORKING_DIRECTORY "${CODE_DIR}/.."
        COMMENT "Validating repeated standalone macOS Metal launch and teardown"
    )

    add_custom_target(validateAppImmViewerMetal
        DEPENDS
            validateAppImmViewerMetalPlayback
            validateAppImmViewerMetalCliContract
            validateAppImmViewerMetalCapture
            validateAppImmViewerMetalNativeFrameFailure
            validateAppImmViewerMetalBundle
            validateAppImmViewerMetalContentOverride
            validateAppImmViewerMetalRepeat
    )

    add_test(
        NAME appImmViewerMetalPlayback
        COMMAND ${CMAKE_COMMAND}
            --build "${CMAKE_BINARY_DIR}"
            --target validateAppImmViewerMetalPlayback
            --config Release
    )
    set_tests_properties(appImmViewerMetalPlayback PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        LABELS "macos;metal;standalone"
    )

    add_test(
        NAME appImmViewerMetalCliContract
        COMMAND ${CMAKE_COMMAND}
            --build "${CMAKE_BINARY_DIR}"
            --target validateAppImmViewerMetalCliContract
            --config Release
    )
    set_tests_properties(appImmViewerMetalCliContract PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        LABELS "macos;metal;standalone"
    )

    add_test(
        NAME appImmViewerMetalCapture
        COMMAND ${CMAKE_COMMAND}
            --build "${CMAKE_BINARY_DIR}"
            --target validateAppImmViewerMetalCapture
            --config Release
    )
    set_tests_properties(appImmViewerMetalCapture PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        LABELS "macos;metal;standalone"
    )

    add_test(
        NAME appImmViewerMetalNativeFrameFailure
        COMMAND ${CMAKE_COMMAND}
            --build "${CMAKE_BINARY_DIR}"
            --target validateAppImmViewerMetalNativeFrameFailure
            --config Release
    )
    set_tests_properties(appImmViewerMetalNativeFrameFailure PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        LABELS "macos;metal;standalone"
    )

    add_test(
        NAME appImmViewerMetalBundle
        COMMAND ${CMAKE_COMMAND}
            --build "${CMAKE_BINARY_DIR}"
            --target validateAppImmViewerMetalBundle
            --config Release
    )
    set_tests_properties(appImmViewerMetalBundle PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        LABELS "macos;metal;standalone"
    )

    add_test(
        NAME appImmViewerMetalContentOverride
        COMMAND ${CMAKE_COMMAND}
            --build "${CMAKE_BINARY_DIR}"
            --target validateAppImmViewerMetalContentOverride
            --config Release
    )
    set_tests_properties(appImmViewerMetalContentOverride PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        LABELS "macos;metal;standalone"
    )

    add_test(
        NAME appImmViewerMetalRepeat
        COMMAND ${CMAKE_COMMAND}
            --build "${CMAKE_BINARY_DIR}"
            --target validateAppImmViewerMetalRepeat
            --config Release
    )
    set_tests_properties(appImmViewerMetalRepeat PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        LABELS "macos;metal;standalone"
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
