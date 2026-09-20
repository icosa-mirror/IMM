#pragma once

#include "libImmCore/src/libBasics/piVecTypes.h"

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
        bool mFrameMappingQueued = false;
        bool mDeletionQueued = false;
        bool mPropertyQueued = false;
        bool mReferencedDeletionRejected = false;
        int mDocumentId = -1;
        int mLayerId = -1;
        uint64_t mAppliedFrame = 0;
        uint64_t mRevision = 0;
        uint64_t mCreationFrame = 0;
        uint64_t mCreationRevision = 0;
        uint64_t mCreatedDrawingId = 0;
        uint64_t mOriginalDrawingId = 0;
        uint64_t mFrameMappingFrame = 0;
        uint64_t mFrameMappingRevision = 0;
        uint64_t mDeletionFrame = 0;
        uint64_t mDeletionRevision = 0;
        uint64_t mPropertyFrame = 0;
        uint64_t mPropertyRevision = 0;
        bool mOriginalCanonicalVisible = false;
        bool mTargetCanonicalVisible = false;
        int mDrawingCountBeforeCreation = -1;
        ImmCore::bound3 mDrawingBoxBefore;
    };
}
