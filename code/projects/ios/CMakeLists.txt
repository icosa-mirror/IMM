cmake_minimum_required(VERSION 3.5)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(imm_ios LANGUAGES C CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_SYSTEM_PROCESSOR "arm64" CACHE STRING "" FORCE)
set(CMAKE_MACOSX_BUNDLE OFF CACHE BOOL "" FORCE)
set(CMAKE_INSTALL_DOCDIR "share/doc" CACHE STRING "" FORCE)
set(CMAKE_INSTALL_MANDIR "share/man" CACHE STRING "" FORCE)

if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "" FORCE)
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
set(ENABLE_SHARED OFF CACHE BOOL "" FORCE)
set(ENABLE_EXECUTABLES OFF CACHE BOOL "" FORCE)
set(BUILD_EXECUTABLES OFF CACHE BOOL "" FORCE)
set(WITH_TOOLS OFF CACHE BOOL "" FORCE)
set(WITH_UTILS OFF CACHE BOOL "" FORCE)
set(WITH_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${THIRDPARTY_DIR}/libjpeg-turbo/libjpeg-turbo ${CMAKE_BINARY_DIR}/libjpeg-turbo-build)

set(IMMCORE_UNITY_SOURCES
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
    ${CODE_DIR}/libImmCore/src/libMesh/piRenderMesh.cpp
    ${CODE_DIR}/libImmCore/src/libRender/piRenderer.cpp
    ${CODE_DIR}/libImmCore/src/libRender/metal/piMetal_Renderer.mm
    ${CODE_DIR}/libImmCore/src/libWave/piWave.cpp
    ${CODE_DIR}/libImmCore/src/libWave/formats/piWaveWAV.cpp
    ${CODE_DIR}/libImmCore/src/libWave/formats/piWaveOGG.cpp
    ${CODE_DIR}/libImmCore/src/libSound/piSound.cpp
    ${CODE_DIR}/libImmCore/src/libSound/piSoundEngineNULL.cpp
)

add_library(libImmCoreUnity STATIC ${IMMCORE_UNITY_SOURCES})
target_compile_definitions(libImmCoreUnity PUBLIC IMM_IOS=1)
target_include_directories(libImmCoreUnity
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
target_link_libraries(libImmCoreUnity
    PUBLIC
        ogg
        vorbis
        png_static
        turbojpeg-static
        ZLIB::ZLIB
        "-framework Metal"
        "-framework QuartzCore"
        "-framework Foundation"
)

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

add_library(libImmImporterMinimal STATIC ${IMMIMPORTER_SOURCES})
target_include_directories(libImmImporterMinimal PUBLIC ${CODE_DIR})
target_link_libraries(libImmImporterMinimal PUBLIC libImmCoreMinimal)

add_library(libImmImporterUnity STATIC ${IMMIMPORTER_SOURCES})
target_include_directories(libImmImporterUnity PUBLIC ${CODE_DIR})
target_link_libraries(libImmImporterUnity PUBLIC libImmCoreUnity)

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

add_library(libImmPlayerUnity STATIC ${IMMPLAYER_SOURCES})
target_compile_definitions(libImmPlayerUnity PUBLIC IMM_IOS=1)
target_include_directories(libImmPlayerUnity PUBLIC ${CODE_DIR})
target_link_libraries(libImmPlayerUnity PUBLIC libImmImporterUnity libImmCoreUnity)

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
add_dependencies(libImmPlayerUnity ShadersModel ShadersPretessellated)

set_source_files_properties(${CODE_DIR}/appImmUnity/src/main.cpp PROPERTIES LANGUAGE OBJCXX)
add_library(ImmUnity STATIC ${CODE_DIR}/appImmUnity/src/main.cpp)
target_compile_definitions(ImmUnity PUBLIC IMM_IOS=1)
target_include_directories(ImmUnity PUBLIC ${CODE_DIR})
target_link_libraries(ImmUnity PRIVATE libImmPlayerUnity libImmImporterUnity libImmCoreUnity)
set_target_properties(ImmUnity PROPERTIES
    OUTPUT_NAME "ImmUnityPlugin"
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/unity-objects
)

set(IMMUNITY_UPM_IOS_DIR ${CODE_DIR}/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/iOS)
add_custom_command(TARGET ImmUnity POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/unity"
    COMMAND xcrun libtool -static -o "${CMAKE_BINARY_DIR}/unity/libImmUnityPlugin.a"
        "$<TARGET_FILE:ImmUnity>"
        "$<TARGET_FILE:libImmPlayerUnity>"
        "$<TARGET_FILE:libImmImporterUnity>"
        "$<TARGET_FILE:libImmCoreUnity>"
        "$<TARGET_FILE:ogg>"
        "$<TARGET_FILE:vorbis>"
        "$<TARGET_FILE:png_static>"
        "$<TARGET_FILE:turbojpeg-static>"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${IMMUNITY_UPM_IOS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/unity/libImmUnityPlugin.a" "${IMMUNITY_UPM_IOS_DIR}/libImmUnityPlugin.a"
)

file(WRITE "${CMAKE_BINARY_DIR}/unity_link_smoke.cpp"
    "extern \"C\" void UnityPluginLoad(void *);\n"
    "extern \"C\" int Init(int, int, char *, char *);\n"
    "int main() { UnityPluginLoad((void*)0); return Init(0, 0, (char*)0, (char*)0); }\n"
)
add_executable(ImmUnityIOSLinkSmoke EXCLUDE_FROM_ALL "${CMAKE_BINARY_DIR}/unity_link_smoke.cpp")
add_dependencies(ImmUnityIOSLinkSmoke ImmUnity)
target_link_options(ImmUnityIOSLinkSmoke PRIVATE "-Wl,-force_load,${CMAKE_BINARY_DIR}/unity/libImmUnityPlugin.a")
target_link_libraries(ImmUnityIOSLinkSmoke PRIVATE
    "-framework Metal"
    "-framework QuartzCore"
    "-framework Foundation"
    ZLIB::ZLIB
)
set_target_properties(ImmUnityIOSLinkSmoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/unity
)

set(IMMSTROKEREADER_SOURCES
    ${CODE_DIR}/appImmStrokeReader/src/main.cpp
    ${CODE_DIR}/appImmStrokeReader/src/strokeStore.cpp
)

add_library(ImmStrokeReader STATIC ${IMMSTROKEREADER_SOURCES})
target_include_directories(ImmStrokeReader PUBLIC ${CODE_DIR})
target_include_directories(ImmStrokeReader PRIVATE ${CODE_DIR}/libImmCore/src/libBasics)
target_link_libraries(ImmStrokeReader PRIVATE libImmImporterMinimal libImmCoreMinimal)
set_target_properties(ImmStrokeReader PROPERTIES
    OUTPUT_NAME "ImmStrokeReader"
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/stroke-reader
)

set(IMMSTROKEREADER_UPM_IOS_DIR ${CODE_DIR}/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/iOS)
add_custom_command(TARGET ImmStrokeReader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${IMMSTROKEREADER_UPM_IOS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:ImmStrokeReader>" "${IMMSTROKEREADER_UPM_IOS_DIR}/$<TARGET_FILE_NAME:ImmStrokeReader>"
)
