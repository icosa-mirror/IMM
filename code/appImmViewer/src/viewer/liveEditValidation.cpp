#include "liveEditValidation.h"

#include "viewer.h"
#include "libImmImporter/src/document/layer.h"
#include "libImmImporter/src/document/layerPaint/element.h"
#include "libImmPlayer/src/player.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <utility>
#include <vector>

namespace ExePlayer
{
    static bool iTransformsMatch(const ImmCore::trans3d & a, const ImmCore::trans3d & b)
    {
        constexpr double epsilon = 0.000001;
        return std::fabs(a.mTranslation.x - b.mTranslation.x) < epsilon &&
            std::fabs(a.mTranslation.y - b.mTranslation.y) < epsilon &&
            std::fabs(a.mTranslation.z - b.mTranslation.z) < epsilon &&
            std::fabs(a.mRotation.x - b.mRotation.x) < epsilon &&
            std::fabs(a.mRotation.y - b.mRotation.y) < epsilon &&
            std::fabs(a.mRotation.z - b.mRotation.z) < epsilon &&
            std::fabs(a.mRotation.w - b.mRotation.w) < epsilon &&
            std::fabs(a.mScale - b.mScale) < epsilon && a.mFlip == b.mFlip;
    }

    static bool iSpawnVolumesMatch(const ImmImporter::LayerSpawnArea::Volume & a,
        const ImmImporter::LayerSpawnArea::Volume & b)
    {
        constexpr float epsilon = 0.000001f;
        if (a.mType != b.mType || a.mAllowTranslationX != b.mAllowTranslationX ||
            a.mAllowTranslationY != b.mAllowTranslationY ||
            a.mAllowTranslationZ != b.mAllowTranslationZ)
            return false;
        if (a.mType == ImmImporter::LayerSpawnArea::Volume::Type::Sphere)
            return std::fabs(a.mShape.mSphere.x - b.mShape.mSphere.x) < epsilon &&
                std::fabs(a.mShape.mSphere.y - b.mShape.mSphere.y) < epsilon &&
                std::fabs(a.mShape.mSphere.z - b.mShape.mSphere.z) < epsilon &&
                std::fabs(a.mShape.mSphere.w - b.mShape.mSphere.w) < epsilon;
        return std::fabs(a.mShape.mBox.mMinX - b.mShape.mBox.mMinX) < epsilon &&
            std::fabs(a.mShape.mBox.mMinY - b.mShape.mBox.mMinY) < epsilon &&
            std::fabs(a.mShape.mBox.mMinZ - b.mShape.mBox.mMinZ) < epsilon &&
            std::fabs(a.mShape.mBox.mMaxX - b.mShape.mBox.mMaxX) < epsilon &&
            std::fabs(a.mShape.mBox.mMaxY - b.mShape.mBox.mMaxY) < epsilon &&
            std::fabs(a.mShape.mBox.mMaxZ - b.mShape.mBox.mMaxZ) < epsilon;
    }

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

