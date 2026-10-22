#pragma once

#include "libImmCore/src/libBasics/piVecTypes.h"
#include "libImmImporter/src/document/layerSpawnArea.h"

#include <stdint.h>
#include <vector>

namespace ImmCore
{
    class piLog;
}

namespace ExePlayer
{
    class Viewer;

    class LiveEditValidation
    {
    public:
        static uint64_t RequestedFrameFromEnvironment(void);

        void Reset(void);
        void Tick(Viewer & viewer, ImmCore::piLog * log, uint64_t frame,
            bool enabled, uint64_t requestedFrame);
        bool IsBenchmarkEnabled() const { return mBenchmarkEnabled; }
        bool IsBenchmarkFinished() const { return mBenchmarkEnabled && mMeasured; }
        bool IsBenchmarkSuccessful() const
        { return IsBenchmarkFinished() && mBenchmarkIteration == mBenchmarkWarmup + mBenchmarkSamples; }

        bool IsStandaloneProbeEnabled() const { return mStandaloneEnabled; }
        bool IsStandaloneProbeFinished() const { return mStandaloneEnabled && mMeasured; }
        bool IsStandaloneProbeSuccessful() const { return IsStandaloneProbeFinished() && mStandaloneSuccessful; }

    private:
        void TickPaintCreation(Viewer & viewer, ImmCore::piLog * log, uint64_t frame, uint64_t requestedFrame);
        void TickSubtreeDeletion(Viewer & viewer, ImmCore::piLog * log, uint64_t frame, uint64_t requestedFrame);
        void TickFrameSequence(Viewer & viewer, ImmCore::piLog * log, uint64_t frame, uint64_t requestedFrame);
        void TickFrameSequenceBundle(Viewer & viewer, ImmCore::piLog * log, uint64_t frame);
        void TickNewPaintFrameSequence(Viewer & viewer, ImmCore::piLog * log, uint64_t frame, uint64_t requestedFrame);
        void TickGPUPreparedUnload(Viewer & viewer, ImmCore::piLog * log, uint64_t frame, uint64_t requestedFrame);
        void TickPaintPlaybackKeys(Viewer & viewer, ImmCore::piLog * log, uint64_t frame, uint64_t requestedFrame);
        bool mPaintPlaybackKeysEnabled = false;
        int mPaintPlaybackKeysStage = 0;
        uint32_t mPaintPlaybackSeen = 0;
        bool mPaintPlaybackReloaded = false;
        struct SubtreeLayerSnapshot
        {
            int id = -1;
            int parent = -1;
            int child = -1;
            int strokes = 0;
            int hasBox = 0;
            ImmCore::bound3 box;
            bool target = false;
            std::vector<uint64_t> handles;
            std::vector<ImmCore::bound3> boxes;
        };
        bool mStandaloneConfigured = false;
        bool mStandaloneEnabled = false;
        bool mGPUPreparedUnloadEnabled = false;
        bool mGPUPreparedCaptureOnly = false;
        bool mGPUPreparedSessionInvalid = false;
        int mGPUPreparedUnloadStage = 0;
        int mGPUPreparedReloadDocument = -1;
        int mGPUPreparedBaseDrawCalls = 0;
        int mGPUPreparedBaseTriangles = 0;
        uint64_t mGPUPreparedHeldFrame = 0;
        bool mPaintCreationEnabled = false;
        bool mFrameSequenceEnabled = false;
        bool mFrameSequenceReadinessLogged = false;
        int mFrameSequenceStage = 0;
        int mFrameSequenceIndex = 0;
        int mFrameSequenceReload = -1;
        int mFrameSequenceTrianglesA = 0;
        int mFrameSequenceTrianglesB = 0;
        int mFrameSequenceDrawCalls = 0;
        std::vector<uint64_t> mFrameSequenceExpected;
        ImmCore::bound3 mFrameSequenceBoxA;
        ImmCore::bound3 mFrameSequenceBoxB;
        uint32_t mFrameSequencePlayingDrawings = 0;
        int64_t mFrameSequencePlayingPreviousTime = 0;
        bool mFrameSequencePlayingAdvanced = false;
        bool mPaintCreationUnload = false;
        bool mPaintCreationSessionInvalid = false;
        int mPaintCreationReloadDocument = -1;
        int mPaintCreationStage = 0;
        int mPaintCreationLayer = -1;
        uint64_t mPaintCreationHandle = 0;
        bool mStandaloneSuccessful = false;
        bool mStandaloneExpectRejection = false;
        int mStandaloneTarget = -1;
        int mStandaloneLayerCount = 0;
        int mStandaloneTargetLayers = 0;
        uint64_t mStandaloneOldRevision = 0;
        uint64_t mStandaloneTerminalFrame = 0;
        ImmCore::bound3d mStandaloneBoxBefore;
        std::vector<SubtreeLayerSnapshot> mStandaloneLayers;
        void TickBenchmark(Viewer & viewer, ImmCore::piLog * log, uint64_t frame, uint64_t requestedFrame);
        void TickPropertyBenchmark(Viewer & viewer, ImmCore::piLog * log, uint64_t frame, uint64_t requestedFrame);
        struct PropertyBenchmarkLayer
        {
            int id = -1;
            int parent = -1;
            int frames = 0;
            ImmCore::trans3d transform = ImmCore::trans3d::identity();
            std::vector<uint64_t> handles;
            std::vector<ImmCore::bound3> boxes;
        };
        int mPropertyBenchmarkKind = 0;
        bool mPropertyBenchmarkControl = false;
        int mPropertyBenchmarkStage = 0;
        int mPropertyBenchmarkLayerCount = 0;
        int mPropertyBenchmarkBaseDrawCalls = 0;
        int mPropertyBenchmarkBaseTriangles = 0;
        uint64_t mPropertyBenchmarkTerminalFrame = 0;
        std::vector<PropertyBenchmarkLayer> mPropertyBenchmarkLayers;
        bool mBenchmarkConfigured = false;
        bool mBenchmarkEnabled = false;
        bool mBenchmarkPending = false;
        bool mBenchmarkPlaybackReady = false;
        bool mBenchmarkSeeked = false;
        uint32_t mBenchmarkPoints = 0;
        uint32_t mBenchmarkSamples = 30;
        uint32_t mBenchmarkWarmup = 5;
        uint32_t mBenchmarkIteration = 0;
        uint64_t mBenchmarkPreviousRevision = 0;
        double mBenchmarkQueueMs = 0;
        double mBenchmarkCommitCallMs = 0;
        bool mApplied = false;
        bool mMeasured = false;
        bool mCreationQueued = false;
        bool mDeletionQueued = false;
        bool mPropertyQueued = false;
        bool mKeyQueued = false;
        bool mKeyRemovalQueued = false;
        bool mInitialSpawnAreaQueued = false;
        bool mSpawnAreaQueued = false;
        bool mGroupLayerQueued = false;
        bool mGroupLayerChildQueued = false;
        bool mGroupLayerReorderQueued = false;
        bool mGroupLayerCrossParentQueued = false;
        bool mGroupLayerDeletionQueued = false;
        bool mReferencedDeletionRejected = false;
        int mDocumentId = -1;
        int mLayerId = -1;
        uint64_t mAppliedFrame = 0;
        uint64_t mRevision = 0;
        uint64_t mCreationFrame = 0;
        uint64_t mCreationRevision = 0;
        uint64_t mCreatedDrawingId = 0;
        uint64_t mOriginalDrawingId = 0;
        uint64_t mDeletionFrame = 0;
        uint64_t mDeletionRevision = 0;
        uint64_t mPropertyFrame = 0;
        uint64_t mPropertyRevision = 0;
        uint64_t mKeyFrame = 0;
        uint64_t mKeyRevision = 0;
        uint64_t mKeyRemovalFrame = 0;
        uint64_t mKeyRemovalRevision = 0;
        uint64_t mInitialSpawnAreaFrame = 0;
        uint64_t mInitialSpawnAreaRevision = 0;
        int mInitialSpawnAreaLayerId = -1;
        uint64_t mSpawnAreaFrame = 0;
        uint64_t mSpawnAreaRevision = 0;
        ImmImporter::LayerSpawnArea::Volume mTargetSpawnAreaVolume = {};
        ImmImporter::LayerSpawnArea::TrackingLevel mTargetSpawnAreaTracking =
            ImmImporter::LayerSpawnArea::TrackingLevel::Floor;
        ImmCore::trans3d mTargetSpawnAreaTransform = ImmCore::trans3d::identity();
        uint64_t mGroupLayerFrame = 0;
        uint64_t mGroupLayerRevision = 0;
        uint64_t mGroupLayerChildFrame = 0;
        uint64_t mGroupLayerChildRevision = 0;
        uint64_t mGroupLayerReorderFrame = 0;
        uint64_t mGroupLayerReorderRevision = 0;
        uint64_t mGroupLayerCrossParentFrame = 0;
        uint64_t mGroupLayerCrossParentRevision = 0;
        int mCreatedGroupLayerId = -1;
        int mCreatedGroupChildLayerId = -1;
        int mCreatedGroupParentId = -1;
        int mGroupLayerReparentTargetId = -1;
        int mLayerCountBeforeGroupCreation = 0;
        uint64_t mGroupLayerDeletionFrame = 0;
        uint64_t mGroupLayerDeletionRevision = 0;
        bool mOriginalCanonicalVisible = false;
        bool mTargetCanonicalVisible = false;
        float mOriginalEffectiveOpacity = 1.0f;
        float mTargetCanonicalOpacity = 1.0f;
        ImmCore::trans3d mOriginalEffectiveTransform = ImmCore::trans3d::identity();
        ImmCore::trans3d mTargetCanonicalTransform = ImmCore::trans3d::identity();
        int mDrawingCountBeforeCreation = -1;
        int mVisibilityKeyCountBefore = 0;
        ImmCore::bound3 mDrawingBoxBefore;
    };
}
