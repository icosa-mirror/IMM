# Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

cmake_minimum_required(VERSION 3.7)

file(GLOB_RECURSE _srcs
  Src/GL/*
  Src/Kernel/*
  Src/Tracing/*.h
  Src/Tracing/LibOVREvents.*
  Src/Util/*.cpp
  Src/Util/*.h
  Src/Util/Shaders/*.hlsl
)

# Create a directory for our shaders to be compiled into
# Ideally this would be created automatically...
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Shaders)

set_source_files_properties("Src/Util/Shaders/Blt_ps.hlsl"     PROPERTIES VS_SHADER_TYPE Pixel  VS_SHADER_MODEL 4.0 VS_SHADER_ENTRYPOINT main VS_SHADER_FLAGS "/Vn Blt_ps /Fh \"$(OutDir)../Shaders/Blt_ps.h\"")
set_source_files_properties("Src/Util/Shaders/Blt_ps_ms2.hlsl" PROPERTIES VS_SHADER_TYPE Pixel  VS_SHADER_MODEL 5.0 VS_SHADER_ENTRYPOINT main VS_SHADER_FLAGS "/Vn Blt_ps_ms2 /Fh \"$(OutDir)../Shaders/Blt_ps_ms2.h\"")
set_source_files_properties("Src/Util/Shaders/GrayBlt_ps.hlsl" PROPERTIES VS_SHADER_TYPE Pixel  VS_SHADER_MODEL 4.0 VS_SHADER_ENTRYPOINT main VS_SHADER_FLAGS "/Vn GrayBlt_ps /Fh \"$(OutDir)../Shaders/GrayBlt_ps.h\"")
set_source_files_properties("Src/Util/Shaders/Blt_vs.hlsl"     PROPERTIES VS_SHADER_TYPE Vertex VS_SHADER_MODEL 4.0 VS_SHADER_ENTRYPOINT main VS_SHADER_FLAGS "/Vn Blt_vs /Fh \"$(OutDir)../Shaders/Blt_vs.h\"")

add_library(LibOVRKernel STATIC ${_srcs})

target_link_libraries(LibOVRKernel
  PUBLIC
    LibOVR
    PC_SDK_Logging
)

target_include_directories(LibOVRKernel PUBLIC Src)
target_include_directories(LibOVRKernel PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
