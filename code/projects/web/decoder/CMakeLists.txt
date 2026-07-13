cmake_minimum_required(VERSION 3.20)

project(ImmWebDecoder LANGUAGES CXX)

enable_testing()

set(
    IMM_WEB_OUTPUT_DIRECTORY
    "${CMAKE_CURRENT_BINARY_DIR}"
    CACHE PATH
    "Directory for generated Emscripten module and worker assets"
)

add_library(imm_web_decoder STATIC
    src/imm_web_decoder.cpp
)

target_include_directories(imm_web_decoder
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(imm_web_decoder PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(imm_web_decoder PRIVATE /W4 /WX)
else()
    target_compile_options(imm_web_decoder PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(EMSCRIPTEN)
    set(IMM_REPOSITORY_CODE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
    set(IMM_CORE_DIR "${IMM_REPOSITORY_CODE_DIR}/libImmCore")
    set(IMM_IMPORTER_DIR "${IMM_REPOSITORY_CODE_DIR}/libImmImporter")

    add_library(imm_web_scene_decoder STATIC
        src/imm_web_log.cpp
        src/imm_web_file_os.cpp
        src/imm_web_scene.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../appImmStrokeReader/src/strokeStore.cpp

        ${IMM_CORE_DIR}/src/libBasics/piArray.cpp
        ${IMM_CORE_DIR}/src/libBasics/piPool.cpp
        ${IMM_CORE_DIR}/src/libBasics/piString.cpp
        ${IMM_CORE_DIR}/src/libBasics/piTick.cpp
        ${IMM_CORE_DIR}/src/libBasics/piImage.cpp
        ${IMM_CORE_DIR}/src/libBasics/piFile.cpp
        ${IMM_CORE_DIR}/src/libBasics/piStr.cpp
        ${IMM_CORE_DIR}/src/libBasics/piTypes.cpp
        ${IMM_CORE_DIR}/src/libCompression/basic/piBitInterlacer.cpp
        ${IMM_CORE_DIR}/src/libCompression/basic/piPredictors.cpp
        ${IMM_CORE_DIR}/src/libCompression/basic/piQuantize.cpp
        ${IMM_CORE_DIR}/src/libCompression/basic/piTransforms.cpp
        ${IMM_CORE_DIR}/src/libMesh/piMesh.cpp
        ${IMM_CORE_DIR}/src/libWave/piWave.cpp

        ${IMM_IMPORTER_DIR}/src/document/layerPaint/element.cpp
        ${IMM_IMPORTER_DIR}/src/document/layerPaint/drawingPretessellated.cpp
        ${IMM_IMPORTER_DIR}/src/document/layerPaint/drawingStatic.cpp
        ${IMM_IMPORTER_DIR}/src/document/sequence.cpp
        ${IMM_IMPORTER_DIR}/src/document/layer.cpp
        ${IMM_IMPORTER_DIR}/src/document/behaviors/keepalive.cpp
        ${IMM_IMPORTER_DIR}/src/document/layerModel3d.cpp
        ${IMM_IMPORTER_DIR}/src/document/layerPicture.cpp
        ${IMM_IMPORTER_DIR}/src/document/layerSound.cpp
        ${IMM_IMPORTER_DIR}/src/document/layerInstance.cpp
        ${IMM_IMPORTER_DIR}/src/document/layerPaintStatic.cpp
        ${IMM_IMPORTER_DIR}/src/document/layerPaintPretessellated.cpp
        ${IMM_IMPORTER_DIR}/src/document/layerSpawnArea.cpp
        ${IMM_IMPORTER_DIR}/src/fromImmersive/fromImmersive.cpp
        ${IMM_IMPORTER_DIR}/src/fromImmersive/fromImmersiveLayer.cpp
        ${IMM_IMPORTER_DIR}/src/fromImmersive/fromImmersiveLayerPaint.cpp
        ${IMM_IMPORTER_DIR}/src/fromImmersive/fromImmersiveLayerModel.cpp
        ${IMM_IMPORTER_DIR}/src/fromImmersive/fromImmersiveLayerSound.cpp
        ${IMM_IMPORTER_DIR}/src/fromImmersive/fromImmersiveLayerPicture.cpp
        ${IMM_IMPORTER_DIR}/src/fromImmersive/fromImmersiveLayerSpawnArea.cpp
        ${IMM_IMPORTER_DIR}/src/fromImmersive/fromImmersiveUtils.cpp
    )
    target_include_directories(imm_web_scene_decoder PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${IMM_REPOSITORY_CODE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../appImmStrokeReader/src
    )
    target_compile_definitions(imm_web_scene_decoder PRIVATE IMM_WEB_DECODER=1)
    target_compile_features(imm_web_scene_decoder PUBLIC cxx_std_17)
    target_compile_options(imm_web_scene_decoder PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        "SHELL:-sUSE_LIBPNG=1"
        "SHELL:-sUSE_LIBJPEG=1"
    )

    add_executable(imm_web_decoder_wasm
        src/imm_web_wasm_entry.cpp
    )
    target_link_libraries(imm_web_decoder_wasm PRIVATE imm_web_decoder imm_web_scene_decoder)
    target_compile_features(imm_web_decoder_wasm PRIVATE cxx_std_17)
    set_target_properties(imm_web_decoder_wasm PROPERTIES
        OUTPUT_NAME "imm-web-decoder"
        SUFFIX ".mjs"
        RUNTIME_OUTPUT_DIRECTORY "${IMM_WEB_OUTPUT_DIRECTORY}"
    )
    target_link_options(imm_web_decoder_wasm PRIVATE
        "SHELL:-sWASM=1"
        "SHELL:-sMODULARIZE=1"
        "SHELL:-sEXPORT_ES6=1"
        "SHELL:-sENVIRONMENT=web,worker,node"
        "SHELL:-sFILESYSTEM=0"
        "SHELL:-sALLOW_MEMORY_GROWTH=1"
        "SHELL:-sMAXIMUM_MEMORY=4GB"
        "SHELL:-sSTACK_SIZE=4194304"
        "SHELL:-sEXPORTED_FUNCTIONS=['_imm_web_schema_version','_imm_web_inspect','_imm_web_decode_scene','_imm_web_release_scene','_imm_web_get_layer_count','_imm_web_get_background_color','_imm_web_get_layer_info','_imm_web_get_layer_transforms','_imm_web_get_animation_info','_imm_web_get_frame_buffer','_imm_web_get_drawing_count','_imm_web_get_drawing_biggest_stroke','_imm_web_get_stroke_count','_imm_web_get_stroke_info','_imm_web_get_stroke_points','_imm_web_get_stroke_point_times','_imm_web_get_picture_info','_imm_web_get_picture_pixels','_imm_web_get_playback_info','_imm_web_get_timeline_layer_info','_imm_web_get_timeline_layer_transforms','_imm_web_get_animation_key','_imm_web_get_chapter_info','_imm_web_get_keep_alive_info','_malloc','_free']"
        "SHELL:-sEXPORTED_RUNTIME_METHODS=['HEAPU8']"
        "SHELL:-sNO_EXIT_RUNTIME=1"
        "SHELL:-sUSE_ZLIB=1"
        "SHELL:-sUSE_LIBPNG=1"
        "SHELL:-sUSE_LIBJPEG=1"
    )

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/js/imm-web-decoder-worker.mjs
        ${IMM_WEB_OUTPUT_DIRECTORY}/imm-web-decoder-worker.mjs
        COPYONLY
    )
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/js/imm-web-geometry.mjs
        ${IMM_WEB_OUTPUT_DIRECTORY}/imm-web-geometry.mjs
        COPYONLY
    )

    find_program(IMM_WEB_NODE_EXECUTABLE node REQUIRED)
    add_test(
        NAME imm_web_decoder_wasm_smoke
        COMMAND
            ${IMM_WEB_NODE_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/imm_web_decoder_wasm_smoke.mjs
            ${IMM_WEB_OUTPUT_DIRECTORY}/imm-web-decoder-worker.mjs
            ${CMAKE_CURRENT_SOURCE_DIR}/../../../../exampleImmFiles/sample1.imm
    )
    add_test(
        NAME imm_web_geometry_tests
        COMMAND
            ${IMM_WEB_NODE_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/imm_web_geometry_tests.mjs
    )
else()
    add_executable(imm_web_decoder_tests
        tests/imm_web_decoder_tests.cpp
    )
    target_link_libraries(imm_web_decoder_tests PRIVATE imm_web_decoder)
    target_compile_features(imm_web_decoder_tests PRIVATE cxx_std_17)
    add_test(
        NAME imm_web_decoder_tests
        COMMAND imm_web_decoder_tests ${CMAKE_CURRENT_SOURCE_DIR}/../../../../exampleImmFiles/sample1.imm
    )
endif()
