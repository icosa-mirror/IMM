#pragma once

// Opt-in end-to-end sampling of the normal viewer loop. This records evidence;
// thresholds belong to the validation tools, never to the renderer.
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <string>
#include <vector>

namespace ExePlayer
{
class FrameSample
{
    using Clock = std::chrono::steady_clock;
    std::string mOutput;
    Clock::time_point mStart = Clock::now();
    Clock::time_point mReady;
    Clock::time_point mSampleStart;
    Clock::time_point mPreviousRender;
    double mWarmup = 2.0;
    double mRequested = 5.0;
    double mObserved = 0.0;
    bool mReadySeen = false;
    bool mSampling = false;
    bool mFinished = false;
    int mEngineFrames = 0;
    int mRenderedFrames = 0;
    int mWidth = 0;
    int mHeight = 0;
    std::string mApi;
    std::vector<double> mIntervals;

    static double Seconds(Clock::time_point later, Clock::time_point earlier)
    {
        return std::chrono::duration<double>(later - earlier).count();
    }

    static double Duration(const char* name, double fallback)
    {
        const char* value = std::getenv(name);
        if (!value) return fallback;
        char* end = nullptr;
        const double parsed = std::strtod(value, &end);
        return end != value && *end == '\0' && std::isfinite(parsed) &&
            parsed > 0.0 && parsed <= 30.0 ? parsed : -1.0;
    }

    void Finish(const char* error)
    {
        if (mFinished || mOutput.empty()) return;
        mFinished = true;
        std::ofstream out(mOutput);
        out.imbue(std::locale::classic());
        out << std::setprecision(12);
        out << "{\n  \"schema\":\"imm-frame-sample-v1\",\n"
            << "  \"product\":\"standalone\",\n"
            << "  \"graphics_api\":\"" << mApi << "\",\n"
            << "  \"width\":" << mWidth << ",\"height\":" << mHeight << ",\n"
            << "  \"warmup_seconds\":" << mWarmup << ",\n"
            << "  \"requested_seconds\":" << mRequested << ",\n"
            << "  \"observed_seconds\":" << mObserved << ",\n"
            << "  \"engine_frame_count\":" << mEngineFrames << ",\n"
            << "  \"imm_rendered_frame_count\":" << mRenderedFrames << ",\n"
            << "  \"intervals_ms\":[";
        for (size_t i = 0; i < mIntervals.size(); ++i)
        {
            if (i) out << ',';
            out << mIntervals[i];
        }
        out << "],\n  \"errors\":[";
        if (error) out << '"' << error << '"';
        out << "]\n}\n";
        out.close();
        std::cerr << "[IMM-FPS] " << (out.fail() ? "Failed writing " : "Wrote ")
            << mOutput << '\n';
    }

public:
    FrameSample()
    {
        const char* path = std::getenv("IMM_PERFORMANCE_OUTPUT");
        if (!path || !*path) return;
        mOutput = path;
        mWarmup = Duration("IMM_PERFORMANCE_WARMUP_SECONDS", 2.0);
        mRequested = Duration("IMM_PERFORMANCE_SAMPLE_SECONDS", 5.0);
        if (mWarmup < 0.0 || mRequested < 0.0)
            Finish("Invalid sampling duration; expected a number in (0, 30].");
    }

    ~FrameSample() { Finish("Viewer stopped before sampling completed."); }

    void Frame(bool rendered, int width, int height, const char* api)
    {
        if (mOutput.empty() || mFinished) return;
        const auto now = Clock::now();
        if (!mReadySeen && rendered)
        {
            mReadySeen = true;
            mReady = now;
        }
        if (!mSampling)
        {
            if (!mReadySeen || Seconds(now, mReady) < mWarmup || !rendered)
            {
                if (Seconds(now, mStart) >= 90.0)
                    Finish("No rendered workload became ready within 90 seconds.");
                return;
            }
            mSampling = true;
            mSampleStart = mPreviousRender = now;
            mWidth = width;
            mHeight = height;
            mApi = api;
            // The initial rendered frame starts the window; subsequent frames
            // complete intervals. Do not count an extra frame at time zero.
            return;
        }
        mObserved = Seconds(now, mSampleStart);
        ++mEngineFrames;
        if (width != mWidth || height != mHeight || mApi != api)
        {
            Finish("Rendering configuration changed during sampling.");
            return;
        }
        if (rendered)
        {
            ++mRenderedFrames;
            mIntervals.push_back(1000.0 * Seconds(now, mPreviousRender));
            mPreviousRender = now;
        }
        if (mObserved >= mRequested) Finish(nullptr);
    }
};
}
