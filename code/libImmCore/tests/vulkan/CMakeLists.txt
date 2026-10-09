cmake_minimum_required(VERSION 3.20)
project(ImmVulkanHostLifetime LANGUAGES CXX)
add_executable(imm_vulkan_host_frame_slots host_frame_slots.cpp)
add_executable(imm_vulkan_render_graph_events render_graph_events.cpp)
add_executable(imm_vulkan_feature_capture feature_capture.cpp)
foreach(target imm_vulkan_host_frame_slots imm_vulkan_render_graph_events imm_vulkan_feature_capture)
    target_compile_features(${target} PRIVATE cxx_std_11)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /EHsc)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
    endif()
endforeach()
enable_testing()
add_test(NAME vulkan_host_frame_lifetime COMMAND imm_vulkan_host_frame_slots)
add_test(NAME vulkan_host_shutdown_contract COMMAND imm_vulkan_render_graph_events)
add_test(NAME vulkan_logical_device_feature_capture COMMAND imm_vulkan_feature_capture)

# Native player libraries from the regular Windows solution supply the model layer.
option(IMM_VULKAN_MODEL_SMOKE "Build the production Vulkan model readback check" OFF)
if(IMM_VULKAN_MODEL_SMOKE)
    if(NOT WIN32)
        message(FATAL_ERROR "The Vulkan model readback target currently requires Windows player libraries")
    endif()
    get_filename_component(IMM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
    find_package(Vulkan REQUIRED)
    add_executable(imm_vulkan_model_smoke model_smoke.cpp multiview_smoke.cpp
        ../../../appImmShared/src/imm_engine_bridge.cpp
        ../../src/libRender/vulkan/piVulkan_Renderer.cpp
        ../../src/libRender/directx12/piDX12_ShaderBindings.cpp
        ../../src/libRender/directx12/piDX12_Renderer.cpp
        ../../src/libRender/directx12/piDX12_CommandContext.cpp)
    target_compile_features(imm_vulkan_model_smoke PRIVATE cxx_std_17)
    target_compile_definitions(imm_vulkan_model_smoke PRIVATE WINDOWS NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(imm_vulkan_model_smoke PRIVATE IMM_VULKAN_SAMPLE_FILE=L"${IMM_ROOT}/exampleImmFiles/sample1.imm")
    target_include_directories(imm_vulkan_model_smoke PRIVATE "${IMM_ROOT}/code")
    set_property(TARGET imm_vulkan_model_smoke PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_link_libraries(imm_vulkan_model_smoke PRIVATE Vulkan::Vulkan d3d12 dxgi d3dcompiler d3d11 ws2_32 winmm opengl32 delayimp
        "${IMM_ROOT}/code/libImmPlayer/bin/x64/$<CONFIG>/libImmPlayer.lib"
        "${IMM_ROOT}/code/libImmImporter/bin/x64/$<CONFIG>/libImmImporter.lib"
        "${IMM_ROOT}/code/libImmExporter/bin/x64/$<CONFIG>/libImmExporter.lib"
        "${IMM_ROOT}/code/libImmCore/bin/x64/$<CONFIG>/libImmCore.lib")
    foreach(pair IN ITEMS "zlib/zlib" "libjpeg-turbo/jpeg" "libpng/libpng16" "libopusenc/opusenc" "opus/opus" "libogg/ogg" "libvorbis/vorbis" "libvorbis/vorbisenc")
        string(REPLACE "/" ";" parts "${pair}")
        list(GET parts 0 directory)
        list(GET parts 1 library)
        target_link_libraries(imm_vulkan_model_smoke PRIVATE "${IMM_ROOT}/thirdparty/${directory}/lib/${library}.lib")
    endforeach()
    target_link_libraries(imm_vulkan_model_smoke PRIVATE "${IMM_ROOT}/thirdparty/audio360-sdk/Audio360/Windows/x64/Audio360.lib")
    foreach(dll IN ITEMS zlib1 jpeg62 libpng16 ogg vorbis vorbisenc Audio360)
        target_link_options(imm_vulkan_model_smoke PRIVATE "/DELAYLOAD:${dll}.dll")
    endforeach()
    foreach(pair IN ITEMS "zlib/zlib1" "libjpeg-turbo/jpeg62" "libpng/libpng16" "libogg/ogg" "libvorbis/vorbis" "libvorbis/vorbisenc")
        string(REPLACE "/" ";" parts "${pair}")
        list(GET parts 0 directory)
        list(GET parts 1 library)
        add_custom_command(TARGET imm_vulkan_model_smoke POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${IMM_ROOT}/thirdparty/${directory}/bin/${library}.dll" "$<TARGET_FILE_DIR:imm_vulkan_model_smoke>")
    endforeach()
    add_test(NAME vulkan_model_readback COMMAND imm_vulkan_model_smoke)
    set_tests_properties(vulkan_model_readback PROPERTIES TIMEOUT 60)
    add_test(NAME vulkan_model_document_readback COMMAND imm_vulkan_model_smoke)
    set_tests_properties(vulkan_model_document_readback PROPERTIES TIMEOUT 60
        ENVIRONMENT "IMM_VULKAN_SAMPLE_FILE=${IMM_ROOT}/exampleImmFiles/urp-content/model-unlit.imm")
    # Offline IMM authoring utility. Reuse the native probe's library/toolchain
    # configuration and DLL output directory; generation never initializes a GPU.
    add_executable(imm_urp_content_fixtures content_fixtures.cpp)
    foreach(property IN ITEMS COMPILE_FEATURES COMPILE_DEFINITIONS INCLUDE_DIRECTORIES MSVC_RUNTIME_LIBRARY LINK_LIBRARIES LINK_OPTIONS)
        get_target_property(fixture_property imm_vulkan_model_smoke ${property})
        set_property(TARGET imm_urp_content_fixtures PROPERTY ${property} "${fixture_property}")
    endforeach()
    add_dependencies(imm_urp_content_fixtures imm_vulkan_model_smoke)
    add_test(NAME urp_content_fixture_roundtrip COMMAND imm_urp_content_fixtures
        "${CMAKE_CURRENT_BINARY_DIR}/generated-content-fixtures" "${IMM_ROOT}/exampleImmFiles/urp-content")
    set_tests_properties(urp_content_fixture_roundtrip PROPERTIES TIMEOUT 30)
    foreach(pair IN ITEMS "opus/opus" "libopusenc/opusenc")
        string(REPLACE "/" ";" parts "${pair}")
        list(GET parts 0 directory)
        list(GET parts 1 library)
        add_custom_command(TARGET imm_urp_content_fixtures POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${IMM_ROOT}/thirdparty/${directory}/bin/${library}.dll" "$<TARGET_FILE_DIR:imm_urp_content_fixtures>")
    endforeach()
endif()
