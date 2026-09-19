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
            return;
        }

        if (mMeasured || frame < mAppliedFrame + 3)
            return;

        mMeasured = true;
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
    }
}
