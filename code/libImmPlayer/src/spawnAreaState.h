#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ImmPlayer
{
    // Optional viewpoint policy; existing enumeration/selection remains unrestricted.
    enum class PlaybackRequestStatus : int32_t
    {
        None = 0, Pending = 1, Applied = 2, Rejected = 3, Superseded = 4
    };

    // Matches the existing Unity SpawnAreaPose layout, without strings or ownership.
    struct SpawnAreaPoseState
    {
        float posx, posy, posz;
        float rotx, roty, rotz, rotw;
        float sca;
        int32_t animated;
        int32_t isFloorLevel;
        int32_t locomotion;
    };

    struct SpawnAreaSnapshot
    {
        uint32_t structSize;
        uint32_t structVersion;
        uint64_t evaluationRevision;
        uint64_t requestedCommand;
        uint64_t completedCommand;
        int32_t commandStatus;
        int32_t chapter;
        int32_t activeId;
        int32_t preferredId;
        int32_t availableCount;
        int32_t activeAvailable;
        int32_t preferredAvailable;
        int32_t needsUpdate;
        SpawnAreaPoseState activePose;
        SpawnAreaPoseState preferredPose;
        uint32_t reserved[2];
    };

    static_assert(sizeof(SpawnAreaPoseState) == 44, "SpawnAreaPoseState ABI changed");
    static_assert(offsetof(SpawnAreaSnapshot, activePose) == 64, "SpawnAreaSnapshot ABI changed");
    static_assert(sizeof(SpawnAreaSnapshot) == 160, "SpawnAreaSnapshot ABI changed");
}
