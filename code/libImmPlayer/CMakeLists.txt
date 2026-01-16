# libImmPlayer - IMM playback/rendering library

set(IMMPLAYER_SOURCES
    src/blue_noise.cpp
    src/document.cpp
    src/layerRenderers/layerRendererModel/layerRendererModel.cpp
    src/layerRenderers/layerRendererPaint/pretessellated/layerRendererPaintPretessellated.cpp
    src/layerRenderers/layerRendererPaint/static/layerRendererPaintStatic.cpp
    src/layerRenderers/layerRendererPicture/layerRendererPicture.cpp
    src/layerRenderers/layerRendererSound/layerRendererSound.cpp
    src/mngrPlayer.cpp
    src/player.cpp
)

# Create static library
add_library(ImmPlayer STATIC ${IMMPLAYER_SOURCES})

# Include directories
target_include_directories(ImmPlayer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/layerRenderers
    ${CMAKE_CURRENT_SOURCE_DIR}/..  # For libImmCore/src/... includes
)

# Generate GLES shader headers for Android builds
if(ANDROID)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    set(IMMPLAYER_GLSL_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set(IMMPLAYER_GLSL_GEN_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/gen_glsl_header.py")

    function(immplayer_gen_glsl_header input_file output_file var_name)
        add_custom_command(
            OUTPUT "${output_file}"
            COMMAND "${Python3_EXECUTABLE}" "${IMMPLAYER_GLSL_GEN_SCRIPT}"
                --input "${input_file}"
                --output "${output_file}"
                --var "${var_name}"
            DEPENDS "${input_file}" "${IMMPLAYER_GLSL_GEN_SCRIPT}"
            COMMENT "Generating ${output_file}"
            VERBATIM
        )
    endfunction()

    set(IMMPLAYER_GLSL_HEADERS
        "${IMMPLAYER_GLSL_GEN_DIR}/shader_model_vs_es.h"
        "${IMMPLAYER_GLSL_GEN_DIR}/shader_model_fs_es.h"
        "${IMMPLAYER_GLSL_GEN_DIR}/shader_pretessellated_brush_vs_es.h"
        "${IMMPLAYER_GLSL_GEN_DIR}/shader_pretessellated_brush_fs_es.h"
    )

    immplayer_gen_glsl_header(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/layerRenderers/layerRendererModel/gles/shader_model_vs.es.glsl"
        "${IMMPLAYER_GLSL_GEN_DIR}/shader_model_vs_es.h"
        "shader_model_vs"
    )
    immplayer_gen_glsl_header(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/layerRenderers/layerRendererModel/gles/shader_model_fs.es.glsl"
        "${IMMPLAYER_GLSL_GEN_DIR}/shader_model_fs_es.h"
        "shader_model_fs"
    )
    immplayer_gen_glsl_header(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/layerRenderers/layerRendererPaint/pretessellated/shader_pretessellated_brush_vs.es.glsl"
        "${IMMPLAYER_GLSL_GEN_DIR}/shader_pretessellated_brush_vs_es.h"
        "shader_pretessellated_brush_vs"
    )
    immplayer_gen_glsl_header(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/layerRenderers/layerRendererPaint/pretessellated/shader_pretessellated_brush_fs.es.glsl"
        "${IMMPLAYER_GLSL_GEN_DIR}/shader_pretessellated_brush_fs_es.h"
        "shader_pretessellated_brush_fs"
    )

    add_custom_target(ImmPlayerGlslGen DEPENDS ${IMMPLAYER_GLSL_HEADERS})
    add_dependencies(ImmPlayer ImmPlayerGlslGen)
    target_include_directories(ImmPlayer PRIVATE "${IMMPLAYER_GLSL_GEN_DIR}")
endif()

# Link to ImmCore and ImmImporter
target_link_libraries(ImmPlayer PUBLIC ImmCore ImmImporter)
