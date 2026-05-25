// Unity Native Plugin API copyright (c) Unity Technologies.
// Licensed under the Unity Companion License for Unity-dependent projects.

#pragma once

#include "IUnityGraphics.h"

typedef void *UnityMetalBundleRef;
typedef void *UnityMetalDeviceRef;
typedef void *UnityMetalCommandBufferRef;
typedef void *UnityMetalCommandQueueRef;
typedef void *UnityMetalCommandEncoderRef;
typedef void *UnityMetalRenderPassDescriptorRef;
typedef void *UnityMetalTextureRef;

UNITY_DECLARE_INTERFACE(IUnityGraphicsMetalV2)
{
    UnityMetalCommandBufferRef (UNITY_INTERFACE_API * CommitCurrentCommandBuffer)();
    UnityMetalCommandQueueRef (UNITY_INTERFACE_API * CommandQueue)();
    UnityMetalBundleRef (UNITY_INTERFACE_API * MetalBundle)();
    UnityMetalDeviceRef (UNITY_INTERFACE_API * MetalDevice)();
    UnityMetalCommandBufferRef (UNITY_INTERFACE_API * CurrentCommandBuffer)();
    UnityMetalCommandEncoderRef (UNITY_INTERFACE_API * CurrentCommandEncoder)();
    void (UNITY_INTERFACE_API * EndCurrentCommandEncoder)();
    UnityMetalRenderPassDescriptorRef (UNITY_INTERFACE_API * CurrentRenderPassDescriptor)();
    UnityRenderBuffer (UNITY_INTERFACE_API * RenderBufferFromHandle)(void* bufferHandle);
    UnityMetalTextureRef (UNITY_INTERFACE_API * TextureFromRenderBuffer)(UnityRenderBuffer buffer);
    UnityMetalTextureRef (UNITY_INTERFACE_API * AAResolvedTextureFromRenderBuffer)(UnityRenderBuffer buffer);
    UnityMetalTextureRef (UNITY_INTERFACE_API * StencilTextureFromRenderBuffer)(UnityRenderBuffer buffer);
};
UNITY_REGISTER_INTERFACE_GUID(0xF58857784FEF46ECULL, 0x9DB7A8803B87DA3DULL, IUnityGraphicsMetalV2)

UNITY_DECLARE_INTERFACE(IUnityGraphicsMetalV1)
{
    UnityMetalBundleRef (UNITY_INTERFACE_API * MetalBundle)();
    UnityMetalDeviceRef (UNITY_INTERFACE_API * MetalDevice)();
    UnityMetalCommandBufferRef (UNITY_INTERFACE_API * CurrentCommandBuffer)();
    UnityMetalCommandEncoderRef (UNITY_INTERFACE_API * CurrentCommandEncoder)();
    void (UNITY_INTERFACE_API * EndCurrentCommandEncoder)();
    UnityMetalRenderPassDescriptorRef (UNITY_INTERFACE_API * CurrentRenderPassDescriptor)();
    UnityRenderBuffer (UNITY_INTERFACE_API * RenderBufferFromHandle)(void* bufferHandle);
    UnityMetalTextureRef (UNITY_INTERFACE_API * TextureFromRenderBuffer)(UnityRenderBuffer buffer);
    UnityMetalTextureRef (UNITY_INTERFACE_API * AAResolvedTextureFromRenderBuffer)(UnityRenderBuffer buffer);
    UnityMetalTextureRef (UNITY_INTERFACE_API * StencilTextureFromRenderBuffer)(UnityRenderBuffer buffer);
};
UNITY_REGISTER_INTERFACE_GUID(0x29F8F3D03833465EULL, 0x92138551C15D823DULL, IUnityGraphicsMetalV1)

UNITY_DECLARE_INTERFACE(IUnityGraphicsMetal)
{
    UnityMetalBundleRef (UNITY_INTERFACE_API * MetalBundle)();
    UnityMetalDeviceRef (UNITY_INTERFACE_API * MetalDevice)();
    UnityMetalCommandBufferRef (UNITY_INTERFACE_API * CurrentCommandBuffer)();
    UnityMetalCommandEncoderRef (UNITY_INTERFACE_API * CurrentCommandEncoder)();
    void (UNITY_INTERFACE_API * EndCurrentCommandEncoder)();
    UnityMetalRenderPassDescriptorRef (UNITY_INTERFACE_API * CurrentRenderPassDescriptor)();
    UnityRenderBuffer (UNITY_INTERFACE_API * RenderBufferFromHandle)(void* bufferHandle);
    UnityMetalTextureRef (UNITY_INTERFACE_API * TextureFromRenderBuffer)(UnityRenderBuffer buffer);
    UnityMetalTextureRef (UNITY_INTERFACE_API * AAResolvedTextureFromRenderBuffer)(UnityRenderBuffer buffer);
    UnityMetalTextureRef (UNITY_INTERFACE_API * StencilTextureFromRenderBuffer)(UnityRenderBuffer buffer);
};
UNITY_REGISTER_INTERFACE_GUID(0x992C8EAEA95811E5ULL, 0x9A62C4B5B9876117ULL, IUnityGraphicsMetal)
