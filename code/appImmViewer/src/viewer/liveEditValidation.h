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
        int mDocumentId = -1;
        int mLayerId = -1;
        uint64_t mAppliedFrame = 0;
        uint64_t mRevision = 0;
        ImmCore::bound3 mDrawingBoxBefore;
    };
}
