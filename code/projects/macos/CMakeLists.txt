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

set(OPUS_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_DISABLE_INTRINSICS ON CACHE BOOL "" FORCE)
set(OPUS_MAY_HAVE_NEON OFF CACHE BOOL "" FORCE)
set(OPUS_PRESUME_NEON OFF CACHE BOOL "" FORCE)
add_subdirectory(${THIRDPARTY_DIR}/opus-src ${CMAKE_BINARY_DIR}/opus-build)

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
    ${CODE_DIR}/libImmCore/src/libSound/macos/piSoundEngineAVFoundation.mm
)

add_library(libImmCore STATIC ${IMMCORE_SOURCES})
target_include_directories(libImmCore
    PUBLIC
        ${CODE_DIR}
        ${THIRDPARTY_DIR}/ogg/include
        ${THIRDPARTY_DIR}/opus-src/include
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
        opus
        png_static
        turbojpeg-static
        ZLIB::ZLIB
        "-framework Metal"
        "-framework MetalKit"
        "-framework QuartzCore"
        "-framework Foundation"
        "-framework AVFoundation"
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

set_source_files_properties(${CODE_DIR}/appImmUnity/src/main.cpp PROPERTIES LANGUAGE OBJCXX)
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

set(IMMUNITY_UPM_OSX_DIR ${CODE_DIR}/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/OSX)
add_custom_command(TARGET ImmUnity POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${IMMUNITY_UPM_OSX_DIR}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${IMMUNITY_UPM_OSX_DIR}/ImmUnityPlugin.bundle"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_BUNDLE_DIR:ImmUnity>" "${IMMUNITY_UPM_OSX_DIR}/ImmUnityPlugin.bundle"
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

add_executable(ImmPictureScan ${CODE_DIR}/appImmTools/ImmPictureScan.cpp)
target_include_directories(ImmPictureScan PUBLIC ${CODE_DIR})
target_include_directories(ImmPictureScan PRIVATE ${CODE_DIR}/libImmCore/src/libBasics)
target_link_libraries(ImmPictureScan PRIVATE libImmImporterMinimal libImmCoreMinimal)
set_target_properties(ImmPictureScan PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools
)

option(IMM_BUILD_VIEWER "Build the macOS ImmViewer app" ON)
set(IMM_METAL_CONTENT_SWEEP_PATH "" CACHE PATH "Optional local .imm file or directory for standalone Metal content sweep validation")
set(IMM_METAL_CONTENT_SWEEP_MAX_FILES "100" CACHE STRING "Maximum number of .imm files to render in the optional standalone Metal content sweep; set 0 for unbounded")
set(IMM_METAL_CONTENT_SWEEP_MAX_BYTES "50000000" CACHE STRING "Maximum .imm file size in bytes for the optional standalone Metal content sweep; set 0 for unbounded")
set(IMM_METAL_CONTENT_SWEEP_MAX_FRAME "300" CACHE STRING "Validation max frame for each file in the optional standalone Metal content sweep")
set(IMM_METAL_CONTENT_SWEEP_MIN_PASSED "1" CACHE STRING "Minimum number of nonblank passed renders required by the optional standalone Metal content sweep")
set(IMM_METAL_CONTENT_SWEEP_FAIL_ON_FAILED "0" CACHE STRING "Set to 1 to make the optional standalone Metal content sweep fail on any nonblank validation failure")
set(IMM_METAL_PICTURE_SCAN_PATH "" CACHE PATH "Optional local .imm file or directory for importer-backed picture metadata scanning")
set(IMM_METAL_PICTURE_SCAN_MAX_FILES "200" CACHE STRING "Maximum number of .imm files to scan in the optional picture metadata scan")
set(IMM_METAL_PICTURE_SCAN_SKIP_FILES "0" CACHE STRING "Number of matching .imm files to skip before the optional picture metadata scan starts")
set(IMM_METAL_PICTURE_SCAN_MAX_SIZE_MB "200" CACHE STRING "Maximum .imm file size in MB for the optional picture metadata scan")
set(IMM_METAL_PICTURE_SCAN_TIMEOUT_SEC "30" CACHE STRING "Per-file timeout in seconds for the optional picture metadata scan")
set(IMM_METAL_PICTURE_SCAN_NAME_REGEX "" CACHE STRING "Optional regular expression that matching .imm paths must satisfy before picture metadata scanning")
set(IMM_METAL_PICTURE_SCAN_MIN_CUBEMAP_FILES "0" CACHE STRING "Minimum number of cubemap picture files required by the optional picture metadata scan")
set(IMM_METAL_AUTHORED_AUDIO_PATH "" CACHE FILEPATH "Optional local .imm file with authored audio layers for standalone Metal validation")
set(IMM_METAL_AUTHORED_2D_PICTURE_PATH "" CACHE FILEPATH "Optional local .imm file with authored 2D picture layers for standalone Metal validation")
set(IMM_METAL_AUTHORED_360_PICTURE_PATH "" CACHE FILEPATH "Optional local .imm file with authored 360 picture layers for standalone Metal validation")
set(IMM_METAL_AUTHORED_360_CUBEMAP_PATH "" CACHE FILEPATH "Optional local .imm file with authored 360 cubemap picture layers for standalone Metal validation")
set(IMM_METAL_REFERENCE_CAPTURE_PATH "" CACHE FILEPATH "Optional known-good non-Metal PNG/PPM capture to compare against a fresh standalone Metal sample1 static capture")
set(IMM_METAL_REFERENCE_MAX_MEAN_ABS "" CACHE STRING "Optional maximum mean absolute channel difference for standalone Metal reference comparison")
set(IMM_METAL_REFERENCE_MAX_RMS "" CACHE STRING "Optional maximum RMS channel difference for standalone Metal reference comparison")
set(IMM_METAL_REFERENCE_MAX_CHANNEL_DIFF "" CACHE STRING "Optional maximum single-channel difference for standalone Metal reference comparison")
set(IMM_METAL_REFERENCE_MAX_DIFFERING_PERCENT "" CACHE STRING "Optional maximum differing pixel percentage for standalone Metal reference comparison")

set(IMM_METAL_REFERENCE_COMPARE_ARGS)
if(NOT IMM_METAL_REFERENCE_MAX_MEAN_ABS STREQUAL "")
    list(APPEND IMM_METAL_REFERENCE_COMPARE_ARGS --max-mean-abs "${IMM_METAL_REFERENCE_MAX_MEAN_ABS}")
endif()
if(NOT IMM_METAL_REFERENCE_MAX_RMS STREQUAL "")
    list(APPEND IMM_METAL_REFERENCE_COMPARE_ARGS --max-rms "${IMM_METAL_REFERENCE_MAX_RMS}")
endif()
if(NOT IMM_METAL_REFERENCE_MAX_CHANNEL_DIFF STREQUAL "")
    list(APPEND IMM_METAL_REFERENCE_COMPARE_ARGS --max-channel-diff "${IMM_METAL_REFERENCE_MAX_CHANNEL_DIFF}")
endif()
if(NOT IMM_METAL_REFERENCE_MAX_DIFFERING_PERCENT STREQUAL "")
    list(APPEND IMM_METAL_REFERENCE_COMPARE_ARGS --max-differing-percent "${IMM_METAL_REFERENCE_MAX_DIFFERING_PERCENT}")
endif()

set(IMM_METAL_PICTURE_SCAN_ARGS)
if(NOT IMM_METAL_PICTURE_SCAN_NAME_REGEX STREQUAL "")
    list(APPEND IMM_METAL_PICTURE_SCAN_ARGS --name-regex "${IMM_METAL_PICTURE_SCAN_NAME_REGEX}")
endif()

if(IMM_METAL_PICTURE_SCAN_PATH)
    add_custom_target(scanImmPictureLayers
        COMMAND ${CODE_DIR}/appImmViewer/scripts/scan_imm_picture_layers.sh
            --tool "$<TARGET_FILE:ImmPictureScan>"
            --max-files "${IMM_METAL_PICTURE_SCAN_MAX_FILES}"
            --skip-files "${IMM_METAL_PICTURE_SCAN_SKIP_FILES}"
            --max-size-mb "${IMM_METAL_PICTURE_SCAN_MAX_SIZE_MB}"
            --per-file-timeout-sec "${IMM_METAL_PICTURE_SCAN_TIMEOUT_SEC}"
            --min-cubemap-files "${IMM_METAL_PICTURE_SCAN_MIN_CUBEMAP_FILES}"
            ${IMM_METAL_PICTURE_SCAN_ARGS}
            --output "${CMAKE_BINARY_DIR}/imm-picture-layers.tsv"
            "${IMM_METAL_PICTURE_SCAN_PATH}"
        DEPENDS ImmPictureScan
        WORKING_DIRECTORY "${CODE_DIR}/.."
        COMMENT "Scanning IMM picture layer metadata"
    )
else()
    add_custom_target(scanImmPictureLayers
        COMMAND ${CMAKE_COMMAND} -E echo
            "Set -DIMM_METAL_PICTURE_SCAN_PATH=/path/to/imm-or-directory to enable importer-backed picture metadata scanning"
        DEPENDS ImmPictureScan
        WORKING_DIRECTORY "${CODE_DIR}/.."
        COMMENT "IMM picture metadata scan is not configured"
    )
endif()

if(IMM_METAL_PICTURE_SCAN_PATH)
    add_test(
        NAME immPictureLayerMetadataScan
        COMMAND ${CMAKE_COMMAND}
            --build "${CMAKE_BINARY_DIR}"
            --target scanImmPictureLayers
            --config Release
    )
    set_tests_properties(immPictureLayerMetadataScan PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        LABELS "macos;metadata;picture"
    )
endif()

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

    add_custom_target(validateAppImmViewerMetalAudio
        COMMAND ${CODE_DIR}/appImmViewer/scripts/validate_metal_standalone.sh
            --audio-contract
            "$<TARGET_FILE:appImmViewerMetal>"
            "${CODE_DIR}/../exampleImmFiles/sample1.imm"
        DEPENDS appImmViewerMetal
        WORKING_DIRECTORY "${CODE_DIR}/.."
        COMMENT "Validating standalone macOS Metal audio decode"
    )

    add_custom_target(validateAppImmViewerMetalLongAudio
        COMMAND ${CMAKE_COMMAND} -E env
            "IMM_METAL_VALIDATE_FRAME=360"
            "IMM_METAL_VALIDATE_MAX_FRAME=420"
            "IMM_METAL_VALIDATE_AUDIO_PROGRESS_THRESHOLD_SEC=3.0"
            ${CODE_DIR}/appImmViewer/scripts/validate_metal_standalone.sh
            --audio-contract
            "$<TARGET_FILE:appImmViewerMetal>"
            "${CODE_DIR}/../exampleImmFiles/sample1.imm"
        DEPENDS appImmViewerMetal
        WORKING_DIRECTORY "${CODE_DIR}/.."
        COMMENT "Validating longer standalone macOS Metal audio playback"
    )

    add_custom_target(validateAppImmViewerMetalInteractiveAudio
        COMMAND ${CMAKE_COMMAND} -E env
            "IMM_METAL_INTERACTIVE_SMOKE_EXIT_AFTER_SECONDS=6.0"
            "IMM_METAL_INTERACTIVE_SMOKE_TIMEOUT_SECONDS=30"
            "IMM_METAL_VALIDATE_VOLUME_CONTROLS=1"
            "IMM_METAL_VALIDATE_PLAYBACK_CONTROLS=1"
            "IMM_METAL_VALIDATE_OPEN_FAILURE_RESTORE=1"
            "IMM_METAL_VALIDATE_RECENT_DOCUMENTS=1"
            ${CODE_DIR}/appImmViewer/scripts/validate_metal_standalone.sh
            --interactive-audio-contract
            "$<TARGET_FILE:appImmViewerMetal>"
            "${CODE_DIR}/../exampleImmFiles/sample1.imm"
        DEPENDS appImmViewerMetal
        WORKING_DIRECTORY "${CODE_DIR}/.."
        COMMENT "Validating normal-run standalone macOS Metal audio playback"
    )

    if(IMM_METAL_AUTHORED_AUDIO_PATH)
        add_custom_target(validateAppImmViewerMetalAuthoredAudio
            COMMAND ${CMAKE_COMMAND} -E env
                "IMM_METAL_VALIDATE_EXPECTED_VALUES=0"
                "IMM_METAL_VALIDATE_MAX_FRAME=180"
                "IMM_METAL_VALIDATE_MIN_NONZERO=1"
                "IMM_METAL_VALIDATE_MIN_DRAWCALLS=1"
                "IMM_METAL_VALIDATE_MIN_PICTURE_DRAWCALLS=0"
                "IMM_METAL_VALIDATE_MIN_PICTURE360_DRAWCALLS=0"
                "IMM_METAL_VALIDATE_MIN_TRIANGLES=1"
                "IMM_METAL_VALIDATE_HELPER_DRAWS=0"
                "IMM_METAL_VALIDATE_AUDIO_EXPECTED_OPUS_DECODED=0"
                "IMM_METAL_VALIDATE_AUDIO_MIN_WAV_ADDED=1"
                "IMM_METAL_VALIDATE_AUDIO_MIN_TOTAL_ADDED=1"
                ${CODE_DIR}/appImmViewer/scripts/validate_metal_standalone.sh
                --audio-contract
                "$<TARGET_FILE:appImmViewerMetal>"
                "${IMM_METAL_AUTHORED_AUDIO_PATH}"
            DEPENDS appImmViewerMetal
            WORKING_DIRECTORY "${CODE_DIR}/.."
            COMMENT "Validating authored IMM audio content with standalone macOS Metal"
        )

        add_custom_target(validateAppImmViewerMetalAuthoredLongAudio
            COMMAND ${CMAKE_COMMAND} -E env
                "IMM_METAL_VALIDATE_EXPECTED_VALUES=0"
                "IMM_METAL_VALIDATE_FRAME=360"
                "IMM_METAL_VALIDATE_MAX_FRAME=420"
                "IMM_METAL_VALIDATE_MIN_NONZERO=1"
                "IMM_METAL_VALIDATE_MIN_DRAWCALLS=1"
                "IMM_METAL_VALIDATE_MIN_PICTURE_DRAWCALLS=0"
                "IMM_METAL_VALIDATE_MIN_PICTURE360_DRAWCALLS=0"
                "IMM_METAL_VALIDATE_MIN_TRIANGLES=1"
                "IMM_METAL_VALIDATE_HELPER_DRAWS=0"
                "IMM_METAL_VALIDATE_AUDIO_EXPECTED_OPUS_DECODED=0"
                "IMM_METAL_VALIDATE_AUDIO_MIN_WAV_ADDED=1"
                "IMM_METAL_VALIDATE_AUDIO_MIN_TOTAL_ADDED=1"
                "IMM_METAL_VALIDATE_AUDIO_PROGRESS_THRESHOLD_SEC=3.0"
                ${CODE_DIR}/appImmViewer/scripts/validate_metal_standalone.sh
                --audio-contract
                "$<TARGET_FILE:appImmViewerMetal>"
                "${IMM_METAL_AUTHORED_AUDIO_PATH}"
            DEPENDS appImmViewerMetal
            WORKING_DIRECTORY "${CODE_DIR}/.."
            COMMENT "Validating longer authored IMM audio playback with standalone macOS Metal"
        )
    else()
        add_custom_target(validateAppImmViewerMetalAuthoredAudio
            COMMAND ${CMAKE_COMMAND} -E echo
                "Set -DIMM_METAL_AUTHORED_AUDIO_PATH=/path/to/authored-audio.imm to enable authored audio validation"
            DEPENDS appImmViewerMetal
            WORKING_DIRECTORY "${CODE_DIR}/.."
            COMMENT "Standalone macOS Metal authored audio validation is not configured"
        )

        add_custom_target(validateAppImmViewerMetalAuthoredLongAudio
            COMMAND ${CMAKE_COMMAND} -E echo
                "Set -DIMM_METAL_AUTHORED_AUDIO_PATH=/path/to/authored-audio.imm to enable authored long-audio validation"
            DEPENDS appImmViewerMetal
            WORKING_DIRECTORY "${CODE_DIR}/.."
            COMMENT "Standalone macOS Metal authored long-audio validation is not configured"
        )
    endif()

    if(IMM_METAL_AUTHORED_2D_PICTURE_PATH)
        add_custom_target(validateAppImmViewerMetalAuthoredPicture2D
            COMMAND ${CMAKE_COMMAND} -E env
                "IMM_METAL_VALIDATE_EXPECTED_VALUES=0"
                "IMM_METAL_VALIDATE_MIN_PICTURE_DRAWCALLS=1"
                "IMM_METAL_VALIDATE_MIN_PICTURE2D_DRAWCALLS=1"
                "IMM_METAL_VALIDATE_MIN_PICTURE360_DRAWCALLS=0"
                "IMM_METAL_VALIDATE_MIN_TRIANGLES=1"
                "IMM_METAL_VALIDATE_CAPTURE_DIR=${CMAKE_BINARY_DIR}/authored-picture2d-captures"
                "IMM_METAL_VALIDATE_CAPTURE_FORMAT=png"
                ${CODE_DIR}/appImmViewer/scripts/validate_metal_standalone.sh
                "$<TARGET_FILE:appImmViewerMetal>"
                "${IMM_METAL_AUTHORED_2D_PICTURE_PATH}"
            DEPENDS appImmViewerMetal
            WORKING_DIRECTORY "${CODE_DIR}/.."
            COMMENT "Validating authored 2D picture IMM content with standalone macOS Metal"
        )
    else()
        add_custom_target(validateAppImmViewerMetalAuthoredPicture2D
            COMMAND ${CMAKE_COMMAND} -E echo
                "Set -DIMM_METAL_AUTHORED_2D_PICTURE_PATH=/path/to/authored-2d-picture.imm to enable authored 2D picture validation"
            DEPENDS appImmViewerMetal
            WORKING_DIRECTORY "${CODE_DIR}/.."
            COMMENT "Standalone macOS Metal authored 2D picture validation is not configured"
        )
    endif()

    if(IMM_METAL_AUTHORED_360_PICTURE_PATH)
        add_custom_target(validateAppImmViewerMetalAuthoredPicture360
            COMMAND ${CMAKE_COMMAND} -E env
                "IMM_METAL_VALIDATE_EXPECTED_VALUES=0"
                "IMM_METAL_VALIDATE_MIN_PICTURE_DRAWCALLS=1"
                "IMM_METAL_VALIDATE_MIN_PICTURE2D_DRAWCALLS=0"
                "IMM_METAL_VALIDATE_MIN_PICTURE360_DRAWCALLS=1"
                "IMM_METAL_VALIDATE_MIN_PICTURE360_EQUIRECT_DRAWCALLS=1"
                "IMM_METAL_VALIDATE_MIN_PICTURE360_CUBEMAP_DRAWCALLS=0"
                "IMM_METAL_VALIDATE_MIN_TRIANGLES=1"
                "IMM_METAL_VALIDATE_CAPTURE_DIR=${CMAKE_BINARY_DIR}/authored-picture360-captures"
                "IMM_METAL_VALIDATE_CAPTURE_FORMAT=png"
                ${CODE_DIR}/appImmViewer/scripts/validate_metal_standalone.sh
                "$<TARGET_FILE:appImmViewerMetal>"
                "${IMM_METAL_AUTHORED_360_PICTURE_PATH}"
            DEPENDS appImmViewerMetal
            WORKING_DIRECTORY "${CODE_DIR}/.."
            COMMENT "Validating authored 360 picture IMM content with standalone macOS Metal"
        )
    else()
        add_custom_target(validateAppImmViewerMetalAuthoredPicture360
            COMMAND ${CMAKE_COMMAND} -E echo
                "Set -DIMM_METAL_AUTHORED_360_PICTURE_PATH=/path/to/authored-360-picture.imm to enable authored 360 picture validation"
            DEPENDS appImmViewerMetal
            WORKING_DIRECTORY "${CODE_DIR}/.."
            COMMENT "Standalone macOS Metal authored 360 picture validation is not configured"
        )
    endif()

    if(IMM_METAL_AUTHORED_360_CUBEMAP_PATH)
        add_custom_target(validateAppImmViewerMetalAuthoredPicture360Cubemap
            COMMAND ${CMAKE_COMMAND} -E env
                "IMM_METAL_VALIDATE_EXPECTED_VALUES=0"
                "IMM_METAL_VALIDATE_MIN_PICTURE_DRAWCALLS=1"
                "IMM_METAL_VALIDATE_MIN_PICTURE2D_DRAWCALLS=0"
                "IMM_METAL_VALIDATE_MIN_PICTURE360_DRAWCALLS=1"
                "IMM_METAL_VALIDATE_MIN_PICTURE360_EQUIRECT_DRAWCALLS=0"
                "IMM_METAL_VALIDATE_MIN_PICTURE360_CUBEMAP_DRAWCALLS=1"
                "IMM_METAL_VALIDATE_MIN_TRIANGLES=1"
                "IMM_METAL_VALIDATE_CAPTURE_DIR=${CMAKE_BINARY_DIR}/authored-picture360-cubemap-captures"
                "IMM_METAL_VALIDATE_CAPTURE_FORMAT=png"
                ${CODE_DIR}/appImmViewer/scripts/validate_metal_standalone.sh
                "$<TARGET_FILE:appImmViewerMetal>"
                "${IMM_METAL_AUTHORED_360_CUBEMAP_PATH}"
            DEPENDS appImmViewerMetal
            WORKING_DIRECTORY "${CODE_DIR}/.."
            COMMENT "Validating authored 360 cubemap picture IMM content with standalone macOS Metal"
        )
    else()
        add_custom_target(validateAppImmViewerMetalAuthoredPicture360Cubemap
            COMMAND ${CMAKE_COMMAND} -E echo
                "Set -DIMM_METAL_AUTHORED_360_CUBEMAP_PATH=/path/to/authored-360-cubemap.imm to enable authored 360 cubemap validation"
            DEPENDS appImmViewerMetal
            WORKING_DIRECTORY "${CODE_DIR}/.."
            COMMENT "Standalone macOS Metal authored 360 cubemap validation is not configured"
        )
    endif()

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

    add_custom_target(validateAppImmViewerMetalReload
        COMMAND ${CODE_DIR}/appImmViewer/scripts/validate_metal_standalone.sh
            --reload-contract
            "$<TARGET_FILE:appImmViewerMetal>"
            "${CODE_DIR}/../exampleImmFiles/sample1.imm"
        DEPENDS appImmViewerMetal
        WORKING_DIRECTORY "${CODE_DIR}/.."
        COMMENT "Validating standalone macOS Metal in-process content reload"
    )

    if(IMM_METAL_CONTENT_SWEEP_PATH)
        add_custom_target(validateAppImmViewerMetalContentSweep
            COMMAND ${CMAKE_COMMAND} -E env
                "IMM_METAL_SWEEP_OUTPUT=${CMAKE_BINARY_DIR}/metal-content-sweep.tsv"
                "IMM_METAL_SWEEP_LOG_DIR=${CMAKE_BINARY_DIR}/metal-content-sweep-logs"
                "IMM_METAL_SWEEP_CAPTURE_DIR=${CMAKE_BINARY_DIR}/metal-content-sweep-captures"
                "IMM_METAL_SWEEP_MAX_FILES=${IMM_METAL_CONTENT_SWEEP_MAX_FILES}"
                "IMM_METAL_SWEEP_MAX_BYTES=${IMM_METAL_CONTENT_SWEEP_MAX_BYTES}"
                "IMM_METAL_SWEEP_MAX_FRAME=${IMM_METAL_CONTENT_SWEEP_MAX_FRAME}"
                "IMM_METAL_SWEEP_MIN_PASSED=${IMM_METAL_CONTENT_SWEEP_MIN_PASSED}"
                "IMM_METAL_SWEEP_FAIL_ON_FAILED=${IMM_METAL_CONTENT_SWEEP_FAIL_ON_FAILED}"
                ${CODE_DIR}/appImmViewer/scripts/validate_metal_content_sweep.sh
                "$<TARGET_FILE:appImmViewerMetal>"
                "${IMM_METAL_CONTENT_SWEEP_PATH}"
            DEPENDS appImmViewerMetal
            WORKING_DIRECTORY "${CODE_DIR}/.."
            COMMENT "Sweeping local IMM content with standalone macOS Metal validation"
        )
    else()
        add_custom_target(validateAppImmViewerMetalContentSweep
            COMMAND ${CMAKE_COMMAND} -E echo
                "Set -DIMM_METAL_CONTENT_SWEEP_PATH=/path/to/imm-or-directory to enable local standalone Metal content sweep validation"
            DEPENDS appImmViewerMetal
            WORKING_DIRECTORY "${CODE_DIR}/.."
            COMMENT "Standalone macOS Metal local content sweep is not configured"
        )
    endif()

    if(IMM_METAL_REFERENCE_CAPTURE_PATH)
        add_custom_target(validateAppImmViewerMetalReferenceCompare
            COMMAND ${CMAKE_COMMAND} -E env
                "IMM_METAL_VALIDATE_CAPTURE_DIR=${CMAKE_BINARY_DIR}/reference-comparison"
                "IMM_METAL_VALIDATE_CAPTURE_FORMAT=png"
                ${CODE_DIR}/appImmViewer/scripts/validate_metal_standalone.sh
                "$<TARGET_FILE:appImmViewerMetal>"
                "${CODE_DIR}/../exampleImmFiles/sample1.imm"
            COMMAND python3
                ${CODE_DIR}/appImmViewer/scripts/compare_captures.py
                "${IMM_METAL_REFERENCE_CAPTURE_PATH}"
                "${CMAKE_BINARY_DIR}/reference-comparison/static.png"
                --json-output "${CMAKE_BINARY_DIR}/reference-comparison/reference-vs-metal-static.json"
                --diff-output "${CMAKE_BINARY_DIR}/reference-comparison/reference-vs-metal-static-diff.png"
                --contact-sheet-output "${CMAKE_BINARY_DIR}/reference-comparison/reference-vs-metal-static-contact-sheet.png"
                ${IMM_METAL_REFERENCE_COMPARE_ARGS}
            DEPENDS appImmViewerMetal
            WORKING_DIRECTORY "${CODE_DIR}/.."
            COMMENT "Comparing standalone macOS Metal sample1 capture against configured reference"
        )
    else()
        add_custom_target(validateAppImmViewerMetalReferenceCompare
            COMMAND ${CMAKE_COMMAND} -E echo
                "Set -DIMM_METAL_REFERENCE_CAPTURE_PATH=/path/to/reference.png and optional -DIMM_METAL_REFERENCE_MAX_* thresholds to enable reference-image comparison"
            DEPENDS appImmViewerMetal
            WORKING_DIRECTORY "${CODE_DIR}/.."
            COMMENT "Standalone macOS Metal reference-image comparison is not configured"
        )
    endif()

    set(IMM_METAL_VALIDATION_TARGETS
        validateAppImmViewerMetalPlayback
        validateAppImmViewerMetalCliContract
        validateAppImmViewerMetalCapture
        validateAppImmViewerMetalAudio
        validateAppImmViewerMetalLongAudio
        validateAppImmViewerMetalInteractiveAudio
        validateAppImmViewerMetalNativeFrameFailure
        validateAppImmViewerMetalBundle
        validateAppImmViewerMetalContentOverride
        validateAppImmViewerMetalRepeat
        validateAppImmViewerMetalReload
    )
    if(IMM_METAL_REFERENCE_CAPTURE_PATH)
        list(APPEND IMM_METAL_VALIDATION_TARGETS validateAppImmViewerMetalReferenceCompare)
    endif()
    if(IMM_METAL_AUTHORED_AUDIO_PATH)
        list(APPEND IMM_METAL_VALIDATION_TARGETS validateAppImmViewerMetalAuthoredAudio validateAppImmViewerMetalAuthoredLongAudio)
    endif()
    if(IMM_METAL_AUTHORED_2D_PICTURE_PATH)
        list(APPEND IMM_METAL_VALIDATION_TARGETS validateAppImmViewerMetalAuthoredPicture2D)
    endif()
    if(IMM_METAL_AUTHORED_360_PICTURE_PATH)
        list(APPEND IMM_METAL_VALIDATION_TARGETS validateAppImmViewerMetalAuthoredPicture360)
    endif()
    if(IMM_METAL_AUTHORED_360_CUBEMAP_PATH)
        list(APPEND IMM_METAL_VALIDATION_TARGETS validateAppImmViewerMetalAuthoredPicture360Cubemap)
    endif()
    if(IMM_METAL_CONTENT_SWEEP_PATH)
        list(APPEND IMM_METAL_VALIDATION_TARGETS validateAppImmViewerMetalContentSweep)
    endif()

    add_custom_target(validateAppImmViewerMetal
        DEPENDS ${IMM_METAL_VALIDATION_TARGETS}
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
        NAME appImmViewerMetalAudio
        COMMAND ${CMAKE_COMMAND}
            --build "${CMAKE_BINARY_DIR}"
            --target validateAppImmViewerMetalAudio
            --config Release
    )
    set_tests_properties(appImmViewerMetalAudio PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        LABELS "macos;metal;standalone;audio"
    )

    add_test(
        NAME appImmViewerMetalLongAudio
        COMMAND ${CMAKE_COMMAND}
            --build "${CMAKE_BINARY_DIR}"
            --target validateAppImmViewerMetalLongAudio
            --config Release
    )
    set_tests_properties(appImmViewerMetalLongAudio PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        LABELS "macos;metal;standalone;audio"
    )

    add_test(
        NAME appImmViewerMetalInteractiveAudio
        COMMAND ${CMAKE_COMMAND}
            --build "${CMAKE_BINARY_DIR}"
            --target validateAppImmViewerMetalInteractiveAudio
            --config Release
    )
    set_tests_properties(appImmViewerMetalInteractiveAudio PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        LABELS "macos;metal;standalone;audio"
    )

    if(IMM_METAL_AUTHORED_AUDIO_PATH)
        add_test(
            NAME appImmViewerMetalAuthoredAudio
            COMMAND ${CMAKE_COMMAND}
                --build "${CMAKE_BINARY_DIR}"
                --target validateAppImmViewerMetalAuthoredAudio
                --config Release
        )
        set_tests_properties(appImmViewerMetalAuthoredAudio PROPERTIES
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            LABELS "macos;metal;standalone;audio"
        )

        add_test(
            NAME appImmViewerMetalAuthoredLongAudio
            COMMAND ${CMAKE_COMMAND}
                --build "${CMAKE_BINARY_DIR}"
                --target validateAppImmViewerMetalAuthoredLongAudio
                --config Release
        )
        set_tests_properties(appImmViewerMetalAuthoredLongAudio PROPERTIES
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            LABELS "macos;metal;standalone;audio"
        )
    endif()

    if(IMM_METAL_AUTHORED_2D_PICTURE_PATH)
        add_test(
            NAME appImmViewerMetalAuthoredPicture2D
            COMMAND ${CMAKE_COMMAND}
                --build "${CMAKE_BINARY_DIR}"
                --target validateAppImmViewerMetalAuthoredPicture2D
                --config Release
        )
        set_tests_properties(appImmViewerMetalAuthoredPicture2D PROPERTIES
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            LABELS "macos;metal;standalone;picture"
        )
    endif()

    if(IMM_METAL_AUTHORED_360_PICTURE_PATH)
        add_test(
            NAME appImmViewerMetalAuthoredPicture360
            COMMAND ${CMAKE_COMMAND}
                --build "${CMAKE_BINARY_DIR}"
                --target validateAppImmViewerMetalAuthoredPicture360
                --config Release
        )
        set_tests_properties(appImmViewerMetalAuthoredPicture360 PROPERTIES
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            LABELS "macos;metal;standalone;picture"
        )
    endif()

    if(IMM_METAL_AUTHORED_360_CUBEMAP_PATH)
        add_test(
            NAME appImmViewerMetalAuthoredPicture360Cubemap
            COMMAND ${CMAKE_COMMAND}
                --build "${CMAKE_BINARY_DIR}"
                --target validateAppImmViewerMetalAuthoredPicture360Cubemap
                --config Release
        )
        set_tests_properties(appImmViewerMetalAuthoredPicture360Cubemap PROPERTIES
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            LABELS "macos;metal;standalone;picture"
        )
    endif()

    if(IMM_METAL_CONTENT_SWEEP_PATH)
        add_test(
            NAME appImmViewerMetalContentSweep
            COMMAND ${CMAKE_COMMAND}
                --build "${CMAKE_BINARY_DIR}"
                --target validateAppImmViewerMetalContentSweep
                --config Release
        )
        set_tests_properties(appImmViewerMetalContentSweep PROPERTIES
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            LABELS "macos;metal;standalone;content"
        )
    endif()

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

    add_test(
        NAME appImmViewerMetalReload
        COMMAND ${CMAKE_COMMAND}
            --build "${CMAKE_BINARY_DIR}"
            --target validateAppImmViewerMetalReload
            --config Release
    )
    set_tests_properties(appImmViewerMetalReload PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        LABELS "macos;metal;standalone"
    )

    if(IMM_METAL_REFERENCE_CAPTURE_PATH)
        add_test(
            NAME appImmViewerMetalReferenceCompare
            COMMAND ${CMAKE_COMMAND}
                --build "${CMAKE_BINARY_DIR}"
                --target validateAppImmViewerMetalReferenceCompare
                --config Release
        )
        set_tests_properties(appImmViewerMetalReferenceCompare PROPERTIES
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            LABELS "macos;metal;standalone;reference"
        )
    endif()
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