        if (mSpawnAreaQueued)
        {
            if (frame < mSpawnAreaFrame + 3)
                return;
            mMeasured = true;
            ImmPlayer::Document::AuthoringCommitStatus spawnStatus;
            const bool hasSpawnStatus = mSpawnAreaRevision != 0 &&
                player->GetAuthoringCommitStatus(mDocumentId, mSpawnAreaRevision, spawnStatus);
            ImmPlayer::Player::SpawnAreaDiagnostics diagnostics;
            const bool hasDiagnostics = player->GetSpawnAreaDiagnostics(
                mDocumentId, mInitialSpawnAreaLayerId, diagnostics);
            const bool spawnAreaMatch = hasDiagnostics &&
                iSpawnVolumesMatch(diagnostics.volume, mTargetSpawnAreaVolume) &&
                diagnostics.tracking == mTargetSpawnAreaTracking &&
                iTransformsMatch(diagnostics.transform, mTargetSpawnAreaTransform);
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_SPAWN_AREA] frame=%llu revision=%llu status=%d result=%d spawnAreaMatch=%d",
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(mSpawnAreaRevision),
                hasSpawnStatus ? static_cast<int>(spawnStatus.mState) : -1,
                hasSpawnStatus ? spawnStatus.mResult : -1,
                spawnAreaMatch ? 1 : 0);
            return;
        }

        if (mInitialSpawnAreaQueued)
        {
            if (frame < mInitialSpawnAreaFrame + 3)
                return;
            ImmPlayer::Document::AuthoringCommitStatus spawnStatus;
            const bool hasSpawnStatus = mInitialSpawnAreaRevision != 0 &&
                player->GetAuthoringCommitStatus(
                    mDocumentId, mInitialSpawnAreaRevision, spawnStatus);
            const bool initialLayerMatch = player->GetInitialSpawnAreaLayerId(mDocumentId) ==
                mInitialSpawnAreaLayerId;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_INITIAL_SPAWN] frame=%llu revision=%llu status=%d result=%d initialLayerMatch=%d",
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(mInitialSpawnAreaRevision),
                hasSpawnStatus ? static_cast<int>(spawnStatus.mState) : -1,
                hasSpawnStatus ? spawnStatus.mResult : -1,
                initialLayerMatch ? 1 : 0);
            const bool initialSpawnSucceeded = hasSpawnStatus &&
                spawnStatus.mState == ImmPlayer::Document::AuthoringCommitState::Presented &&
                spawnStatus.mResult == 0 && initialLayerMatch;
            ImmPlayer::Player::SpawnAreaDiagnostics diagnostics;
            if (!initialSpawnSucceeded || !player->GetSpawnAreaDiagnostics(
                mDocumentId, mInitialSpawnAreaLayerId, diagnostics))
            {
                mMeasured = true;
                return;
            }
            mTargetSpawnAreaVolume = diagnostics.volume;
            mTargetSpawnAreaVolume.mAllowTranslationX =
                !mTargetSpawnAreaVolume.mAllowTranslationX;
            if (mTargetSpawnAreaVolume.mType ==
                ImmImporter::LayerSpawnArea::Volume::Type::Sphere)
                mTargetSpawnAreaVolume.mShape.mSphere.w += 0.125f;
            else
                mTargetSpawnAreaVolume.mShape.mBox.mMaxX += 0.125f;
            mTargetSpawnAreaTracking = diagnostics.tracking ==
                ImmImporter::LayerSpawnArea::TrackingLevel::Floor ?
                ImmImporter::LayerSpawnArea::TrackingLevel::Eye :
                ImmImporter::LayerSpawnArea::TrackingLevel::Floor;
            mTargetSpawnAreaTransform = diagnostics.transform;
            mTargetSpawnAreaTransform.mTranslation.y += 0.125;
            const int32_t setResult = player->QueueSpawnArea(
                mDocumentId, static_cast<uint32_t>(mInitialSpawnAreaLayerId),
                mTargetSpawnAreaVolume, mTargetSpawnAreaTracking,
                mTargetSpawnAreaTransform);
            const uint64_t setRevision = setResult == 0 ?
                player->CommitEdits(mDocumentId) : 0;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_SPAWN_AREA] frame=%llu layer=%d setResult=%d revision=%llu",
                static_cast<unsigned long long>(frame), mInitialSpawnAreaLayerId,
                setResult, static_cast<unsigned long long>(setRevision));
            mSpawnAreaQueued = true;
            mSpawnAreaFrame = frame;
            mSpawnAreaRevision = setRevision;
            return;
        }

        if (mKeyRemovalQueued)
        {
            if (frame < mKeyRemovalFrame + 3)
                return;
            ImmPlayer::Document::AuthoringCommitStatus removalStatus;
            const bool hasRemovalStatus = mKeyRemovalRevision != 0 &&
                player->GetAuthoringCommitStatus(
                    mDocumentId, mKeyRemovalRevision, removalStatus);
            ImmPlayer::Player::LayerDiagnostics diagnostics;
            const bool hasDiagnostics = player->GetLayerDiagnostics(
                mDocumentId, mLayerId, diagnostics);
            const bool keyCountRestored = hasDiagnostics &&
                diagnostics.visibilityKeyCount == mVisibilityKeyCountBefore;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_KEY_REMOVE] frame=%llu revision=%llu status=%d result=%d keyCountRestored=%d",
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(mKeyRemovalRevision),
                hasRemovalStatus ? static_cast<int>(removalStatus.mState) : -1,
                hasRemovalStatus ? removalStatus.mResult : -1,
                keyCountRestored ? 1 : 0);
            const bool removalSucceeded = hasRemovalStatus &&
                removalStatus.mState == ImmPlayer::Document::AuthoringCommitState::Presented &&
                removalStatus.mResult == 0 && keyCountRestored;
            if (!removalSucceeded)
            {
                mMeasured = true;
                return;
            }
            int spawnLayerId = -1;
            for (int i = 0; i < player->GetLayerCount(mDocumentId); i++)
            {
                ImmPlayer::Player::LayerInfo info;
                if (player->GetLayerInfoByIndex(mDocumentId, i, info) &&
                    info.type == static_cast<int>(ImmImporter::Layer::Type::SpawnArea))
                {
                    spawnLayerId = info.id;
                    break;
                }
            }
            if (spawnLayerId < 0)
            {
                mMeasured = true;
                log->Printf(LT_MESSAGE,
                    L"[IMM_LIVE_EDIT_INITIAL_SPAWN] frame=%llu available=0",
                    static_cast<unsigned long long>(frame));
                return;
            }
            const int32_t spawnResult = spawnLayerId >= 0 ?
                player->QueueInitialSpawnArea(
                    mDocumentId, static_cast<uint32_t>(spawnLayerId)) : -1;
            const uint64_t spawnRevision = spawnResult == 0 ?
                player->CommitEdits(mDocumentId) : 0;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_INITIAL_SPAWN] frame=%llu layer=%d setResult=%d revision=%llu",
                static_cast<unsigned long long>(frame), spawnLayerId, spawnResult,
                static_cast<unsigned long long>(spawnRevision));
            mInitialSpawnAreaQueued = true;
            mInitialSpawnAreaFrame = frame;
            mInitialSpawnAreaRevision = spawnRevision;
            mInitialSpawnAreaLayerId = spawnLayerId;
            return;
        }

        if (mKeyQueued)
        {
            if (frame < mKeyFrame + 3)
                return;
            ImmPlayer::Document::AuthoringCommitStatus keyStatus;
            const bool hasKeyStatus = mKeyRevision != 0 &&
                player->GetAuthoringCommitStatus(mDocumentId, mKeyRevision, keyStatus);
            ImmPlayer::Player::LayerDiagnostics diagnostics;
            const bool hasDiagnostics = player->GetLayerDiagnostics(
                mDocumentId, mLayerId, diagnostics);
            const bool keyCountChanged = hasDiagnostics &&
                diagnostics.visibilityKeyCount == mVisibilityKeyCountBefore + 1;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_KEY_SET] frame=%llu revision=%llu status=%d result=%d keyCountChanged=%d",
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(mKeyRevision),
                hasKeyStatus ? static_cast<int>(keyStatus.mState) : -1,
                hasKeyStatus ? keyStatus.mResult : -1, keyCountChanged ? 1 : 0);
            const bool keySucceeded = hasKeyStatus &&
                keyStatus.mState == ImmPlayer::Document::AuthoringCommitState::Presented &&
                keyStatus.mResult == 0 && keyCountChanged;
            if (!keySucceeded)
            {
                mMeasured = true;
                return;
            }
            const int32_t removalResult = player->QueueAnimationKeyRemoval(
                mDocumentId, static_cast<uint32_t>(mLayerId),
                ImmImporter::Layer::AnimProperty::Visibility,
                ImmCore::piTick::FromSeconds(3600.0));
            const uint64_t removalRevision = removalResult == 0 ?
                player->CommitEdits(mDocumentId) : 0;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_KEY_REMOVE] frame=%llu layer=%d removeResult=%d revision=%llu",
                static_cast<unsigned long long>(frame), mLayerId, removalResult,
                static_cast<unsigned long long>(removalRevision));
            mKeyRemovalQueued = true;
            mKeyRemovalFrame = frame;
            mKeyRemovalRevision = removalRevision;
            return;
        }

        if (mPropertyQueued)
        {
            if (frame < mPropertyFrame + 3)
                return;
            ImmPlayer::Document::AuthoringCommitStatus propertyStatus;
            const bool hasPropertyStatus = mPropertyRevision != 0 &&
                player->GetAuthoringCommitStatus(
                    mDocumentId, mPropertyRevision, propertyStatus);
            ImmPlayer::Player::LayerDiagnostics diagnostics;
            const bool hasDiagnostics = player->GetLayerDiagnostics(
                mDocumentId, mLayerId, diagnostics);
            const bool canonicalChanged = hasDiagnostics &&
                diagnostics.canonicalVisible == (mTargetCanonicalVisible ? 1 : 0);
            const bool overridePreserved = hasDiagnostics &&
                diagnostics.visibilityOverrideEnabled == 1 &&
                diagnostics.visibilityOverrideValue == (mOriginalCanonicalVisible ? 1 : 0);
            const bool effectivePreserved = hasDiagnostics &&
                diagnostics.isVisible == (mOriginalCanonicalVisible ? 1 : 0);
            const bool canonicalOpacityChanged = hasDiagnostics &&
                std::fabs(diagnostics.canonicalOpacity - mTargetCanonicalOpacity) < 0.0001f;
            const bool opacityOverridePreserved = hasDiagnostics &&
                diagnostics.opacityOverrideEnabled == 1 &&
                std::fabs(diagnostics.opacityOverrideValue - mOriginalEffectiveOpacity) < 0.0001f;
            const bool opacityEffectivePreserved = hasDiagnostics &&
                std::fabs(diagnostics.opacity - mOriginalEffectiveOpacity) < 0.0001f;
            const bool canonicalTransformChanged = hasDiagnostics &&
                iTransformsMatch(diagnostics.canonicalTransform, mTargetCanonicalTransform);
            const bool transformOverridePreserved = hasDiagnostics &&
                diagnostics.transformOverrideEnabled == 1 &&
                iTransformsMatch(diagnostics.transformOverride, mOriginalEffectiveTransform);
            const bool transformEffectivePreserved = hasDiagnostics &&
                iTransformsMatch(diagnostics.transform, mOriginalEffectiveTransform);
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_PROPERTY] frame=%llu revision=%llu status=%d result=%d "
                L"canonicalChanged=%d overridePreserved=%d effectivePreserved=%d "
                L"canonicalOpacityChanged=%d opacityOverridePreserved=%d opacityEffectivePreserved=%d "
                L"canonicalTransformChanged=%d transformOverridePreserved=%d transformEffectivePreserved=%d",
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(mPropertyRevision),
                hasPropertyStatus ? static_cast<int>(propertyStatus.mState) : -1,
                hasPropertyStatus ? propertyStatus.mResult : -1,
                canonicalChanged ? 1 : 0, overridePreserved ? 1 : 0,
                effectivePreserved ? 1 : 0, canonicalOpacityChanged ? 1 : 0,
                opacityOverridePreserved ? 1 : 0, opacityEffectivePreserved ? 1 : 0,
                canonicalTransformChanged ? 1 : 0, transformOverridePreserved ? 1 : 0,
                transformEffectivePreserved ? 1 : 0);
            const bool propertySucceeded = hasPropertyStatus &&
                propertyStatus.mState == ImmPlayer::Document::AuthoringCommitState::Presented &&
                propertyStatus.mResult == 0 && canonicalChanged && overridePreserved &&
                effectivePreserved && canonicalOpacityChanged && opacityOverridePreserved &&
                opacityEffectivePreserved && canonicalTransformChanged &&
                transformOverridePreserved && transformEffectivePreserved;
            if (!propertySucceeded)
            {
                mMeasured = true;
                return;
            }
            mVisibilityKeyCountBefore = diagnostics.visibilityKeyCount;
            ImmImporter::Layer::AnimValue value;
            value.init();
            value.mBool = mTargetCanonicalVisible;
            const int32_t keyResult = player->QueueAnimationKey(
                mDocumentId, static_cast<uint32_t>(mLayerId),
                ImmImporter::Layer::AnimProperty::Visibility,
                ImmCore::piTick::FromSeconds(3600.0), value,
                ImmImporter::Layer::InterpolationType::None);
            const uint64_t keyRevision = keyResult == 0 ?
                player->CommitEdits(mDocumentId) : 0;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_KEY_SET] frame=%llu layer=%d keyResult=%d revision=%llu",
                static_cast<unsigned long long>(frame), mLayerId, keyResult,
                static_cast<unsigned long long>(keyRevision));
            mKeyQueued = true;
            mKeyFrame = frame;
            mKeyRevision = keyRevision;
            return;
        }

        if (mDeletionQueued)
        {
            if (frame < mDeletionFrame + 3)
                return;
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
            uint64_t mappedDrawingId = 0;
            const bool restored = player->GetFrameDrawingHandle(
                mDocumentId, mLayerId, 0, mappedDrawingId) &&
                mappedDrawingId == mOriginalDrawingId;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_FRAME_RESTORE] frame=%llu revision=%llu status=%d result=%d handleMatch=%d",
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(mDeletionRevision),
                hasDeletionStatus ? static_cast<int>(deletionStatus.mState) : -1,
                hasDeletionStatus ? deletionStatus.mResult : -1, restored ? 1 : 0);
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
            const bool deletionSucceeded = hasDeletionStatus &&
                deletionStatus.mState == ImmPlayer::Document::AuthoringCommitState::Presented &&
                deletionStatus.mResult == 0 &&
                drawingCountAfter == mDrawingCountBeforeCreation && deletedHandleMissing &&
                originalHandleStable && restored && mReferencedDeletionRejected;
            ImmPlayer::Player::LayerDiagnostics diagnostics;
            if (!deletionSucceeded ||
                !player->GetLayerDiagnostics(mDocumentId, mLayerId, diagnostics))
            {
                mMeasured = true;
                return;
            }
            mOriginalCanonicalVisible = diagnostics.canonicalVisible != 0;
            mTargetCanonicalVisible = !mOriginalCanonicalVisible;
            mOriginalEffectiveOpacity = diagnostics.opacity;
            mTargetCanonicalOpacity = diagnostics.canonicalOpacity > 0.5f ? 0.25f : 0.75f;
            mOriginalEffectiveTransform = diagnostics.transform;
            mTargetCanonicalTransform = diagnostics.canonicalTransform;
            mTargetCanonicalTransform.mTranslation.x += 0.125;
            const bool overrideSet = player->SetLayerVisible(
                mDocumentId, mLayerId, mOriginalCanonicalVisible) &&
                player->SetLayerOpacity(mDocumentId, mLayerId, mOriginalEffectiveOpacity) &&
                player->SetLayerTransform(
                    mDocumentId, mLayerId, mOriginalEffectiveTransform);
            const int32_t propertyResult = overrideSet ? player->QueueLayerProperties(
                mDocumentId, static_cast<uint32_t>(mLayerId), true,
                mTargetCanonicalVisible, true, mTargetCanonicalOpacity, true,
                mTargetCanonicalTransform) : -4;
            const uint64_t propertyRevision = propertyResult == 0 ?
                player->CommitEdits(mDocumentId) : 0;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_PROPERTY] frame=%llu layer=%d visible=%d opacity=%.3f overrideSet=%d "
                L"propertyResult=%d revision=%llu",
                static_cast<unsigned long long>(frame), mLayerId,
                mTargetCanonicalVisible ? 1 : 0, mTargetCanonicalOpacity,
                overrideSet ? 1 : 0,
                propertyResult, static_cast<unsigned long long>(propertyRevision));
            mPropertyQueued = true;
            mPropertyFrame = frame;
            mPropertyRevision = propertyRevision;
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
            uint64_t mappedDrawingId = 0;
            const bool resolvedMapping = player->GetFrameDrawingHandle(
                mDocumentId, mLayerId, 0, mappedDrawingId);
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_CREATE] frame=%llu revision=%llu status=%d result=%d "
                L"drawingCountBefore=%d drawingCountAfter=%d handleMatch=%d",
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(mCreationRevision),
                hasCreationStatus ? static_cast<int>(creationStatus.mState) : -1,
                hasCreationStatus ? creationStatus.mResult : -1,
                mDrawingCountBeforeCreation, drawingCountAfter,
                resolvedCreated && resolvedCreatedId == mCreatedDrawingId ? 1 : 0);
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_FRAME_SET] frame=%llu revision=%llu status=%d result=%d handleMatch=%d",
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(mCreationRevision),
                hasCreationStatus ? static_cast<int>(creationStatus.mState) : -1,
                hasCreationStatus ? creationStatus.mResult : -1,
                resolvedMapping && mappedDrawingId == mCreatedDrawingId ? 1 : 0);

            if (!hasCreationStatus ||
                creationStatus.mState != ImmPlayer::Document::AuthoringCommitState::Presented ||
                creationStatus.mResult != 0 || !resolvedCreated ||
                resolvedCreatedId != mCreatedDrawingId || !resolvedMapping ||
                mappedDrawingId != mCreatedDrawingId)
            {
                mMeasured = true;
                return;
            }
            mReferencedDeletionRejected = player->QueueDrawingDeletion(
                mDocumentId, static_cast<uint32_t>(mLayerId), mCreatedDrawingId) == -6;
            const int32_t restoreResult = player->QueueFrameMapping(
                mDocumentId, static_cast<uint32_t>(mLayerId), 0, mOriginalDrawingId);
            const int32_t deletionResult = restoreResult == 0 ?
                player->QueueDrawingDeletion(
                    mDocumentId, static_cast<uint32_t>(mLayerId), mCreatedDrawingId) : -1;
            const uint64_t restoreRevision = restoreResult == 0 && deletionResult == 0 ?
                player->CommitEdits(mDocumentId) : 0;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_FRAME_RESTORE] frame=%llu drawing=%llu mappingResult=%d deletionResult=%d revision=%llu",
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(mOriginalDrawingId), restoreResult, deletionResult,
                static_cast<unsigned long long>(restoreRevision));
            mDeletionQueued = true;
            mDeletionFrame = frame;
            mDeletionRevision = restoreRevision;
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
        const int32_t mappingResult = geometryResult == 0 ? player->QueueFrameMapping(
            mDocumentId, static_cast<uint32_t>(mLayerId), 0, createdDrawingId) : geometryResult;
        const uint64_t creationRevision = mappingResult == 0 ?
            player->CommitEdits(mDocumentId) : 0;
        log->Printf(LT_MESSAGE,
            L"[IMM_LIVE_EDIT_CREATE] frame=%llu drawing=%llu createResult=%d geometryResult=%d mappingResult=%d revision=%llu",
            static_cast<unsigned long long>(frame),
            static_cast<unsigned long long>(createdDrawingId), createResult, geometryResult,
            mappingResult, static_cast<unsigned long long>(creationRevision));
        mCreationQueued = true;
        mCreationFrame = frame;
        mCreationRevision = creationRevision;
        mCreatedDrawingId = createdDrawingId;
    }
}
