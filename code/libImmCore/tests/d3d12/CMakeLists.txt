cmake_minimum_required(VERSION 3.20)
project(ImmD3D12SubmissionSmoke LANGUAGES CXX)
if(NOT WIN32)
    message(FATAL_ERROR "The D3D12 submission smoke requires Windows")
endif()

add_executable(imm_d3d12_submission_smoke
    submission_smoke.cpp
    ../../src/libRender/directx12/piDX12_ShaderBindings.cpp
    ../../src/libRender/directx12/piDX12_Renderer.cpp
    ../../src/libRender/directx12/piDX12_CommandContext.cpp)
target_compile_features(imm_d3d12_submission_smoke PRIVATE cxx_std_17)
target_compile_definitions(imm_d3d12_submission_smoke PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_definitions(imm_d3d12_submission_smoke PRIVATE IMM_PICTURE_SHADER_DIRECTORY=L"${CMAKE_CURRENT_SOURCE_DIR}/../../../libImmPlayer/src/layerRenderers/layerRendererPicture/")
target_compile_options(imm_d3d12_submission_smoke PRIVATE /W4 /WX /EHsc)
target_link_libraries(imm_d3d12_submission_smoke PRIVATE d3d12 dxgi d3dcompiler)

enable_testing()
add_test(NAME d3d12_submission_readback COMMAND imm_d3d12_submission_smoke)
set_tests_properties(d3d12_submission_readback PROPERTIES TIMEOUT 60)

# The regular Windows solution build produces these libraries before this check.
option(IMM_D3D12_PLAYER_SMOKE "Build the native player integration check" OFF)
if(IMM_D3D12_PLAYER_SMOKE)
    get_filename_component(IMM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
    add_executable(imm_d3d12_player_smoke player_smoke.cpp
        ../../src/libRender/directx12/piDX12_ShaderBindings.cpp
        ../../src/libRender/directx12/piDX12_Renderer.cpp
        ../../src/libRender/directx12/piDX12_CommandContext.cpp)
    target_compile_features(imm_d3d12_player_smoke PRIVATE cxx_std_17)
    target_compile_definitions(imm_d3d12_player_smoke PRIVATE WINDOWS NOMINMAX WIN32_LEAN_AND_MEAN)
    target_include_directories(imm_d3d12_player_smoke PRIVATE "${IMM_ROOT}/code")
    target_compile_definitions(imm_d3d12_player_smoke PRIVATE IMM_D3D12_SAMPLE_FILE=L"${IMM_ROOT}/exampleImmFiles/sample1.imm")
    set_property(TARGET imm_d3d12_player_smoke PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_link_libraries(imm_d3d12_player_smoke PRIVATE d3d12 dxgi d3dcompiler d3d11 ws2_32 winmm opengl32 delayimp
        "${IMM_ROOT}/code/libImmPlayer/bin/x64/$<CONFIG>/libImmPlayer.lib"
        "${IMM_ROOT}/code/libImmImporter/bin/x64/$<CONFIG>/libImmImporter.lib"
        "${IMM_ROOT}/code/libImmExporter/bin/x64/$<CONFIG>/libImmExporter.lib"
        "${IMM_ROOT}/code/libImmCore/bin/x64/$<CONFIG>/libImmCore.lib")
    foreach(pair IN ITEMS "zlib/zlib" "libjpeg-turbo/jpeg" "libpng/libpng16" "libopusenc/opusenc" "opus/opus" "libogg/ogg" "libvorbis/vorbis" "libvorbis/vorbisenc")
        string(REPLACE "/" ";" parts "${pair}")
        list(GET parts 0 directory)
        list(GET parts 1 library)
        target_link_libraries(imm_d3d12_player_smoke PRIVATE "${IMM_ROOT}/thirdparty/${directory}/lib/${library}.lib")
    endforeach()
    target_link_libraries(imm_d3d12_player_smoke PRIVATE "${IMM_ROOT}/thirdparty/audio360-sdk/Audio360/Windows/x64/Audio360.lib")
    foreach(dll IN ITEMS zlib1 jpeg62 libpng16 ogg vorbis vorbisenc)
        target_link_options(imm_d3d12_player_smoke PRIVATE "/DELAYLOAD:${dll}.dll")
    endforeach()
    foreach(pair IN ITEMS "zlib/zlib1" "libjpeg-turbo/jpeg62" "libpng/libpng16" "libogg/ogg" "libvorbis/vorbis" "libvorbis/vorbisenc")
        string(REPLACE "/" ";" parts "${pair}")
        list(GET parts 0 directory)
        list(GET parts 1 library)
        add_custom_command(TARGET imm_d3d12_player_smoke POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${IMM_ROOT}/thirdparty/${directory}/bin/${library}.dll" "$<TARGET_FILE_DIR:imm_d3d12_player_smoke>")
    endforeach()
    add_test(NAME d3d12_player_document_loading COMMAND imm_d3d12_player_smoke)
    set_tests_properties(d3d12_player_document_loading PROPERTIES TIMEOUT 60)
endif()
