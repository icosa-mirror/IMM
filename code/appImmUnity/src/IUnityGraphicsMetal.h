// Unity Native Plugin API copyright (c) Unity Technologies.
// Licensed under the Unity Companion License for Unity-dependent projects.

#pragma once

#include "IUnityGraphics.h"

#if defined(__OBJC__)
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#else
typedef void NSBundle;
typedef void MTLRenderPassDescriptor;
typedef void *id;
#endif

typedef void* UnityRenderBuffer;
typedef void* UnityTextureID;

UNITY_DECLARE_INTERFACE(IUnityGraphicsMetalV1)
{
    NSBundle* (UNITY_INTERFACE_API * MetalBundle)();
    id<MTLDevice> (UNITY_INTERFACE_API * MetalDevice)();
    id<MTLCommandBuffer> (UNITY_INTERFACE_API * CurrentCommandBuffer)();
    id<MTLCommandEncoder> (UNITY_INTERFACE_API * CurrentCommandEncoder)();
    void (UNITY_INTERFACE_API * EndCurrentCommandEncoder)();
    MTLRenderPassDescriptor* (UNITY_INTERFACE_API * CurrentRenderPassDescriptor)();
    UnityRenderBuffer (UNITY_INTERFACE_API * RenderBufferFromHandle)(void* bufferHandle);
    id<MTLTexture> (UNITY_INTERFACE_API * TextureFromRenderBuffer)(UnityRenderBuffer buffer);
    id<MTLTexture> (UNITY_INTERFACE_API * AAResolvedTextureFromRenderBuffer)(UnityRenderBuffer buffer);
    id<MTLTexture> (UNITY_INTERFACE_API * StencilTextureFromRenderBuffer)(UnityRenderBuffer buffer);
};
UNITY_REGISTER_INTERFACE_GUID(0x29F8F3D03833465EULL, 0x92138551C15D823DULL, IUnityGraphicsMetalV1)

UNITY_DECLARE_INTERFACE(IUnityGraphicsMetal)
{
    NSBundle* (UNITY_INTERFACE_API * MetalBundle)();
    id<MTLDevice> (UNITY_INTERFACE_API * MetalDevice)();
    id<MTLCommandBuffer> (UNITY_INTERFACE_API * CurrentCommandBuffer)();
    id<MTLCommandEncoder> (UNITY_INTERFACE_API * CurrentCommandEncoder)();
    void (UNITY_INTERFACE_API * EndCurrentCommandEncoder)();
    MTLRenderPassDescriptor* (UNITY_INTERFACE_API * CurrentRenderPassDescriptor)();
    UnityRenderBuffer (UNITY_INTERFACE_API * RenderBufferFromHandle)(void* bufferHandle);
    id<MTLTexture> (UNITY_INTERFACE_API * TextureFromRenderBuffer)(UnityRenderBuffer buffer);
    id<MTLTexture> (UNITY_INTERFACE_API * AAResolvedTextureFromRenderBuffer)(UnityRenderBuffer buffer);
    id<MTLTexture> (UNITY_INTERFACE_API * StencilTextureFromRenderBuffer)(UnityRenderBuffer buffer);
};
UNITY_REGISTER_INTERFACE_GUID(0x992C8EAEA95811E5ULL, 0x9A62C4B5B9876117ULL, IUnityGraphicsMetal)
