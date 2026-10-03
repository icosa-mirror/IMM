#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

// ABI v2, 64-bit targets. Caller owns this immutable request until completed becomes
// 1 (read with acquire semantics). Native code writes gpuCompletion/result/completed.
// Targets must be bound before the event; viewCount selects mono or two-slice stereo.
// Matrices use the native row-major array convention. GPU completion is a renderer
// fence token, not permission for the caller to signal Unity or renderer fences.
struct alignas(8) ImmRenderGraphPacket
{
    uint32_t version = 2, size = sizeof(ImmRenderGraphPacket);
    // 0 initialize, 1 render with Unity buffers, 2 shutdown, 3 maintenance,
    // 4 Metal render with a Unity colour buffer and an explicitly borrowed MTLTexture depth.
    uint32_t operation = 0, viewCount = 1;
    uint64_t sequence = 0;
    int32_t camera = 0, colorSpace = 0, samples = 8, enableSound = 1;
    uint64_t colorBuffer = 0, depthBuffer = 0;
    uint32_t colorFormat = 0, depthFormat = 0;
    int32_t x = 0, y = 0, width = 0, height = 0;
    float worldToView[16] = {}, projection[16] = {};
    float leftView[16] = {}, leftProjection[16] = {};
    float rightView[16] = {}, rightProjection[16] = {};
    uint64_t gpuCompletion = 0;
    int32_t result = 0;
    std::atomic<int32_t> completed{0};
};
static_assert(sizeof(ImmRenderGraphPacket) == 480, "RenderGraph packet ABI size");
static_assert(offsetof(ImmRenderGraphPacket, worldToView) == 80, "RenderGraph matrix ABI offset");
static_assert(offsetof(ImmRenderGraphPacket, leftView) == 208, "RenderGraph left-eye ABI offset");
static_assert(offsetof(ImmRenderGraphPacket, rightView) == 336, "RenderGraph right-eye ABI offset");
static_assert(offsetof(ImmRenderGraphPacket, gpuCompletion) == 464, "RenderGraph GPU completion ABI offset");
static_assert(offsetof(ImmRenderGraphPacket, result) == 472, "RenderGraph result ABI offset");
static_assert(offsetof(ImmRenderGraphPacket, completed) == 476, "RenderGraph completion ABI offset");
static_assert(std::is_standard_layout<ImmRenderGraphPacket>::value, "RenderGraph packet must have standard layout");
static_assert(ATOMIC_INT_LOCK_FREE == 2 && std::is_same<int32_t, int>::value, "RenderGraph completion must be lock-free");
constexpr int ImmRenderGraphEventId = 0x494d4d;

