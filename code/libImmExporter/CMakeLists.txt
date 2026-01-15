# libImmExporter - IMM file export library

set(IMMEXPORTER_SOURCES
    src/document/behaviors/keepalive.cpp
    src/document/layer.cpp
    src/document/layerModel3d.cpp
    src/document/layerPaint/drawing.cpp
    src/document/layerPaint/element.cpp
    src/document/layerPaint.cpp
    src/document/layerPicture.cpp
    src/document/layerSound.cpp
    src/document/layerSpawnArea.cpp
    src/document/sequence.cpp
    src/toImmersive/toImmersive.cpp
    src/toImmersive/toImmersiveLayer.cpp
    src/toImmersive/toImmersiveLayerModel.cpp
    src/toImmersive/toImmersiveLayerPaint.cpp
    src/toImmersive/toImmersiveLayerPicture.cpp
    src/toImmersive/toImmersiveLayerSound.cpp
    src/toImmersive/toImmersiveLayerSpawnArea.cpp
    src/toImmersive/toImmersiveUtils.cpp
)

# Create static library
add_library(ImmExporter STATIC ${IMMEXPORTER_SOURCES})

# Include directories
target_include_directories(ImmExporter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/document
    ${CMAKE_CURRENT_SOURCE_DIR}/src/toImmersive
    ${CMAKE_CURRENT_SOURCE_DIR}/..  # For libImmCore/src/... includes
)

# Link to ImmCore
target_link_libraries(ImmExporter PUBLIC ImmCore)
