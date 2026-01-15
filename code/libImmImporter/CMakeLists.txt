# libImmImporter - IMM file import library

set(IMMIMPORTER_SOURCES
    src/document/behaviors/keepalive.cpp
    src/document/layer.cpp
    src/document/layerEffect.cpp
    src/document/layerInstance.cpp
    src/document/layerModel3d.cpp
    src/document/layerPaint/drawingPretessellated.cpp
    src/document/layerPaint/drawingStatic.cpp
    src/document/layerPaint/element.cpp
    src/document/layerPaintPretessellated.cpp
    src/document/layerPaintStatic.cpp
    src/document/layerPicture.cpp
    src/document/layerSound.cpp
    src/document/layerSpawnArea.cpp
    src/document/sequence.cpp
    src/fromImmersive/fromImmersive.cpp
    src/fromImmersive/fromImmersiveLayer.cpp
    src/fromImmersive/fromImmersiveLayerModel.cpp
    src/fromImmersive/fromImmersiveLayerPaint.cpp
    src/fromImmersive/fromImmersiveLayerPicture.cpp
    src/fromImmersive/fromImmersiveLayerSound.cpp
    src/fromImmersive/fromImmersiveLayerSpawnArea.cpp
    src/fromImmersive/fromImmersiveUtils.cpp
)

# Create static library
add_library(ImmImporter STATIC ${IMMIMPORTER_SOURCES})

# Include directories
target_include_directories(ImmImporter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/document
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fromImmersive
    ${CMAKE_CURRENT_SOURCE_DIR}/..  # For libImmCore/src/... includes
)

# Link to ImmCore
target_link_libraries(ImmImporter PUBLIC ImmCore)
