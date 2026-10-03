#include "../../src/libRender/vulkan/piVulkan_HostFrameSlots.h"
#include <cstdio>
#include <map>
#include <stdexcept>

static void Require(bool valid, const char *message)
{
    if (!valid) throw std::runtime_error(message);
}

int main()
{
    ImmCore::piVulkanHostFrameSlots frames;
    std::map<size_t, uint64_t> pending;
    size_t slot = 0;
    bool newFrame = false;
    // Deliberately keep more than three frames pending. Each queued camera in
    // the same frame must append to that frame rather than reset another frame.
    for (uint64_t frame = 1; frame <= 12; ++frame)
    {
        Require(frames.Acquire(frame, 0, slot, newFrame) && newFrame, "Cannot acquire delayed frame");
        Require(pending.count(slot) == 0, "Reused resources before host GPU completion");
        pending[slot] = frame;
        size_t secondCamera = 0;
        Require(frames.Acquire(frame, 0, secondCamera, newFrame) && !newFrame && secondCamera == slot,
                "Second camera must retain the frame's resource owner");
    }
    Require(frames.Size() == 12, "Host latency was silently limited to three frames");
    Require(frames.Acquire(13, 5, slot, newFrame) && newFrame, "Cannot retire completed host frames");
    Require(pending.at(slot) <= 5, "Reused a frame newer than the safe frame");
    pending[slot] = 13;
    Require(frames.Acquire(14, 5, slot, newFrame) && newFrame, "Cannot retain another queued frame");
    Require(pending.at(slot) <= 5, "Overwrote a frame that remains queued");
    Require(!frames.Acquire(14, 15, slot, newFrame), "Accepted a future safe frame");
    Require(!frames.Acquire(13, 5, slot, newFrame), "Accepted a regressing recording frame");
    Require(!frames.Acquire(14, 4, slot, newFrame), "Accepted a regressing completion frame");
    ImmCore::piVulkanHostFrameSlots zero;
    Require(zero.Acquire(0, 0, slot, newFrame) && newFrame, "Frame zero must be usable");
    Require(zero.Acquire(0, 0, slot, newFrame) && !newFrame, "Reclaimed the currently recording frame");
    std::puts("IMM_VULKAN_HOST_LIFETIME PASS queued cameras, delayed completion, safe reuse and invalid counters");
}
