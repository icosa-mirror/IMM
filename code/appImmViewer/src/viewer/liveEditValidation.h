#pragma once

#include "libImmCore/src/libBasics/piVecTypes.h"
#include "libImmImporter/src/document/layerSpawnArea.h"

#include <stdint.h>

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

    private:
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
        uint64_t mGroupLayerReorderFrame = 0;
        uint64_t mGroupLayerReorderRevision = 0;
        uint64_t mGroupLayerCrossParentFrame = 0;
        uint64_t mGroupLayerCrossParentRevision = 0;
        int mCreatedGroupLayerId = -1;
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
