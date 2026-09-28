cmake_minimum_required(VERSION 3.20)
project(ImmD3D12SubmissionSmoke LANGUAGES CXX)
if(NOT WIN32)
    message(FATAL_ERROR "The D3D12 submission smoke requires Windows")
endif()

add_executable(imm_d3d12_submission_smoke
    submission_smoke.cpp
    ../../src/libRender/directx12/piDX12_CommandContext.cpp)
target_compile_features(imm_d3d12_submission_smoke PRIVATE cxx_std_17)
target_compile_definitions(imm_d3d12_submission_smoke PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(imm_d3d12_submission_smoke PRIVATE /W4 /WX /EHsc)
target_link_libraries(imm_d3d12_submission_smoke PRIVATE d3d12 dxgi d3dcompiler)

enable_testing()
add_test(NAME d3d12_submission_readback COMMAND imm_d3d12_submission_smoke)
set_tests_properties(d3d12_submission_readback PROPERTIES TIMEOUT 60)
