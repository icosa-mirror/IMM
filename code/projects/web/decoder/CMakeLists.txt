cmake_minimum_required(VERSION 3.20)

project(ImmWebDecoder LANGUAGES CXX)

enable_testing()

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
    add_executable(imm_web_decoder_wasm
        src/imm_web_wasm_entry.cpp
    )
    target_link_libraries(imm_web_decoder_wasm PRIVATE imm_web_decoder)
    target_compile_features(imm_web_decoder_wasm PRIVATE cxx_std_17)
    set_target_properties(imm_web_decoder_wasm PROPERTIES
        OUTPUT_NAME "imm-web-decoder"
        SUFFIX ".mjs"
    )
    target_link_options(imm_web_decoder_wasm PRIVATE
        "SHELL:-sWASM=1"
        "SHELL:-sMODULARIZE=1"
        "SHELL:-sEXPORT_ES6=1"
        "SHELL:-sENVIRONMENT=web,worker,node"
        "SHELL:-sFILESYSTEM=0"
        "SHELL:-sALLOW_MEMORY_GROWTH=1"
        "SHELL:-sEXPORTED_FUNCTIONS=['_imm_web_schema_version','_imm_web_inspect','_malloc','_free']"
        "SHELL:-sEXPORTED_RUNTIME_METHODS=['HEAPU8']"
        "SHELL:-sNO_EXIT_RUNTIME=1"
    )

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/js/imm-web-decoder-worker.mjs
        ${CMAKE_CURRENT_BINARY_DIR}/imm-web-decoder-worker.mjs
        COPYONLY
    )

    find_program(IMM_WEB_NODE_EXECUTABLE node REQUIRED)
    add_test(
        NAME imm_web_decoder_wasm_smoke
        COMMAND
            ${IMM_WEB_NODE_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/imm_web_decoder_wasm_smoke.mjs
            ${CMAKE_CURRENT_BINARY_DIR}/imm-web-decoder-worker.mjs
            ${CMAKE_CURRENT_SOURCE_DIR}/../../../../exampleImmFiles/sample1.imm
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
