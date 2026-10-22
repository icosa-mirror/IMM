#include "liveEditValidation.h"

#include "viewer.h"
#include "libImmImporter/src/document/layer.h"
#include "libImmImporter/src/document/layerPaint/element.h"
#include "libImmPlayer/src/player.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <string>
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

    template <typename Bounds> static bool iSubtreeBoxesMatch(const Bounds & a, const Bounds & b)
    {
        return std::fabs(a.mMinX - b.mMinX) < 0.000001 && std::fabs(a.mMinY - b.mMinY) < 0.000001 &&
               std::fabs(a.mMinZ - b.mMinZ) < 0.000001 && std::fabs(a.mMaxX - b.mMaxX) < 0.000001 &&
               std::fabs(a.mMaxY - b.mMaxY) < 0.000001 && std::fabs(a.mMaxZ - b.mMaxZ) < 0.000001;
    }

    void LiveEditValidation::TickPaintCreation(Viewer & viewer, ImmCore::piLog * log, uint64_t frame,
        uint64_t requestedFrame)
    {
        if (mMeasured || frame < requestedFrame)
            return;
        const auto fail = [&](const wchar_t * reason) {
            log->Printf(LT_ERROR, L"[IMM_LIVE_PAINT_CREATE] pass=0 stage=%d reason=%s", mPaintCreationStage, reason);
            mMeasured = true;
        };
        if (frame - requestedFrame > 10000)
        {
            fail(L"timeout");
            return;
        }
        ImmPlayer::Player * player = viewer.GetPlayer();
        if (mPaintCreationStage == 4)
        {
            ImmPlayer::Player::DocumentState state;
            player->GetDocumentState(state, mDocumentId);
            if (state.mLoadingState == ImmPlayer::Player::LoadingState::Failed)
            {
                fail(L"pending creation unload failed");
                return;
            }
            if (state.mLoadingState != ImmPlayer::Player::LoadingState::Unloaded)
                return;
            bool editing = false;
            const bool queried = player->GetEditingState(mDocumentId, editing);
            ImmPlayer::Document::AuthoringRevisions revisions;
            ImmPlayer::Document::AuthoringCommitStatus status;
            mPaintCreationSessionInvalid = (!queried || !editing) &&
                !player->GetAuthoringRevisions(mDocumentId, revisions) &&
                !player->GetAuthoringCommitStatus(mDocumentId, mRevision, status);
            if (!mPaintCreationSessionInvalid)
            {
                fail(L"unloaded authoring session remains accessible");
                return;
            }
            // Validate the old session before Load can reuse its numeric document slot.
            mPaintCreationReloadDocument = player->Load(L"nested-paint.imm");
            if (mPaintCreationReloadDocument < 0)
            {
                fail(L"fixture reload failed");
                return;
            }
            mPaintCreationStage = 5;
            mStandaloneTerminalFrame = 0;
            return;
        }
        if (mPaintCreationStage == 5)
        {
            ImmPlayer::Player::DocumentState state;
            player->GetDocumentState(state, mPaintCreationReloadDocument);
            if (state.mLoadingState == ImmPlayer::Player::LoadingState::Failed)
            {
                fail(L"reloaded fixture failed");
                return;
            }
            if (state.mLoadingState != ImmPlayer::Player::LoadingState::Loaded)
                return;
            const auto & before = mStandaloneLayers[0];
            bool hierarchy = player->GetLayerCount(mPaintCreationReloadDocument) == mStandaloneLayerCount;
            bool siblingStable = false;
            for (int index = 0; index < player->GetLayerCount(mPaintCreationReloadDocument); index++)
            {
                ImmPlayer::Player::LayerInfo info;
                if (!player->GetLayerInfoByIndex(mPaintCreationReloadDocument, index, info))
                {
                    fail(L"reloaded layer enumeration failed");
                    return;
                }
                if (std::wstring(info.name) == L"LiveCreatedPaint")
                    hierarchy = false;
                if (info.type == static_cast<int>(ImmImporter::Layer::Type::Paint) &&
                    (!info.isLoaded || info.paintNumStrokes < info.paintNumDrawings))
                    return;
                if (std::wstring(info.name) != L"KeepPaint")
                    continue;
                siblingStable = info.type == static_cast<int>(ImmImporter::Layer::Type::Paint) &&
                    info.paintNumStrokes == before.strokes &&
                    info.paintNumDrawings == static_cast<int>(before.boxes.size()) &&
                    info.hasBBox == before.hasBox && (!before.hasBox || iSubtreeBoxesMatch(info.bbox, before.box));
                for (size_t drawing = 0; drawing < before.boxes.size(); drawing++)
                {
                    ImmCore::bound3 box;
                    siblingStable = siblingStable &&
                        player->GetDrawingBBox(mPaintCreationReloadDocument, info.id, static_cast<int>(drawing), box) &&
                        iSubtreeBoxesMatch(box, before.boxes[drawing]);
                }
            }
            const bool bounds = iSubtreeBoxesMatch(
                player->GetDocumentBBox(mPaintCreationReloadDocument), mStandaloneBoxBefore);
            if (!hierarchy || !siblingStable || !bounds)
            {
                fail(L"reload changed fixture hierarchy or bounds");
                return;
            }
            if (mStandaloneTerminalFrame == 0)
                mStandaloneTerminalFrame = frame;
            if (frame < mStandaloneTerminalFrame + 3)
                return;
            ImmPlayer::Document::AuthoringRevisions fresh;
            const bool freshSession = player->AttachEditing(mPaintCreationReloadDocument) &&
                player->GetAuthoringRevisions(mPaintCreationReloadDocument, fresh) &&
                fresh.mRequested == 0 && fresh.mPrepared == 0 && fresh.mPresented == 0;
            mStandaloneSuccessful = mPaintCreationSessionInvalid && freshSession;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_PAINT_CREATE] pass=%d stage=5 unloaded=1 reloaded=1 sessionInvalid=%d freshSession=%d hierarchy=1 siblingStable=1 bounds=1",
                mStandaloneSuccessful ? 1 : 0, mPaintCreationSessionInvalid ? 1 : 0, freshSession ? 1 : 0);
            mMeasured = true;
            return;
        }
        const int doc = viewer.GetPrimaryDocumentId();
        if (doc < 0 || !viewer.IsDocumentLoaded(doc))
            return;
        std::vector<ImmPlayer::Player::LayerInfo> infos;
        for (int index = 0; index < player->GetLayerCount(doc); index++)
        {
            ImmPlayer::Player::LayerInfo info;
            if (!player->GetLayerInfoByIndex(doc, index, info))
            {
                fail(L"layer enumeration failed");
                return;
            }
            infos.push_back(info);
        }
        const auto findInfo = [&](int id) -> const ImmPlayer::Player::LayerInfo * {
            for (const auto & info : infos)
                if (info.id == id)
                    return &info;
            return nullptr;
        };
        if (!mApplied)
        {
            int parent = -1;
            const ImmPlayer::Player::LayerInfo * sibling = nullptr;
            for (const auto & info : infos)
            {
                if (std::wstring(info.name) == L"DeleteTarget")
                {
                    if (parent >= 0)
                    {
                        fail(L"duplicate parent");
                        return;
                    }
                    parent = info.id;
                }
                if (std::wstring(info.name) == L"KeepPaint")
                {
                    if (sibling != nullptr)
                    {
                        fail(L"duplicate sibling");
                        return;
                    }
                    sibling = &info;
                }
                if (info.type == static_cast<int>(ImmImporter::Layer::Type::Paint) &&
                    (!info.isLoaded || info.paintNumDrawings <= 0 || info.paintNumStrokes < info.paintNumDrawings))
                    return;
            }
            if (parent < 0 || sibling == nullptr ||
                sibling->type != static_cast<int>(ImmImporter::Layer::Type::Paint) ||
                !player->AttachEditing(doc))
            {
                fail(L"fixture or attach failed");
                return;
            }
            mDocumentId = doc;
            mStandaloneTarget = parent;
            mStandaloneLayerCount = static_cast<int>(infos.size());
            mStandaloneBoxBefore = player->GetDocumentBBox(doc);
            mStandaloneExpectRejection = std::getenv("IMM_LIVE_EDIT_FAIL_PREPARE_CPU") != nullptr ||
                std::getenv("IMM_LIVE_EDIT_FAIL_PREPARE_GPU") != nullptr ||
                std::getenv("IMM_LIVE_EDIT_FAIL_PRESENT") != nullptr;
            SubtreeLayerSnapshot snapshot;
            snapshot.id = sibling->id;
            snapshot.parent = sibling->parentId;
            snapshot.child = sibling->childIndex;
            snapshot.strokes = sibling->paintNumStrokes;
            snapshot.hasBox = sibling->hasBBox;
            snapshot.box = sibling->bbox;
            for (int drawing = 0; drawing < sibling->paintNumDrawings; drawing++)
            {
                uint64_t handle = 0;
                ImmCore::bound3 box;
                if (!player->GetDrawingHandle(doc, sibling->id, drawing, handle) ||
                    !player->GetDrawingBBox(doc, sibling->id, drawing, box))
                {
                    fail(L"sibling snapshot failed");
                    return;
                }
                snapshot.handles.push_back(handle);
                snapshot.boxes.push_back(box);
            }
            mStandaloneLayers.push_back(std::move(snapshot));
            ImmPlayer::Document::AuthoringRevisions revisions;
            if (!player->GetAuthoringRevisions(doc, revisions))
            {
                fail(L"revision snapshot failed");
                return;
            }
            mStandaloneOldRevision = revisions.mPresented;
            uint32_t created = ~0u;
            if (player->QueuePaintLayerCreation(doc, parent, L"LiveCreatedPaint", created) != 0)
            {
                fail(L"creation queue failed");
                return;
            }
            mPaintCreationLayer = static_cast<int>(created);
            uint64_t unpublished = 0;
            if (findInfo(mPaintCreationLayer) != nullptr || player->GetLayerCount(doc) != mStandaloneLayerCount ||
                player->GetDrawingHandle(doc, mPaintCreationLayer, 0, unpublished) ||
                player->GetFrameDrawingHandle(doc, mPaintCreationLayer, 0, unpublished))
            {
                fail(L"queued layer visible before commit");
                return;
            }
            int32_t result = -99;
            mRevision = player->CommitEdits(doc, &result);
            if (mRevision == 0 || result != 0)
            {
                fail(L"creation commit failed");
                return;
            }
            mPaintCreationStage = 1;
            mApplied = true;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_PAINT_CREATE] queued=1 stage=1 layer=%d revision=%llu unpublished=1 expectRejected=%d",
                mPaintCreationLayer, static_cast<unsigned long long>(mRevision), mStandaloneExpectRejection ? 1 : 0);
            return;
        }
        ImmPlayer::Document::AuthoringCommitStatus status;
        ImmPlayer::Document::AuthoringRevisions revisions;
        if (!player->GetAuthoringCommitStatus(doc, mRevision, status) ||
            !player->GetAuthoringRevisions(doc, revisions) || revisions.mRequested != mRevision ||
            (revisions.mPrepared != mStandaloneOldRevision && revisions.mPrepared != mRevision) ||
            (revisions.mPresented != mStandaloneOldRevision && revisions.mPresented != mRevision))
        {
            fail(L"unexpected revision history");
            return;
        }
        const auto & before = mStandaloneLayers[0];
        const auto * sibling = findInfo(before.id);
        bool siblingStable = sibling != nullptr && sibling->parentId == before.parent &&
            sibling->childIndex == before.child && sibling->paintNumStrokes == before.strokes &&
            sibling->paintNumDrawings == static_cast<int>(before.handles.size()) &&
            sibling->hasBBox == before.hasBox && (!before.hasBox || iSubtreeBoxesMatch(sibling->bbox, before.box));
        for (size_t drawing = 0; drawing < before.handles.size(); drawing++)
        {
            uint64_t handle = 0;
            ImmCore::bound3 box;
            siblingStable = siblingStable &&
                player->GetDrawingHandle(doc, before.id, static_cast<int>(drawing), handle) &&
                handle == before.handles[drawing] &&
                player->GetDrawingBBox(doc, before.id, static_cast<int>(drawing), box) &&
                iSubtreeBoxesMatch(box, before.boxes[drawing]);
        }
        if (!siblingStable)
        {
            fail(L"sibling changed");
            return;
        }
        const bool rejected = status.mState == ImmPlayer::Document::AuthoringCommitState::Rejected;
        const bool presented = status.mState == ImmPlayer::Document::AuthoringCommitState::Presented;
        if (!rejected && !presented)
        {
            if (revisions.mPresented != mStandaloneOldRevision)
            {
                fail(L"pending commit changed presented revision");
                return;
            }
            if (mPaintCreationUnload && mPaintCreationStage == 1 &&
                status.mState == ImmPlayer::Document::AuthoringCommitState::Preparing)
            {
                uint64_t handle = 0;
                if (findInfo(mPaintCreationLayer) != nullptr ||
                    static_cast<int>(infos.size()) != mStandaloneLayerCount ||
                    player->GetDrawingHandle(doc, mPaintCreationLayer, 0, handle) ||
                    !iSubtreeBoxesMatch(player->GetDocumentBBox(doc), mStandaloneBoxBefore))
                {
                    fail(L"pending creation changed public fixture");
                    return;
                }
                log->Printf(LT_MESSAGE,
                    L"[IMM_LIVE_PAINT_CREATE] pendingUnload=1 stage=1 status=%d revision=%llu unpublished=1 siblingStable=1 bounds=1",
                    static_cast<int>(status.mState), static_cast<unsigned long long>(mRevision));
                player->Unload(doc);
                mPaintCreationStage = 4;
            }
            return;
        }
        if (mPaintCreationUnload)
        {
            fail(L"creation reached terminal state before pending unload");
            return;
        }
        if (mStandaloneTerminalFrame == 0)
            mStandaloneTerminalFrame = frame;
        // Leave time for deferred GPU resource retirement before the next edit or capture.
        if (frame < mStandaloneTerminalFrame + 3)
            return;
        const auto * created = findInfo(mPaintCreationLayer);
        uint64_t handle = 0, mapped = 0;
        const bool hasHandle = player->GetDrawingHandle(doc, mPaintCreationLayer, 0, handle);
        const bool hasFrame = player->GetFrameDrawingHandle(doc, mPaintCreationLayer, 0, mapped);
        const bool restoredBounds = iSubtreeBoxesMatch(player->GetDocumentBBox(doc), mStandaloneBoxBefore);
        if (mStandaloneExpectRejection || rejected)
        {
            const bool hierarchy = created == nullptr && static_cast<int>(infos.size()) == mStandaloneLayerCount;
            const bool revision = revisions.mPresented == mStandaloneOldRevision;
            mStandaloneSuccessful = mPaintCreationStage == 1 && mStandaloneExpectRejection && rejected &&
                status.mResult == -10 && hierarchy && !hasHandle && !hasFrame && restoredBounds && revision;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_PAINT_CREATE] pass=%d stage=%d rejected=%d status=%d result=%d hierarchy=%d handles=%d siblingStable=1 bounds=%d revisions=%d",
                mStandaloneSuccessful ? 1 : 0, mPaintCreationStage, rejected ? 1 : 0,
                static_cast<int>(status.mState), status.mResult, hierarchy ? 1 : 0,
                !hasHandle && !hasFrame ? 1 : 0, restoredBounds ? 1 : 0, revision ? 1 : 0);
            mMeasured = true;
            return;
        }
        if (revisions.mPresented != mRevision || revisions.mPrepared != mRevision || status.mResult != 0)
        {
            fail(L"terminal publication revision mismatch");
            return;
        }
        if (mPaintCreationStage == 3)
        {
            const bool hierarchy = created == nullptr && static_cast<int>(infos.size()) == mStandaloneLayerCount;
            const bool staleRejected = !hasHandle && !hasFrame &&
                player->QueueDrawingDeletion(doc, mPaintCreationLayer, mPaintCreationHandle) == -1;
            mStandaloneSuccessful = hierarchy && staleRejected && restoredBounds;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_PAINT_CREATE] pass=%d stage=3 rejected=0 status=%d result=%d hierarchy=%d handles=%d siblingStable=1 bounds=%d revisions=1",
                mStandaloneSuccessful ? 1 : 0, static_cast<int>(status.mState), status.mResult,
                hierarchy ? 1 : 0, staleRejected ? 1 : 0, restoredBounds ? 1 : 0);
            mMeasured = true;
            return;
        }
        ImmPlayer::Player::LayerDiagnostics diagnostics;
        const bool properties = created != nullptr && created->type == static_cast<int>(ImmImporter::Layer::Type::Paint) &&
            created->parentId == mStandaloneTarget && created->paintNumDrawings == 1 && created->paintNumFrames == 1 &&
            created->isVisible == 1 && std::fabs(created->opacity - 1.0f) < 0.000001f &&
            std::wstring(created->name) == L"LiveCreatedPaint" &&
            player->GetLayerDiagnostics(doc, mPaintCreationLayer, diagnostics) &&
            diagnostics.canonicalVisible == 1 && std::fabs(diagnostics.canonicalOpacity - 1.0f) < 0.000001f &&
            iTransformsMatch(diagnostics.canonicalTransform, ImmCore::trans3d::identity());
        const bool handles = hasHandle && hasFrame && handle != 0 && mapped == handle &&
            (mPaintCreationStage == 1 || handle == mPaintCreationHandle);
        if (!properties || !handles || static_cast<int>(infos.size()) != mStandaloneLayerCount + 1)
        {
            fail(L"created layer properties or handles mismatch");
            return;
        }
        int32_t queued = -99;
        if (mPaintCreationStage == 1)
        {
            if (created->paintNumStrokes != 0 || created->hasBBox != 0 || !restoredBounds)
            {
                fail(L"empty creation changed geometry or bounds");
                return;
            }
            mPaintCreationHandle = handle;
            std::vector<ImmPlayer::Document::AuthoringElementGeometry> elements(1);
            auto & element = elements[0];
            element.mBrush = ImmImporter::Element::BrushSectionType::Circle;
            element.mVisibility = ImmImporter::Element::VisibilityType::Always;
            element.mPoints.resize(16);
            for (uint32_t index = 0; index < 16; index++)
            {
                const float t = static_cast<float>(index) / 15.0f;
                auto & point = element.mPoints[index];
                point.mPos = ImmCore::vec3(t * 0.5f, 1.5f, 0.0f);
                point.mNor = ImmCore::vec3(0.0f, 1.0f, 0.0f);
                point.mDir = ImmCore::vec3(0.0f, 0.0f, 1.0f);
                point.mCol = ImmCore::vec3(0.2f, 0.4f, 0.9f);
                point.mAlpha = 1.0f;
                point.mWidth = 0.01f;
                point.mLength = t;
                point.mTime = t;
            }
            queued = player->QueueDrawingGeometry(doc, mPaintCreationLayer, handle, std::move(elements),
                ImmImporter::Drawing::ColorSpace::Gamma, false, 0.01f);
        }
        else
        {
            ImmCore::bound3 box;
            if (created->paintNumStrokes != 1 || created->hasBBox != 1 ||
                !player->GetDrawingBBox(doc, mPaintCreationLayer, 0, box) ||
                std::fabs((box.mMinY + box.mMaxY) * 0.5f - 1.5f) > 0.0001f ||
                box.mMaxX - box.mMinX < 0.49f || !iSubtreeBoxesMatch(created->bbox, box))
            {
                fail(L"populated drawing bounds mismatch");
                return;
            }
            queued = player->QueueLayerDeletion(doc, mPaintCreationLayer);
        }
        log->Printf(LT_MESSAGE,
            L"[IMM_LIVE_PAINT_CREATE] pass=1 stage=%d rejected=0 status=%d result=%d hierarchy=1 handles=1 siblingStable=1 bounds=1 revisions=1 layer=%d drawing=%llu",
            mPaintCreationStage, static_cast<int>(status.mState), status.mResult, mPaintCreationLayer,
            static_cast<unsigned long long>(handle));
        mStandaloneOldRevision = mRevision;
        int32_t result = -99;
        mRevision = queued == 0 ? player->CommitEdits(doc, &result) : 0;
        if (mRevision == 0 || result != 0)
        {
            fail(L"next lifecycle queue/commit failed");
            return;
        }
        mPaintCreationStage++;
        mStandaloneTerminalFrame = 0;
    }

    void LiveEditValidation::TickSubtreeDeletion(Viewer & viewer, ImmCore::piLog * log, uint64_t frame,
                                                 uint64_t requestedFrame)
    {
        if (mMeasured || frame < requestedFrame)
            return;
        const auto fail = [&](const wchar_t * reason) {
            log->Printf(LT_ERROR, L"[IMM_LIVE_SUBTREE] pass=0 reason=%s", reason);
            mMeasured = true;
        };
        if (frame - requestedFrame > 10000)
        {
            fail(L"timeout");
            return;
        }
        ImmPlayer::Player * player = viewer.GetPlayer();
        const int doc = viewer.GetPrimaryDocumentId();
        if (doc < 0 || !viewer.IsDocumentLoaded(doc))
            return;
        std::vector<ImmPlayer::Player::LayerInfo> infos;
        for (int index = 0; index < player->GetLayerCount(doc); index++)
        {
            ImmPlayer::Player::LayerInfo info;
            if (!player->GetLayerInfoByIndex(doc, index, info))
            {
                fail(L"layer enumeration failed");
                return;
            }
            infos.push_back(info);
        }
        if (!mApplied)
        {
            int target = -1, sibling = -1;
            for (const auto & info : infos)
            {
                if (std::wstring(info.name) == L"DeleteTarget")
                {
                    if (target >= 0)
                    {
                        fail(L"duplicate target");
                        return;
                    }
                    target = info.id;
                }
                if (std::wstring(info.name) == L"KeepPaint")
                {
                    if (sibling >= 0)
                    {
                        fail(L"duplicate sibling");
                        return;
                    }
                    sibling = info.id;
                }
                if (info.type == static_cast<int>(ImmImporter::Layer::Type::Paint) &&
                    (!info.isLoaded || info.paintNumDrawings <= 0 || info.paintNumStrokes < info.paintNumDrawings))
                    return;
            }
            if (target < 0 || sibling < 0)
            {
                fail(L"named fixture layers absent");
                return;
            }
            const auto inTarget = [&](int id) {
                for (size_t depth = 0; depth <= infos.size(); depth++)
                {
                    if (id == target)
                        return true;
                    int parent = -1;
                    for (const auto & item : infos)
                        if (item.id == id)
                        {
                            parent = item.parentId;
                            break;
                        }
                    if (parent < 0 || parent == id)
                        return false;
                    id = parent;
                }
                return false;
            };
            if (inTarget(sibling))
            {
                fail(L"sibling belongs to target");
                return;
            }
            if (!player->AttachEditing(doc))
            {
                fail(L"attach failed");
                return;
            }
            mDocumentId = doc;
            mStandaloneTarget = target;
            mStandaloneLayerCount = static_cast<int>(infos.size());
            mStandaloneExpectRejection = std::getenv("IMM_LIVE_EDIT_FAIL_RETIREMENT") != nullptr;
            mStandaloneBoxBefore = player->GetDocumentBBox(doc);
            ImmPlayer::Document::AuthoringRevisions revisions;
            if (!player->GetAuthoringRevisions(doc, revisions))
            {
                fail(L"revision snapshot failed");
                return;
            }
            mStandaloneOldRevision = revisions.mPresented;
            int targetPaints = 0;
            for (const auto & info : infos)
            {
                const bool targetLayer = inTarget(info.id);
                if (!targetLayer && info.id != sibling)
                    continue;
                if (targetLayer)
                    mStandaloneTargetLayers++;
                SubtreeLayerSnapshot snapshot;
                snapshot.id = info.id;
                snapshot.parent = info.parentId;
                snapshot.child = info.childIndex;
                snapshot.strokes = info.paintNumStrokes;
                snapshot.target = targetLayer;
                snapshot.hasBox = info.hasBBox;
                snapshot.box = info.bbox;
                if (info.type == static_cast<int>(ImmImporter::Layer::Type::Paint))
                {
                    if (targetLayer)
                        targetPaints++;
                    for (int drawing = 0; drawing < info.paintNumDrawings; drawing++)
                    {
                        uint64_t handle = 0;
                        ImmCore::bound3 box;
                        if (!player->GetDrawingHandle(doc, info.id, drawing, handle) ||
                            !player->GetDrawingBBox(doc, info.id, drawing, box))
                        {
                            fail(L"drawing snapshot failed");
                            return;
                        }
                        snapshot.handles.push_back(handle);
                        snapshot.boxes.push_back(box);
                    }
                }
                else if (!targetLayer || info.type != static_cast<int>(ImmImporter::Layer::Type::Group))
                {
                    fail(L"unexpected fixture layer type");
                    return;
                }
                mStandaloneLayers.push_back(std::move(snapshot));
            }
            if (targetPaints < 2)
            {
                fail(L"fixture needs multiple target paints");
                return;
            }
            const int32_t queued = player->QueueLayerDeletion(doc, target);
            int32_t result = -99;
            mRevision = queued == 0 ? player->CommitEdits(doc, &result) : 0;
            if (mRevision == 0 || result != 0)
            {
                fail(L"deletion queue/commit failed");
                return;
            }
            mApplied = true;
            log->Printf(LT_MESSAGE,
                        L"[IMM_LIVE_SUBTREE] queued=1 revision=%llu targetLayers=%d targetPaints=%d expectRejected=%d",
                        static_cast<unsigned long long>(mRevision), mStandaloneTargetLayers, targetPaints,
                        mStandaloneExpectRejection ? 1 : 0);
            return;
        }
        ImmPlayer::Document::AuthoringCommitStatus status;
        if (!player->GetAuthoringCommitStatus(mDocumentId, mRevision, status))
        {
            fail(L"status missing");
            return;
        }
        ImmPlayer::Document::AuthoringRevisions observed;
        if (!player->GetAuthoringRevisions(mDocumentId, observed) || observed.mRequested != mRevision ||
            (observed.mPrepared != mStandaloneOldRevision && observed.mPrepared != mRevision) ||
            (observed.mPresented != mStandaloneOldRevision && observed.mPresented != mRevision))
        {
            fail(L"unexpected revision during publication");
            return;
        }
        if (status.mState != ImmPlayer::Document::AuthoringCommitState::Presented &&
            status.mState != ImmPlayer::Document::AuthoringCommitState::Rejected)
        {
            if (observed.mPresented != mStandaloneOldRevision)
                fail(L"pending commit changed presented revision");
            return;
        }
        const bool rejected = status.mState == ImmPlayer::Document::AuthoringCommitState::Rejected;
        if (mStandaloneTerminalFrame == 0)
            mStandaloneTerminalFrame = frame;
        // Let deferred drawing retirement advance before capture/exit.
        if (frame < mStandaloneTerminalFrame + 3)
            return;
        bool hierarchy = true, handles = true, siblingStable = true;
        int targetDrawings = 0;
        for (const auto & before : mStandaloneLayers)
        {
            const ImmPlayer::Player::LayerInfo * after = nullptr;
            for (const auto & info : infos)
                if (info.id == before.id)
                {
                    after = &info;
                    break;
                }
            const bool mustExist = !before.target || rejected;
            if ((after != nullptr) != mustExist)
                hierarchy = false;
            if (after &&
                (after->hasBBox != before.hasBox || (before.hasBox && !iSubtreeBoxesMatch(after->bbox, before.box))))
            {
                hierarchy = false;
                if (!before.target)
                    siblingStable = false;
            }
            if (after && (after->parentId != before.parent || after->childIndex != before.child ||
                          after->paintNumStrokes != before.strokes ||
                          after->paintNumDrawings != static_cast<int>(before.handles.size())))
            {
                // Groups have no drawings; sibling child index may compact after target removal.
                if (!(!before.target && !rejected && after->parentId == before.parent &&
                      after->paintNumStrokes == before.strokes &&
                      after->paintNumDrawings == static_cast<int>(before.handles.size())))
                    hierarchy = false;
            }
            for (size_t drawing = 0; drawing < before.handles.size(); drawing++)
            {
                uint64_t handle = 0;
                ImmCore::bound3 box;
                const bool exists = player->GetDrawingHandle(mDocumentId, before.id, static_cast<int>(drawing), handle);
                if (before.target)
                    targetDrawings++;
                if (mustExist)
                {
                    const bool stable =
                        exists && handle == before.handles[drawing] &&
                        player->GetDrawingBBox(mDocumentId, before.id, static_cast<int>(drawing), box) &&
                        iSubtreeBoxesMatch(box, before.boxes[drawing]);
                    handles = handles && stable;
                    if (!before.target)
                        siblingStable = siblingStable && stable;
                }
                else if (exists || player->QueueDrawingDeletion(mDocumentId, before.id, before.handles[drawing]) != -1)
                    handles = false;
            }
        }
        const auto box = player->GetDocumentBBox(mDocumentId);
        const bool bounds = rejected ? iSubtreeBoxesMatch(box, mStandaloneBoxBefore)
                                     : box.mMaxX < mStandaloneBoxBefore.mMaxX - 1.0 &&
                                           std::fabs(box.mMinX - mStandaloneBoxBefore.mMinX) < 0.000001;
        const bool count =
            static_cast<int>(infos.size()) == mStandaloneLayerCount - (rejected ? 0 : mStandaloneTargetLayers);
        ImmPlayer::Document::AuthoringRevisions revisions;
        const bool revision = player->GetAuthoringRevisions(mDocumentId, revisions) &&
                              revisions.mRequested == mRevision &&
                              revisions.mPresented == (rejected ? mStandaloneOldRevision : mRevision) &&
                              (revisions.mPrepared == mStandaloneOldRevision || revisions.mPrepared == mRevision);
        mStandaloneSuccessful = rejected == mStandaloneExpectRejection && (!rejected || status.mResult == -5) &&
                                hierarchy && handles && siblingStable && bounds && count && revision;
        log->Printf(
            LT_MESSAGE,
            L"[IMM_LIVE_SUBTREE] pass=%d rejected=%d status=%d result=%d hierarchy=%d handles=%d siblingStable=%d bounds=%d count=%d revisions=%d targetDrawings=%d",
            mStandaloneSuccessful ? 1 : 0, rejected ? 1 : 0, static_cast<int>(status.mState), status.mResult,
            hierarchy ? 1 : 0, handles ? 1 : 0, siblingStable ? 1 : 0, bounds ? 1 : 0, count ? 1 : 0, revision ? 1 : 0,
            targetDrawings);
        mMeasured = true;
    }


    void LiveEditValidation::TickBenchmark(Viewer & viewer, ImmCore::piLog * log,
        uint64_t frame, uint64_t requestedFrame)
    {
        if (mMeasured || frame < requestedFrame) return;
        ImmPlayer::Player* player = viewer.GetPlayer();
        const auto fail = [&](const wchar_t* reason)
        {
            log->Printf(LT_ERROR, L"[IMM_LIVE_BENCHMARK_ERROR] frame=%llu iteration=%u %ls",
                static_cast<unsigned long long>(frame), mBenchmarkIteration, reason);
            mMeasured = true;
        };
        if (mDocumentId < 0)
        {
            const int docId = viewer.GetPrimaryDocumentId();
            if (docId < 0 || !viewer.IsDocumentLoaded(docId)) return;
            uint64_t drawings = 0, frames = 0, loadedStrokes = 0;
            uint32_t paintLayers = 0;
            for (int i = 0; i < player->GetLayerCount(docId); ++i)
            {
                ImmPlayer::Player::LayerInfo info;
                if (player->GetLayerInfoByIndex(docId, i, info) &&
                    info.type == static_cast<int>(ImmImporter::Layer::Type::Paint))
                {
                    if (mLayerId < 0) mLayerId = info.id;
                    ++paintLayers;
                    drawings += static_cast<uint64_t>(info.paintNumDrawings);
                    frames += static_cast<uint64_t>(info.paintNumFrames);
                    loadedStrokes += static_cast<uint64_t>(info.paintNumStrokes);
                }
            }
            // These benchmark fixtures contain exactly one stroke per drawing.
            // IsDocumentLoaded only guarantees the initial five-second buffer;
            // let the importer finish its asynchronous remainder before editing.
            if (drawings == 0 || loadedStrokes != drawings) return;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_BENCHMARK_CORPUS] paintLayers=%u drawings=%llu frames=%llu loadedStrokes=%llu",
                paintLayers, static_cast<unsigned long long>(drawings), static_cast<unsigned long long>(frames),
                static_cast<unsigned long long>(loadedStrokes));
            if (mLayerId < 0 || !player->AttachEditing(docId) ||
                !player->GetDrawingHandle(docId, mLayerId, 0, mOriginalDrawingId))
            { fail(L"attach or first drawing lookup failed"); return; }
            mDocumentId = docId;
            // Preserve references to every drawing while measuring a stable active
            // drawing. Replacing every frame mapping with zero would shrink CPU residency.
            player->Pause(docId);
            return;
        }
        if (!mBenchmarkSeeked)
        {
            // Pause is queued; seek only after the next frame has consumed it.
            player->SetTime(mDocumentId, ImmCore::piTick(0), ImmCore::piTick(1));
            mBenchmarkSeeked = true;
            return;
        }
        ImmCore::piTick position(0), stopped(0);
        player->GetTime(mDocumentId, &position, &stopped);
        if (position != ImmCore::piTick(0) || stopped <= ImmCore::piTick(0))
        { fail(L"benchmark playback did not remain held at frame zero"); return; }
        if (!mBenchmarkPlaybackReady)
        {
            mBenchmarkPlaybackReady = true;
            log->Printf(LT_MESSAGE, L"[IMM_LIVE_BENCHMARK_PLAYBACK] position=0 pausedFrame=0");
        }
        ImmPlayer::Document::AuthoringRevisions revisions;
        if (!player->GetAuthoringRevisions(mDocumentId, revisions))
        { fail(L"revision query failed"); return; }
        if (mBenchmarkPending)
        {
            ImmPlayer::Document::AuthoringCommitStatus status;
            if (!player->GetAuthoringCommitStatus(mDocumentId, mRevision, status) ||
                revisions.mRequested != mRevision ||
                (revisions.mPrepared != mBenchmarkPreviousRevision && revisions.mPrepared != mRevision) ||
                (revisions.mPresented != mBenchmarkPreviousRevision && revisions.mPresented != mRevision))
            { fail(L"unexpected revision history"); return; }
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_BENCHMARK_FRAME] frame=%llu requested=%llu prepared=%llu presented=%llu",
                static_cast<unsigned long long>(frame), static_cast<unsigned long long>(revisions.mRequested),
                static_cast<unsigned long long>(revisions.mPrepared), static_cast<unsigned long long>(revisions.mPresented));
            if (status.mState == ImmPlayer::Document::AuthoringCommitState::Rejected)
            { fail(L"commit rejected"); return; }
            if (status.mState != ImmPlayer::Document::AuthoringCommitState::Presented) return;
            const bool timingValid = status.mCommitStartedNs != 0 &&
                status.mPreparingNs >= status.mCommitStartedNs && status.mPreparedNs >= status.mPreparingNs &&
                status.mPresentedNs >= status.mPreparedNs && status.mRejectedNs == 0;
            ImmCore::bound3 box;
            const float targetY = 1.5f + (mBenchmarkIteration % 2 == 0 ? 0.02f : 0.0f);
            if (!timingValid || revisions.mPresented != mRevision || revisions.mPrepared != mRevision ||
                !player->GetDrawingBBox(mDocumentId, mLayerId, 0, box) ||
                std::fabs((box.mMinY + box.mMaxY) * 0.5f - targetY) > 0.0001f)
            { fail(L"publication timing or drawing bounds mismatch"); return; }
            const auto elapsedMs = [&](uint64_t end)
            { return static_cast<double>(end - status.mCommitStartedNs) / 1000000.0; };
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_BENCHMARK] iteration=%u sample=%d warmup=%d points=%u layer=%d revision=%llu status=4 queueMs=%.6f commitCallMs=%.6f commitToPreparingMs=%.6f commitToFirstPreparedMs=%.6f commitToPresentedMs=%.6f",
                mBenchmarkIteration, static_cast<int>(mBenchmarkIteration) - static_cast<int>(mBenchmarkWarmup),
                mBenchmarkIteration < mBenchmarkWarmup ? 1 : 0, mBenchmarkPoints, mLayerId,
                static_cast<unsigned long long>(mRevision), mBenchmarkQueueMs, mBenchmarkCommitCallMs,
                elapsedMs(status.mPreparingNs), elapsedMs(status.mPreparedNs), elapsedMs(status.mPresentedNs));
            ++mBenchmarkIteration;
            mBenchmarkPending = false;
            if (mBenchmarkIteration == mBenchmarkWarmup + mBenchmarkSamples)
            {
                log->Printf(LT_MESSAGE, L"[IMM_LIVE_BENCHMARK_DONE] samples=%u warmup=%u points=%u",
                    mBenchmarkSamples, mBenchmarkWarmup, mBenchmarkPoints);
                mMeasured = true;
            }
            return; // A separate rendered frame precedes the next queue operation.
        }
        std::vector<ImmPlayer::Document::AuthoringElementGeometry> elements(1);
        auto& element = elements[0];
        element.mBrush = ImmImporter::Element::BrushSectionType::Circle;
        element.mVisibility = ImmImporter::Element::VisibilityType::Always;
        element.mPoints.resize(mBenchmarkPoints);
        for (uint32_t i = 0; i < mBenchmarkPoints; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(mBenchmarkPoints - 1);
            auto& point = element.mPoints[i];
            point.mPos = ImmCore::vec3(2.0f + t * 0.5f, 1.5f + (mBenchmarkIteration % 2 == 0 ? 0.02f : 0.0f), 0.0f);
            point.mNor = ImmCore::vec3(0.0f, 1.0f, 0.0f);
            point.mDir = ImmCore::vec3(0.0f, 0.0f, 1.0f);
            point.mCol = ImmCore::vec3(0.2f, 0.4f, 0.9f);
            point.mAlpha = 1.0f; point.mWidth = 0.01f; point.mLength = t; point.mTime = t;
        }
        using Clock = std::chrono::steady_clock;
        const auto begin = Clock::now();
        const int result = player->QueueDrawingGeometry(mDocumentId, static_cast<uint32_t>(mLayerId),
            mOriginalDrawingId, std::move(elements), ImmImporter::Drawing::ColorSpace::Gamma, false, 0.01f);
        const auto queued = Clock::now();
        mBenchmarkPreviousRevision = revisions.mPresented;
        mRevision = result == 0 ? player->CommitEdits(mDocumentId) : 0;
        const auto committed = Clock::now();
        if (mRevision == 0) { fail(L"queue or commit failed"); return; }
        mBenchmarkQueueMs = std::chrono::duration<double, std::milli>(queued - begin).count();
        mBenchmarkCommitCallMs = std::chrono::duration<double, std::milli>(committed - queued).count();
        mBenchmarkPending = true;
    }

    void LiveEditValidation::Tick(Viewer & viewer, ImmCore::piLog * log,
        uint64_t frame, bool enabled, uint64_t requestedFrame)
    {
        if (!enabled || log == nullptr)
            return;

        if (!mStandaloneConfigured)
        {
            mStandaloneConfigured = true;
            const char * value = std::getenv("IMM_VIEWER_LIVE_DELETE_SUBTREE");
            mStandaloneEnabled = value != nullptr && std::string(value) == "1";
            const char * paint = std::getenv("IMM_VIEWER_LIVE_CREATE_PAINT");
            mPaintCreationEnabled = paint != nullptr && std::string(paint) == "1";
            const char * unload = std::getenv("IMM_VIEWER_LIVE_CREATE_PAINT_UNLOAD");
            mPaintCreationUnload = unload != nullptr && std::string(unload) == "1";
            if (mStandaloneEnabled && mPaintCreationEnabled)
            {
                log->Printf(LT_ERROR, L"[IMM_LIVE_PAINT_CREATE] pass=0 stage=0 reason=conflicting standalone modes");
                mMeasured = true;
            }
            mStandaloneEnabled = mStandaloneEnabled || mPaintCreationEnabled;
        }
        if (mStandaloneEnabled)
        {
            if (mPaintCreationEnabled)
                TickPaintCreation(viewer, log, frame, requestedFrame);
            else
                TickSubtreeDeletion(viewer, log, frame, requestedFrame);
            return;
        }

        if (!mBenchmarkConfigured)
        {
            mBenchmarkConfigured = true;
            const char* points = std::getenv("IMM_VIEWER_LIVE_BENCHMARK_POINTS");
            mBenchmarkEnabled = points != nullptr && points[0] != 0;
            if (mBenchmarkEnabled)
            {
                const auto parse = [](const char* name, uint32_t fallback, uint32_t minimum, uint32_t maximum, uint32_t& out)
                {
                    const char* value = std::getenv(name);
                    if (value == nullptr || value[0] == 0) { out = fallback; return true; }
                    char* end = nullptr;
                    const unsigned long number = std::strtoul(value, &end, 10);
                    if (end == value || *end != 0 || number < minimum || number > maximum) return false;
                    out = static_cast<uint32_t>(number);
                    return true;
                };
                if (!parse("IMM_VIEWER_LIVE_BENCHMARK_POINTS", 0, 2, 4096, mBenchmarkPoints) ||
                    !parse("IMM_VIEWER_LIVE_BENCHMARK_SAMPLES", 30, 1, 1000, mBenchmarkSamples) ||
                    !parse("IMM_VIEWER_LIVE_BENCHMARK_WARMUP", 5, 0, 1000, mBenchmarkWarmup))
                {
                    log->Printf(LT_ERROR, L"[IMM_LIVE_BENCHMARK_ERROR] invalid benchmark configuration");
                    mMeasured = true;
                }
            }
        }
        if (mBenchmarkEnabled)
        {
            TickBenchmark(viewer, log, frame, requestedFrame);
            return;
        }
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

        if (mGroupLayerDeletionQueued)
        {
            if (frame < mGroupLayerDeletionFrame + 3)
                return;
            mMeasured = true;
            ImmPlayer::Document::AuthoringCommitStatus deletionStatus;
            const bool hasDeletionStatus = mGroupLayerDeletionRevision != 0 &&
                player->GetAuthoringCommitStatus(
                    mDocumentId, mGroupLayerDeletionRevision, deletionStatus);
            bool subtreeMissing = true;
            for (int i = 0; i < player->GetLayerCount(mDocumentId); i++)
            {
                ImmPlayer::Player::LayerInfo info;
                if (player->GetLayerInfoByIndex(mDocumentId, i, info) &&
                    (info.id == mCreatedGroupLayerId ||
                        info.id == mCreatedGroupChildLayerId))
                {
                    subtreeMissing = false;
                    break;
                }
            }
            const bool countRestored = player->GetLayerCount(mDocumentId) ==
                mLayerCountBeforeGroupCreation;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_LAYER_DESTROY] frame=%llu revision=%llu status=%d result=%d subtreeMissing=%d countRestored=%d",
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(mGroupLayerDeletionRevision),
                hasDeletionStatus ? static_cast<int>(deletionStatus.mState) : -1,
                hasDeletionStatus ? deletionStatus.mResult : -1,
                subtreeMissing ? 1 : 0, countRestored ? 1 : 0);
            return;
        }

        if (mGroupLayerCrossParentQueued)
        {
            if (frame < mGroupLayerCrossParentFrame + 3)
                return;
            ImmPlayer::Document::AuthoringCommitStatus reparentStatus;
            const bool hasReparentStatus = mGroupLayerCrossParentRevision != 0 &&
                player->GetAuthoringCommitStatus(
                    mDocumentId, mGroupLayerCrossParentRevision, reparentStatus);
            bool hierarchyMatch = false;
            std::wstring expectedFullName;
            for (int i = 0; i < player->GetLayerCount(mDocumentId); i++)
            {
                ImmPlayer::Player::LayerInfo info;
                if (!player->GetLayerInfoByIndex(mDocumentId, i, info))
                    continue;
                if (info.id == mGroupLayerReparentTargetId)
                    expectedFullName = std::wstring(info.fullName) + L"/LiveEditGroup";
            }
            for (int i = 0; i < player->GetLayerCount(mDocumentId); i++)
            {
                ImmPlayer::Player::LayerInfo info;
                if (player->GetLayerInfoByIndex(mDocumentId, i, info) &&
                    info.id == mCreatedGroupLayerId)
                {
                    hierarchyMatch = info.parentId == mGroupLayerReparentTargetId &&
                        info.childIndex == 0 && !expectedFullName.empty() &&
                        expectedFullName == info.fullName;
                    break;
                }
            }
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_LAYER_CROSS_PARENT] frame=%llu revision=%llu status=%d result=%d hierarchyMatch=%d",
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(mGroupLayerCrossParentRevision),
                hasReparentStatus ? static_cast<int>(reparentStatus.mState) : -1,
                hasReparentStatus ? reparentStatus.mResult : -1,
                hierarchyMatch ? 1 : 0);
            const bool reparentSucceeded = hasReparentStatus &&
                reparentStatus.mState == ImmPlayer::Document::AuthoringCommitState::Presented &&
                reparentStatus.mResult == 0 && hierarchyMatch;
            const int32_t deletionResult = reparentSucceeded ?
                player->QueueLayerDeletion(
                    mDocumentId, static_cast<uint32_t>(mCreatedGroupLayerId)) : -4;
            const uint64_t deletionRevision = deletionResult == 0 ?
                player->CommitEdits(mDocumentId) : 0;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_LAYER_DESTROY] frame=%llu layer=%d deleteResult=%d revision=%llu",
                static_cast<unsigned long long>(frame), mCreatedGroupLayerId,
                deletionResult, static_cast<unsigned long long>(deletionRevision));
            mGroupLayerDeletionQueued = true;
            mGroupLayerDeletionFrame = frame;
            mGroupLayerDeletionRevision = deletionRevision;
            return;
        }

        if (mGroupLayerReorderQueued)
        {
            if (frame < mGroupLayerReorderFrame + 3)
                return;
            ImmPlayer::Document::AuthoringCommitStatus reorderStatus;
            const bool hasReorderStatus = mGroupLayerReorderRevision != 0 &&
                player->GetAuthoringCommitStatus(
                    mDocumentId, mGroupLayerReorderRevision, reorderStatus);
            bool orderMatch = false;
            for (int i = 0; i < player->GetLayerCount(mDocumentId); i++)
            {
                ImmPlayer::Player::LayerInfo info;
                if (player->GetLayerInfoByIndex(mDocumentId, i, info) &&
                    info.id == mCreatedGroupLayerId)
                {
                    orderMatch = info.parentId == mCreatedGroupParentId &&
                        info.childIndex == 0;
                    break;
                }
            }
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_LAYER_REPARENT] frame=%llu revision=%llu status=%d result=%d orderMatch=%d",
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(mGroupLayerReorderRevision),
                hasReorderStatus ? static_cast<int>(reorderStatus.mState) : -1,
                hasReorderStatus ? reorderStatus.mResult : -1,
                orderMatch ? 1 : 0);
            const bool reorderSucceeded = hasReorderStatus &&
                reorderStatus.mState == ImmPlayer::Document::AuthoringCommitState::Presented &&
                reorderStatus.mResult == 0 && orderMatch;
            const int32_t reparentResult = reorderSucceeded ?
                player->QueueLayerReparent(mDocumentId,
                    static_cast<uint32_t>(mCreatedGroupLayerId),
                    static_cast<uint32_t>(mGroupLayerReparentTargetId), 0) : -4;
            const uint64_t reparentRevision = reparentResult == 0 ?
                player->CommitEdits(mDocumentId) : 0;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_LAYER_CROSS_PARENT] frame=%llu layer=%d parent=%d childIndex=0 reparentResult=%d revision=%llu",
                static_cast<unsigned long long>(frame), mCreatedGroupLayerId,
                mGroupLayerReparentTargetId, reparentResult,
                static_cast<unsigned long long>(reparentRevision));
            mGroupLayerCrossParentQueued = true;
            mGroupLayerCrossParentFrame = frame;
            mGroupLayerCrossParentRevision = reparentRevision;
            return;
        }

        if (mGroupLayerChildQueued)
        {
            if (frame < mGroupLayerChildFrame + 3)
                return;
            ImmPlayer::Document::AuthoringCommitStatus childStatus;
            const bool hasChildStatus = mGroupLayerChildRevision != 0 &&
                player->GetAuthoringCommitStatus(
                    mDocumentId, mGroupLayerChildRevision, childStatus);
            bool childMatch = false;
            for (int i = 0; i < player->GetLayerCount(mDocumentId); i++)
            {
                ImmPlayer::Player::LayerInfo info;
                if (player->GetLayerInfoByIndex(mDocumentId, i, info) &&
                    info.id == mCreatedGroupChildLayerId)
                {
                    childMatch = info.type == static_cast<int>(
                        ImmImporter::Layer::Type::Group) &&
                        info.parentId == mCreatedGroupLayerId;
                    break;
                }
            }
            const bool countChanged = player->GetLayerCount(mDocumentId) ==
                mLayerCountBeforeGroupCreation + 2;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_LAYER_CHILD_CREATE] frame=%llu revision=%llu status=%d result=%d childMatch=%d countChanged=%d",
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(mGroupLayerChildRevision),
                hasChildStatus ? static_cast<int>(childStatus.mState) : -1,
                hasChildStatus ? childStatus.mResult : -1,
                childMatch ? 1 : 0, countChanged ? 1 : 0);
            const bool childSucceeded = hasChildStatus &&
                childStatus.mState == ImmPlayer::Document::AuthoringCommitState::Presented &&
                childStatus.mResult == 0 && childMatch && countChanged;
            const int32_t reorderResult = childSucceeded ?
                player->QueueLayerReparent(mDocumentId,
                    static_cast<uint32_t>(mCreatedGroupLayerId),
                    static_cast<uint32_t>(mCreatedGroupParentId), 0) : -4;
            const uint64_t reorderRevision = reorderResult == 0 ?
                player->CommitEdits(mDocumentId) : 0;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_LAYER_REPARENT] frame=%llu layer=%d parent=%d childIndex=0 reparentResult=%d revision=%llu",
                static_cast<unsigned long long>(frame), mCreatedGroupLayerId,
                mCreatedGroupParentId, reorderResult,
                static_cast<unsigned long long>(reorderRevision));
            mGroupLayerReorderQueued = true;
            mGroupLayerReorderFrame = frame;
            mGroupLayerReorderRevision = reorderRevision;
            return;
        }

        if (mGroupLayerQueued)
        {
            if (frame < mGroupLayerFrame + 3)
                return;
            ImmPlayer::Document::AuthoringCommitStatus creationStatus;
            const bool hasCreationStatus = mGroupLayerRevision != 0 &&
                player->GetAuthoringCommitStatus(
                    mDocumentId, mGroupLayerRevision, creationStatus);
            bool layerMatch = false;
            for (int i = 0; i < player->GetLayerCount(mDocumentId); i++)
            {
                ImmPlayer::Player::LayerInfo info;
                if (player->GetLayerInfoByIndex(mDocumentId, i, info) &&
                    info.id == mCreatedGroupLayerId)
                {
                    layerMatch = info.type == static_cast<int>(
                        ImmImporter::Layer::Type::Group) &&
                        info.parentId == mCreatedGroupParentId;
                    break;
                }
            }
            const bool countChanged = player->GetLayerCount(mDocumentId) ==
                mLayerCountBeforeGroupCreation + 1;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_LAYER_CREATE] frame=%llu revision=%llu status=%d result=%d layerMatch=%d countChanged=%d",
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(mGroupLayerRevision),
                hasCreationStatus ? static_cast<int>(creationStatus.mState) : -1,
                hasCreationStatus ? creationStatus.mResult : -1,
                layerMatch ? 1 : 0, countChanged ? 1 : 0);
            const bool creationSucceeded = hasCreationStatus &&
                creationStatus.mState == ImmPlayer::Document::AuthoringCommitState::Presented &&
                creationStatus.mResult == 0 && layerMatch && countChanged;
            uint32_t childLayerId = 0;
            const int32_t childResult = creationSucceeded ?
                player->QueueGroupLayerCreation(mDocumentId,
                    static_cast<uint32_t>(mCreatedGroupLayerId),
                    L"LiveEditChildGroup", childLayerId) : -4;
            const uint64_t childRevision = childResult == 0 ?
                player->CommitEdits(mDocumentId) : 0;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_LAYER_CHILD_CREATE] frame=%llu parent=%d layer=%u createResult=%d revision=%llu",
                static_cast<unsigned long long>(frame), mCreatedGroupLayerId,
                childLayerId, childResult, static_cast<unsigned long long>(childRevision));
            mGroupLayerChildQueued = true;
            mGroupLayerChildFrame = frame;
            mGroupLayerChildRevision = childRevision;
            mCreatedGroupChildLayerId = static_cast<int>(childLayerId);
            return;
        }

        if (mSpawnAreaQueued)
        {
            if (frame < mSpawnAreaFrame + 3)
                return;
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
            const bool spawnSucceeded = hasSpawnStatus &&
                spawnStatus.mState == ImmPlayer::Document::AuthoringCommitState::Presented &&
                spawnStatus.mResult == 0 && spawnAreaMatch;
            int parentLayerId = -1;
            int reparentTargetId = -1;
            const int layerCount = player->GetLayerCount(mDocumentId);
            for (int i = 0; spawnSucceeded && i < layerCount; i++)
            {
                ImmPlayer::Player::LayerInfo info;
                if (player->GetLayerInfoByIndex(mDocumentId, i, info) &&
                    info.type == static_cast<int>(ImmImporter::Layer::Type::Group))
                {
                    if (parentLayerId < 0)
                        parentLayerId = info.id;
                    else
                    {
                        reparentTargetId = info.id;
                        break;
                    }
                }
            }
            if (parentLayerId < 0 || reparentTargetId < 0)
            {
                mMeasured = true;
                return;
            }
            uint32_t createdLayerId = 0;
            const int32_t createResult = player->QueueGroupLayerCreation(
                mDocumentId, static_cast<uint32_t>(parentLayerId),
                L"LiveEditGroup", createdLayerId);
            const uint64_t createRevision = createResult == 0 ?
                player->CommitEdits(mDocumentId) : 0;
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_EDIT_LAYER_CREATE] frame=%llu parent=%d layer=%u createResult=%d revision=%llu",
                static_cast<unsigned long long>(frame), parentLayerId, createdLayerId,
                createResult, static_cast<unsigned long long>(createRevision));
            mGroupLayerQueued = true;
            mGroupLayerFrame = frame;
            mGroupLayerRevision = createRevision;
            mCreatedGroupLayerId = static_cast<int>(createdLayerId);
            mCreatedGroupParentId = parentLayerId;
            mGroupLayerReparentTargetId = reparentTargetId;
            mLayerCountBeforeGroupCreation = layerCount;
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
        if (hasStatus)
        {
            const bool timingValid = status.mCommitStartedNs != 0 &&
                (status.mPreparingNs == 0 || status.mPreparingNs >= status.mCommitStartedNs) &&
                (status.mPreparedNs == 0 || (status.mPreparingNs != 0 && status.mPreparedNs >= status.mPreparingNs)) &&
                (status.mRejectedNs == 0 || status.mRejectedNs >= status.mCommitStartedNs) &&
                (status.mState != ImmPlayer::Document::AuthoringCommitState::Presented ||
                    (status.mPreparedNs != 0 && status.mPresentedNs >= status.mPreparedNs && status.mRejectedNs == 0));
            const auto elapsedMs = [&](uint64_t end) -> double
            {
                return status.mCommitStartedNs != 0 && end >= status.mCommitStartedNs ?
                    static_cast<double>(end - status.mCommitStartedNs) / 1000000.0 : -1.0;
            };
            log->Printf(LT_MESSAGE,
                L"[IMM_LIVE_TIMING] revision=%llu status=%d valid=%d commitToPreparingMs=%.6f commitToFirstPreparedMs=%.6f commitToPresentedMs=%.6f commitToRejectedMs=%.6f",
                static_cast<unsigned long long>(mRevision), static_cast<int>(status.mState), timingValid ? 1 : 0,
                elapsedMs(status.mPreparingNs), elapsedMs(status.mPreparedNs),
                elapsedMs(status.mPresentedNs), elapsedMs(status.mRejectedNs));
            if (!timingValid)
            {
                mMeasured = true;
                return;
            }
        }
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
