#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ImmCore {

// Host frame numbers, rather than our own queue's fences, govern borrowed draws.
// A slot holds every camera's resources until the host declares that frame safe.
class piVulkanHostFrameSlots
{
public:
    bool Acquire(uint64_t frame, uint64_t safeFrame, size_t &slot, bool &newFrame)
    {
        if (safeFrame > frame || (mSeenFrame && (frame < mLastFrame || safeFrame < mLastSafeFrame)))
            return false;
        mSeenFrame = true;
        mLastFrame = frame;
        mLastSafeFrame = safeFrame;
        for (size_t i = 0; i < mSlots.size(); ++i)
        {
            auto &entry = mSlots[i];
            if (entry.occupied && entry.frame != frame && entry.frame <= safeFrame)
                entry.occupied = false;
            if (entry.occupied && entry.frame == frame)
            {
                slot = i;
                newFrame = false;
                return true;
            }
        }
        slot = 0;
        while (slot < mSlots.size() && mSlots[slot].occupied) ++slot;
        // Do not assume the host has exactly three frames in flight.
        if (slot == mSlots.size()) mSlots.push_back(Slot{});
        mSlots[slot].occupied = true;
        mSlots[slot].frame = frame;
        newFrame = true;
        return true;
    }

    size_t Size() const { return mSlots.size(); }

    // Even safeFrame == frame cannot retire the frame still being recorded.
    bool IsComplete(uint64_t frame) const
    {
        return mSeenFrame && frame < mLastFrame && frame <= mLastSafeFrame;
    }

private:
    struct Slot { uint64_t frame = 0; bool occupied = false; };
    std::vector<Slot> mSlots;
    bool mSeenFrame = false;
    uint64_t mLastFrame = 0, mLastSafeFrame = 0;
};

}
