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

# Link to ImmCore and ImmImporter
target_link_libraries(ImmPlayer PUBLIC ImmCore ImmImporter)
