# appImmUnity - Unity native plugin for IMM

set(IMMUNITY_SOURCES
    src/main.cpp
)

# Create shared library (DLL on Windows, .so on Android)
add_library(ImmUnityPlugin SHARED ${IMMUNITY_SOURCES})

# Include directories
target_include_directories(ImmUnityPlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Link to IMM libraries
target_link_libraries(ImmUnityPlugin PRIVATE
    ImmPlayer
    ImmImporter
    ImmExporter
    ImmCore
)

# Platform-specific settings
if(WIN32)
    # Windows DLL settings
    target_compile_definitions(ImmUnityPlugin PRIVATE
        WIN32
        _WINDOWS
        _USRDLL
        RENDERINGPLUGIN_EXPORTS
    )
    set_target_properties(ImmUnityPlugin PROPERTIES
        OUTPUT_NAME "ImmUnityPlugin"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/exe"
    )
elseif(ANDROID)
    # Android shared library settings
    target_compile_definitions(ImmUnityPlugin PRIVATE
        ANDROID
        IMM_NULL_AUDIO
    )
    # Link Android-specific libraries
    target_link_libraries(ImmUnityPlugin PRIVATE
        android
        log
        EGL
        GLESv3
    )
endif()
