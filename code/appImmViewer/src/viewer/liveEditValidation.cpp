#include "liveEditValidation.h"

#include "viewer.h"
#include "libImmImporter/src/document/layer.h"
#include "libImmImporter/src/document/layerPaint/element.h"
#include "libImmPlayer/src/player.h"

#include <chrono>
#include <cstdlib>
#include <utility>
#include <vector>

namespace ExePlayer
{
    uint64_t LiveEditValidation::RequestedFrameFromEnvironment(void)
    {
        const char * value = std::getenv("IMM_VIEWER_LIVE_EDIT");
        if (value == nullptr || value[0] == 0)
            return ~0ull;
        return std::strtoull(value, nullptr, 10);
    }

    void LiveEditValidation::Reset(void)
    {
        *this = LiveEditValidation{};
    }

    void LiveEditValidation::Tick(Viewer & viewer, ImmCore::piLog * log,
        uint64_t frame, bool enabled, uint64_t requestedFrame)
    {
        if (!enabled || log == nullptr)
            return;

        ImmPlayer::Player * player = viewer.GetPlayer();
        if (!mApplied)
        {
            if (frame < requestedFrame)
                return;

            const int docId = viewer.GetPrimaryDocumentId();
            if (docId < 0 || !viewer.IsDocumentLoaded(docId))
                return;

            int layerId = -1;
            const int layerCount = player->GetLayerCount(docId);
            for (int i = 0; i < layerCount; i++)
            {
                ImmPlayer::Player::LayerInfo info;
                if (player->GetLayerInfoByIndex(docId, i, info) &&
                    info.type == static_cast<int>(ImmImporter::Layer::Type::Paint))
                {
                    layerId = info.id;
                    break;
                }
            }
            if (layerId < 0)
            {
                log->Printf(LT_ERROR,
                    L"[IMM_LIVE_EDIT] frame=%llu no paint layer in document %d",
                    static_cast<unsigned long long>(frame), docId);
                mApplied = true;
                mAppliedFrame = frame;
                return;
            }

            using Clock = std::chrono::steady_clock;
            const auto milliseconds = [](Clock::time_point begin, Clock::time_point end)
            {
                return std::chrono::duration<double, std::milli>(end - begin).count();
            };

            const auto attachStart = Clock::now();
            const bool attached = player->AttachEditing(docId);
            const auto attachEnd = Clock::now();

            constexpr int numPoints = 16;
            constexpr int numElements = 2;
            constexpr float biggestStroke = 0.02f;
            std::vector<ImmImporter::Element::PointSource> points(numPoints);
            for (int i = 0; i < numPoints; i++)
            {
                const float t = static_cast<float>(i) / static_cast<float>(numPoints - 1);
                ImmImporter::Element::PointSource & point = points[i];
                point.mPos = ImmCore::vec3(2.0f + t * 0.5f, 1.5f, 0.0f);
                point.mNor = ImmCore::vec3(0.0f, 1.0f, 0.0f);
                point.mDir = ImmCore::vec3(0.0f, 0.0f, 1.0f);
                point.mCol = ImmCore::vec3(1.0f, 1.0f, 1.0f);
                point.mAlpha = 1.0f;
                point.mWidth = biggestStroke;
                point.mLength = t;
                point.mTime = t;
            }

            const ImmCore::bound3d boxBefore = player->GetDocumentBBox(docId);
            const bool hasDrawingBox = player->GetDrawingBBox(
                docId, layerId, 0, mDrawingBoxBefore);
            uint64_t drawingId = 0;
            const bool resolved = hasDrawingBox &&
                player->GetDrawingHandle(docId, layerId, 0, drawingId);

            std::vector<ImmPlayer::Document::AuthoringElementGeometry> elements;
            elements.reserve(numElements);
            for (int elementIndex = 0; elementIndex < numElements; elementIndex++)
            {
                ImmPlayer::Document::AuthoringElementGeometry element;
                element.mBrush = ImmImporter::Element::BrushSectionType::Circle;
                element.mVisibility = ImmImporter::Element::VisibilityType::Always;
                const auto begin = points.begin() + elementIndex * (numPoints / numElements);
                element.mPoints.assign(begin, begin + (numPoints / numElements));
                elements.push_back(std::move(element));
            }

            const auto editStart = Clock::now();
            const bool replaced = attached && resolved && player->QueueDrawingGeometry(
                docId, static_cast<uint32_t>(layerId), drawingId, std::move(elements),
                ImmImporter::Drawing::ColorSpace::Gamma, false, biggestStroke) == 0;
            const auto editEnd = Clock::now();
            const auto commitStart = Clock::now();
            const uint64_t revision = replaced ? player->CommitEdits(docId) : 0;
            const auto commitEnd = Clock::now();

            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT] frame=%llu docId=%d layerId=%d elements=%d points=%d attached=%d replaced=%d revision=%llu "
                L"attachMs=%.3f editMs=%.3f commitMs=%.3f bboxBefore=(%.3f,%.3f,%.3f)-(%.3f,%.3f,%.3f)",
                static_cast<unsigned long long>(frame), docId, layerId, numElements, numPoints,
                attached ? 1 : 0, replaced ? 1 : 0,
                static_cast<unsigned long long>(revision),
                milliseconds(attachStart, attachEnd), milliseconds(editStart, editEnd),
                milliseconds(commitStart, commitEnd),
                boxBefore.mMinX, boxBefore.mMinY, boxBefore.mMinZ,
                boxBefore.mMaxX, boxBefore.mMaxY, boxBefore.mMaxZ);

