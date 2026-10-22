#pragma once

#include <cstdint>
#include <mutex>

namespace ImmPlayer
{
    // Numeric results of one completed render. Unlike PerformanceInfo this owns
    // no strings, so publication neither allocates nor copies piString storage.
    struct RenderPerformanceSnapshot
    {
        int cpuLoadTimeMS = 0;
        int numDrawCalls = 0;
        int numDrawCallsCulled = 0;
        int numPaintDrawCalls = 0;
        int numPictureDrawCalls = 0;
        int numPicture2DDrawCalls = 0;
        int numPicture360DrawCalls = 0;
        int numPicture360EquirectDrawCalls = 0;
        int numPicture360CubemapDrawCalls = 0;
        int numModelDrawCalls = 0;
        int numTriangles = 0;
        int numTrianglesCulled = 0;
        uint64_t validationTimeFrame = 0;
        float gpuTimeAverageMs = 0.0f;
    };

    class RenderPerformanceSnapshotStore
    {
    public:
        void Publish(const RenderPerformanceSnapshot & snapshot)
        {
            std::lock_guard<std::mutex> guard(mMutex);
            if (mEnabled)
                mCompleted = snapshot;
        }

        void Clear() { Publish(RenderPerformanceSnapshot{}); }

        void SetEnabled(bool enabled)
        {
            std::lock_guard<std::mutex> guard(mMutex);
            mEnabled = enabled;
            if (!enabled)
                mCompleted = RenderPerformanceSnapshot{};
        }

        RenderPerformanceSnapshot Get() const
        {
            std::lock_guard<std::mutex> guard(mMutex);
            return mCompleted;
        }

    private:
        mutable std::mutex mMutex;
        RenderPerformanceSnapshot mCompleted;
        bool mEnabled = true;
    };
}