            mApplied = true;
            mDocumentId = docId;
            mLayerId = layerId;
            mAppliedFrame = frame;
            mRevision = revision;
            mOriginalDrawingId = drawingId;
            return;
        }

        if (mMeasured)
            return;

        if (mDeletionQueued)
        {
            if (frame < mDeletionFrame + 3)
                return;
            mMeasured = true;
            ImmPlayer::Document::AuthoringCommitStatus deletionStatus;
            const bool hasDeletionStatus = mDeletionRevision != 0 &&
                player->GetAuthoringCommitStatus(mDocumentId, mDeletionRevision, deletionStatus);
            int drawingCountAfter = -1;
            for (int i = 0; i < player->GetLayerCount(mDocumentId); i++)
            {
                ImmPlayer::Player::LayerInfo info;
                if (player->GetLayerInfoByIndex(mDocumentId, i, info) && info.id == mLayerId)
                {
                    drawingCountAfter = info.paintNumDrawings;
                    break;
                }
            }
            uint64_t deletedIndexHandle = 0;
            const bool deletedHandleMissing = !player->GetDrawingHandle(
                mDocumentId, mLayerId, mDrawingCountBeforeCreation, deletedIndexHandle);
            uint64_t originalHandle = 0;
            const bool originalHandleStable = player->GetDrawingHandle(
                mDocumentId, mLayerId, 0, originalHandle) &&
                originalHandle == mOriginalDrawingId;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_DELETE] frame=%llu revision=%llu status=%d result=%d "
                L"countRestored=%d deletedHandleMissing=%d originalHandleStable=%d "
                L"referencedDeletionRejected=%d",
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(mDeletionRevision),
                hasDeletionStatus ? static_cast<int>(deletionStatus.mState) : -1,
                hasDeletionStatus ? deletionStatus.mResult : -1,
                drawingCountAfter == mDrawingCountBeforeCreation ? 1 : 0,
                deletedHandleMissing ? 1 : 0, originalHandleStable ? 1 : 0,
                mReferencedDeletionRejected ? 1 : 0);
            return;
        }

        if (mRestoreMappingQueued)
        {
            if (frame < mRestoreMappingFrame + 3)
                return;
            ImmPlayer::Document::AuthoringCommitStatus restoreStatus;
            const bool hasRestoreStatus = mRestoreMappingRevision != 0 &&
                player->GetAuthoringCommitStatus(mDocumentId, mRestoreMappingRevision, restoreStatus);
            uint64_t mappedDrawingId = 0;
            const bool restored = player->GetFrameDrawingHandle(
                mDocumentId, mLayerId, 0, mappedDrawingId) &&
                mappedDrawingId == mOriginalDrawingId;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_FRAME_RESTORE] frame=%llu revision=%llu status=%d result=%d handleMatch=%d",
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(mRestoreMappingRevision),
                hasRestoreStatus ? static_cast<int>(restoreStatus.mState) : -1,
                hasRestoreStatus ? restoreStatus.mResult : -1, restored ? 1 : 0);
            if (!hasRestoreStatus ||
                restoreStatus.mState != ImmPlayer::Document::AuthoringCommitState::Presented ||
                restoreStatus.mResult != 0 || !restored)
            {
                mMeasured = true;
                return;
            }
            const int32_t deletionResult = player->QueueDrawingDeletion(
                mDocumentId, static_cast<uint32_t>(mLayerId), mCreatedDrawingId);
            const uint64_t deletionRevision = deletionResult == 0 ?
                player->CommitEdits(mDocumentId) : 0;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_DELETE] frame=%llu drawing=%llu deletionResult=%d revision=%llu",
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(mCreatedDrawingId), deletionResult,
                static_cast<unsigned long long>(deletionRevision));
            mDeletionQueued = true;
            mDeletionFrame = frame;
            mDeletionRevision = deletionRevision;
            return;
        }

        if (mFrameMappingQueued)
        {
            if (frame < mFrameMappingFrame + 3)
                return;

            ImmPlayer::Document::AuthoringCommitStatus mappingStatus;
            const bool hasMappingStatus = mFrameMappingRevision != 0 &&
                player->GetAuthoringCommitStatus(
                    mDocumentId, mFrameMappingRevision, mappingStatus);
            uint64_t mappedDrawingId = 0;
            const bool resolvedMapping = player->GetFrameDrawingHandle(
                mDocumentId, mLayerId, 0, mappedDrawingId);
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_FRAME_SET] frame=%llu revision=%llu status=%d result=%d handleMatch=%d",
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(mFrameMappingRevision),
                hasMappingStatus ? static_cast<int>(mappingStatus.mState) : -1,
                hasMappingStatus ? mappingStatus.mResult : -1,
                resolvedMapping && mappedDrawingId == mCreatedDrawingId ? 1 : 0);
            if (!hasMappingStatus ||
                mappingStatus.mState != ImmPlayer::Document::AuthoringCommitState::Presented ||
                mappingStatus.mResult != 0 || !resolvedMapping ||
                mappedDrawingId != mCreatedDrawingId)
            {
                mMeasured = true;
                return;
            }
            mReferencedDeletionRejected = player->QueueDrawingDeletion(
                mDocumentId, static_cast<uint32_t>(mLayerId), mCreatedDrawingId) == -6;
            const int32_t restoreResult = player->QueueFrameMapping(
                mDocumentId, static_cast<uint32_t>(mLayerId), 0, mOriginalDrawingId);
            const uint64_t restoreRevision = restoreResult == 0 ?
                player->CommitEdits(mDocumentId) : 0;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_FRAME_RESTORE] frame=%llu drawing=%llu mappingResult=%d revision=%llu",
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(mOriginalDrawingId), restoreResult,
                static_cast<unsigned long long>(restoreRevision));
            mRestoreMappingQueued = true;
            mRestoreMappingFrame = frame;
            mRestoreMappingRevision = restoreRevision;
            return;
        }

        if (mCreationQueued)
        {
            if (frame < mCreationFrame + 3)
                return;

            ImmPlayer::Document::AuthoringCommitStatus creationStatus;
            const bool hasCreationStatus = mCreationRevision != 0 &&
                player->GetAuthoringCommitStatus(mDocumentId, mCreationRevision, creationStatus);
            int drawingCountAfter = -1;
            for (int i = 0; i < player->GetLayerCount(mDocumentId); i++)
            {
                ImmPlayer::Player::LayerInfo info;
                if (player->GetLayerInfoByIndex(mDocumentId, i, info) && info.id == mLayerId)
                {
                    drawingCountAfter = info.paintNumDrawings;
                    break;
                }
            }
            uint64_t resolvedCreatedId = 0;
            const bool resolvedCreated = mDrawingCountBeforeCreation >= 0 &&
                player->GetDrawingHandle(mDocumentId, mLayerId, mDrawingCountBeforeCreation,
                    resolvedCreatedId);
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_CREATE] frame=%llu revision=%llu status=%d result=%d "
                L"drawingCountBefore=%d drawingCountAfter=%d handleMatch=%d",
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(mCreationRevision),
                hasCreationStatus ? static_cast<int>(creationStatus.mState) : -1,
                hasCreationStatus ? creationStatus.mResult : -1,
                mDrawingCountBeforeCreation, drawingCountAfter,
                resolvedCreated && resolvedCreatedId == mCreatedDrawingId ? 1 : 0);

            if (!hasCreationStatus ||
                creationStatus.mState != ImmPlayer::Document::AuthoringCommitState::Presented ||
                creationStatus.mResult != 0 || !resolvedCreated ||
                resolvedCreatedId != mCreatedDrawingId)
            {
                mMeasured = true;
                return;
            }

            const int32_t mappingResult = player->QueueFrameMapping(
                mDocumentId, static_cast<uint32_t>(mLayerId), 0, mCreatedDrawingId);
            const uint64_t mappingRevision = mappingResult == 0 ?
                player->CommitEdits(mDocumentId) : 0;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_FRAME_SET] frame=%llu drawing=%llu mappingResult=%d revision=%llu",
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(mCreatedDrawingId), mappingResult,
                static_cast<unsigned long long>(mappingRevision));
            mFrameMappingQueued = true;
            mFrameMappingFrame = frame;
            mFrameMappingRevision = mappingRevision;
            return;
        }

        if (frame < mAppliedFrame + 3)
            return;

        ImmCore::bound3 drawingBoxAfter;
        const bool hasDrawingBox = player->GetDrawingBBox(
            mDocumentId, mLayerId, 0, drawingBoxAfter);
        ImmPlayer::Document::AuthoringCommitStatus status;
        const bool hasStatus = mRevision != 0 &&
            player->GetAuthoringCommitStatus(mDocumentId, mRevision, status);
        const bool drawingBoxUnchanged = hasDrawingBox &&
            drawingBoxAfter.mMinX == mDrawingBoxBefore.mMinX &&
            drawingBoxAfter.mMinY == mDrawingBoxBefore.mMinY &&
            drawingBoxAfter.mMinZ == mDrawingBoxBefore.mMinZ &&
            drawingBoxAfter.mMaxX == mDrawingBoxBefore.mMaxX &&
            drawingBoxAfter.mMaxY == mDrawingBoxBefore.mMaxY &&
            drawingBoxAfter.mMaxZ == mDrawingBoxBefore.mMaxZ;
        const ImmCore::bound3d boxAfter = player->GetDocumentBBox(mDocumentId);
        log->Printf(LT_MESSAGE,
            L"[IMM_LIVE_EDIT] frame=%llu appliedAt=%llu revision=%llu status=%d result=%d drawingBBoxUnchanged=%d "
            L"bboxAfter=(%.3f,%.3f,%.3f)-(%.3f,%.3f,%.3f)",
            static_cast<unsigned long long>(frame),
            static_cast<unsigned long long>(mAppliedFrame),
            static_cast<unsigned long long>(mRevision),
            hasStatus ? static_cast<int>(status.mState) : -1,
            hasStatus ? status.mResult : -1, drawingBoxUnchanged ? 1 : 0,
            boxAfter.mMinX, boxAfter.mMinY, boxAfter.mMinZ,
            boxAfter.mMaxX, boxAfter.mMaxY, boxAfter.mMaxZ);

        if (!hasStatus || status.mState != ImmPlayer::Document::AuthoringCommitState::Presented ||
            status.mResult != 0)
        {
            mMeasured = true;
            return;
        }

        for (int i = 0; i < player->GetLayerCount(mDocumentId); i++)
        {
            ImmPlayer::Player::LayerInfo info;
            if (player->GetLayerInfoByIndex(mDocumentId, i, info) && info.id == mLayerId)
            {
                mDrawingCountBeforeCreation = info.paintNumDrawings;
                break;
            }
        }

        uint64_t createdDrawingId = 0;
        const int32_t createResult = player->QueueDrawingCreation(
            mDocumentId, static_cast<uint32_t>(mLayerId), createdDrawingId);
        constexpr int creationPointCount = 8;
        constexpr float creationBiggestStroke = 0.015f;
        ImmPlayer::Document::AuthoringElementGeometry creationElement;
        creationElement.mBrush = ImmImporter::Element::BrushSectionType::Circle;
        creationElement.mVisibility = ImmImporter::Element::VisibilityType::Always;
        creationElement.mPoints.resize(creationPointCount);
        for (int i = 0; i < creationPointCount; i++)
        {
            const float t = static_cast<float>(i) / static_cast<float>(creationPointCount - 1);
            ImmImporter::Element::PointSource & point = creationElement.mPoints[i];
            point.mPos = ImmCore::vec3(-2.0f + t * 0.4f, 1.0f, 0.0f);
            point.mNor = ImmCore::vec3(0.0f, 1.0f, 0.0f);
            point.mDir = ImmCore::vec3(0.0f, 0.0f, 1.0f);
            point.mCol = ImmCore::vec3(1.0f, 1.0f, 1.0f);
            point.mAlpha = 1.0f;
            point.mWidth = creationBiggestStroke;
            point.mLength = t;
            point.mTime = t;
        }
        std::vector<ImmPlayer::Document::AuthoringElementGeometry> creationElements;
        creationElements.push_back(std::move(creationElement));
        const int32_t geometryResult = createResult == 0 ? player->QueueDrawingGeometry(
            mDocumentId, static_cast<uint32_t>(mLayerId), createdDrawingId,
            std::move(creationElements), ImmImporter::Drawing::ColorSpace::Gamma, false,
            creationBiggestStroke) : createResult;
        const uint64_t creationRevision = geometryResult == 0 ?
            player->CommitEdits(mDocumentId) : 0;
        log->Printf(LT_MESSAGE,
            L"[IMM_LIVE_EDIT_CREATE] frame=%llu drawing=%llu createResult=%d geometryResult=%d revision=%llu",
            static_cast<unsigned long long>(frame),
            static_cast<unsigned long long>(createdDrawingId), createResult, geometryResult,
            static_cast<unsigned long long>(creationRevision));
        mCreationQueued = true;
        mCreationFrame = frame;
        mCreationRevision = creationRevision;
        mCreatedDrawingId = createdDrawingId;
    }
}
