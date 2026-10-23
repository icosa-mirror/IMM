#pragma once
#include <atomic>
#include "spawnAreaState.h"

#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "libImmCore/src/libBasics/piMutex.h"

#include "layerRenderers/layerRendererPaint/layerRendererPaint.h"
#include "layerRenderers/layerRendererPicture/layerRendererPicture.h"
#include "layerRenderers/layerRendererSound/layerRendererSound.h"
#include "layerRenderers/layerRendererModel/layerRendererModel.h"
#include "mngrPlayer.h"
#include "renderMode.h"

#include "../../libImmImporter/src/document/sequence.h"
#include "../../libImmImporter/src/document/layerSpawnArea.h"
#include "../../libImmImporter/src/document/layerPaintStatic.h"

namespace ImmPlayer {

    // Applied before a document enters the asynchronous load/render pipeline.
    struct LoadOptions
    {
        bool initiallyHidden = false;
        bool suppress360Backdrops = false;
    };

    class Document
    {
    public:
        Document();
        ~Document();

        enum class LoadingState : int
        {
            LoadingPending = 0,
            LoadingCPU = 1,
            LoadingSPU = 7,
            LoadingGPU = 2,
            LoadingComplete = 10,
            Loaded = 3, // playing
            UnloadingGPU = 4,
            UnloadingSPU = 11,
            UnloadingCPU = 5,
            UnloadingCompleted = 12,
            StopingLoadingAsync = 13,
        };

        enum class ErrorState : int
        {
            NoError = 0,
            FailedCPU = 1,
            FailedSPU = 2,
            FailedGPU = 3
        };

        enum class PlaybackState : int
        {
            Playing = 0,		// playing (any case not below)
            Paused = 1,			// paused as requested per user, all animations and sounds stopped
            PausedAndHidden = 2,
            Waiting = 3,		// story waiting for input to proceed, but animations and sounds still playing
            Finished = 4		// story reached the end
        };

        enum class ImportType : int
        {
            IMM_disk = 1,
            IMM_memory = 2
        };

        struct SpawnAreaInfo
        {
            const wchar_t *mName;
            int mVersion;
            bool mIsFloorLevel;
            ImmCore::trans3d mSpawnAreaToWorld;
            ImmImporter::LayerSpawnArea::Volume mVolume;
            bool mAnimated = false;
        };

        struct Command
        {
            enum class Type : int
            {
                None = 0,
                Load = 1,
                Unload = 2,
                SkipForward = 3,
                SkipBack = 4,
                Hide = 5,
                Show = 6,
                Pause = 7,
                Resume = 8,
                Restart = 9,
                Continue = 10,
                SetChapter = 11,
                AuthoringCommit = 12,
                RequestSetChapter = 13,
                RequestSetTime = 14
            }mType;
            const uint8_t* mMemoryData = nullptr;
            uint64_t mMemorySize = 0;
            uint64_t mEvaluationRequest = 0;
            int64_t mSeekStartTicks = 0;
            int64_t mSeekStopTicks = 0;
            ImmCore::piString mStrArg;
            uint64_t mIntArg;
            ImportType mFileType;
        };

        bool Init(const wchar_t* name, uint32_t id, const LoadOptions & options = {});
        bool GetSuppress360Backdrops() const { return mSuppress360Backdrops; }
        void End(void);

        ImmImporter::Sequence *GetSequence(void);

        ImmCore::trans3d GetDocumentToWorld(void) const;
        void SetDocumentToWorld(const ImmCore::trans3d & m);

        LoadingState  GetLoadingState(void);
        ErrorState    GetErrorState(void);
        PlaybackState GetPlaybackState(void);

        bool UpdateStateCPU(
            LayerRendererSound *layerRenderSound,
            LayerRendererPaint *layerPaintRender,
            LayerRendererPicture *layerRenderPicture,
            LayerRendererModel *layerRendererModel,
            ImmImporter::Drawing::ColorSpace colorSpace,
            ImmImporter::Drawing::PaintRenderingTechnique renderingTechnique,
            ImmCore::piSoundEngine* soundEngine, ImmCore::piLog *log, const ImmCore::piTick time, const Command * command,
            const Command * queuedCommand = nullptr);
        bool UpdateStateGPU(LayerRendererPaint *layerPaintRender, LayerRendererPicture *layerRenderPicture, LayerRendererModel *layerRenderModel,
            ImmCore::piRenderer* renderer, ImmCore::piLog *log, ImmImporter::Drawing::ColorSpace colorSpace);
        void UnloadSync(
            LayerRendererSound *layerRenderSound,
            LayerRendererPaint *layerPaintRender,
            LayerRendererPicture *layerRenderPicture,
            LayerRendererModel *layerRendererModel,
            ImmImporter::Drawing::ColorSpace colorSpace,
            ImmImporter::Drawing::PaintRenderingTechnique renderingTechnique,
            ImmCore::piSoundEngine *soundEngine,
            ImmCore::piRenderer *renderer,
            ImmCore::piLog *log,
            const ImmCore::piTick now);

        int GetChapterCount(void) const;
        int GetCurrentChapter(void) const;

        bool GetHasAudio(void);

        void CancelLoading(void);

        float GetVolume(void) const;
        void SetVolume( float volume, ImmCore::piLog *log );

        ImmCore::bound3d GetBBox(void) const;

        int  GetSpawnAreaCount();
        int  GetSpawnArea();
        int  GetInitialSpawnArea();
        int  GetInitialSpawnAreaLayerId() const;
        void SetSpawnArea(int spawnAreaId);
        const ImmCore::piImage* GetSpawnAreaScreenshot(int spawnAreaId);
        void GetSpawnAreaInfo(SpawnAreaInfo &info, int spawnAreaId);
        bool GetSpawnAreaNeedsUpdate();
        void SetSpawnAreaNeedsUpdate(bool state);

        bool IsSpawnAreaAvailable(int spawnAreaId);
        int GetAvailableSpawnAreaIds(int capacity, int * ids);
        int GetSpawnAreaSnapshot(SpawnAreaSnapshot & snapshot, int capacity, int * ids);
        bool TrySetActiveSpawnAreaId(int spawnAreaId, uint64_t expectedEvaluationRevision,
            SpawnAreaPoseState & pose);
        bool CanRequestPlaybackEvaluation() const;
        uint64_t BeginPlaybackEvaluationRequest();

        void GetTime(ImmCore::piTick now, ImmCore::piTick* timeSinceStart, ImmCore::piTick* timeSinceStop);
        void SetTime(ImmCore::piTick now, ImmCore::piTick timeSinceStart, ImmCore::piTick timeSinceStop);

        void UnloadCPU(LayerRendererPaint * layerRendererPaint, LayerRendererPicture * layerRendererPicture, ImmCore::piLog * log);
		bool GetHasPlays(void) const;
        void SetCommandId(int id);
        int  GetCommandId(void);
        bool IsSequenceReady(void) const { return mSequenceReady; }

        //----------------------------------------------------------------------
        // Live editing. Additive: nothing changes for a document that is only
        // playing, because dirty refreshes run only after AttachEditing and only
        // in the Loaded state.
        //----------------------------------------------------------------------

        bool AttachEditing(void);
        bool DetachEditing(void);
        bool DiscardPendingEdits(void);
        bool IsEditing(void) const { return mEditing; }
        uint64_t GetRevision(void) const { return mPresentedRevision; }

        enum class AuthoringCommitState : int32_t
        {
            Unknown = 0,
            Queued = 1,
            Preparing = 2,
            Prepared = 3,
            Presented = 4,
            Rejected = 5
        };

        struct AuthoringRevisions
        {
            uint64_t mRequested = 0;
            uint64_t mPrepared = 0;
            uint64_t mPresented = 0;
        };

        struct AuthoringCommitStatus
        {
            uint64_t mRevision = 0;
            AuthoringCommitState mState = AuthoringCommitState::Unknown;
            int32_t mResult = 0;
            uint32_t mFailingCommand = 0;
            uint64_t mObject = 0;
            // Internal monotonic timestamps, not part of the exported C status ABI.
            uint64_t mCommitStartedNs = 0;
            uint64_t mPreparingNs = 0;
            uint64_t mPreparedNs = 0;
            uint64_t mPresentedNs = 0;
            uint64_t mRejectedNs = 0;
        };

        struct AuthoringElementGeometry
        {
            ImmImporter::Element::BrushSectionType mBrush = ImmImporter::Element::BrushSectionType::Point;
            ImmImporter::Element::VisibilityType mVisibility = ImmImporter::Element::VisibilityType::Always;
            std::vector<ImmImporter::Element::PointSource> mPoints;
        };

        bool GetDrawingHandle(uint32_t layerId, uint32_t drawingIndex, uint64_t * drawingIdOut) const;
        bool GetFrameDrawingHandle(uint32_t layerId, uint32_t frameIndex, uint64_t * drawingIdOut) const;
        int32_t QueueDrawingCreation(uint32_t layerId, uint64_t * drawingIdOut);
        int32_t QueueDrawingDeletion(uint32_t layerId, uint64_t drawingId);
        int32_t QueueDrawingGeometry(uint32_t layerId, uint64_t drawingId,
            std::vector<AuthoringElementGeometry> elements,
            ImmImporter::Drawing::ColorSpace colorSpace, bool flipped, float biggestStroke);
        int32_t QueueFrameMapping(uint32_t layerId, uint32_t frameIndex, uint64_t drawingId);
        static constexpr uint32_t kMaxFrameSequenceLength = 1000000; // Managed MaxFrames default and C ABI ceiling.
        int32_t QueueFrameSequence(uint32_t layerId, std::vector<uint64_t> drawingIds);
        int32_t QueueLayerProperties(uint32_t layerId, bool setVisibility, bool visible,
            bool setOpacity, float opacity, bool setTransform,
            const ImmCore::trans3d & transform, bool setPaintRepeat = false,
            uint32_t paintRepeat = 0);
        int32_t QueueAnimationKey(uint32_t layerId, ImmImporter::Layer::AnimProperty property,
            ImmCore::piTick time, const ImmImporter::Layer::AnimValue & value,
            ImmImporter::Layer::InterpolationType interpolation);
        int32_t QueueAnimationKeyRemoval(uint32_t layerId,
            ImmImporter::Layer::AnimProperty property, ImmCore::piTick time);
        int32_t QueueInitialSpawnArea(uint32_t layerId);
        int32_t QueueSpawnArea(uint32_t layerId,
            const ImmImporter::LayerSpawnArea::Volume & volume,
            ImmImporter::LayerSpawnArea::TrackingLevel tracking,
            const ImmCore::trans3d & transform);
        int32_t QueueGroupLayerCreation(uint32_t parentLayerId, std::wstring name,
            uint32_t * layerIdOut);
        int32_t QueuePaintLayerCreation(uint32_t parentLayerId, std::wstring name,
            uint32_t * layerIdOut);
        int32_t QueueSpawnLayerCreation(uint32_t parentLayerId, std::wstring name,
            uint32_t * layerIdOut);
        int32_t QueueLayerDeletion(uint32_t layerId);
        int32_t QueueLayerReparent(uint32_t layerId, uint32_t parentLayerId,
            uint32_t childIndex);
        bool GetAuthoringRevisions(AuthoringRevisions * revisionsOut) const;
        bool GetAuthoringCommitStatus(uint64_t revision, AuthoringCommitStatus * statusOut) const;
        bool HasQueuedAuthoringCommit(void) const { return !mSealedBatches.empty(); }

        bool HasPendingEdits(void) const { return !mSealedBatches.empty() || mPendingPresentation.mRevision != 0; }

        // Publish queued edits. The revision advances once the queues have been applied.
        uint64_t CommitEdits(int32_t * resultOut = nullptr);

    private:
        bool mEditing = false;
        void GetSpawnAreaPoseState(int spawnAreaId, SpawnAreaPoseState & pose);
        uint64_t mEvaluationRevision = 0;
        ImmCore::piTick mLastCPUTime = ImmCore::piTick(0);
        uint64_t mRequestedPlaybackCommand = 0;
        uint64_t mCompletedPlaybackCommand = 0;
        PlaybackRequestStatus mPlaybackRequestStatus = PlaybackRequestStatus::None;

        uint64_t mRequestedRevision = 0;
        uint64_t mPreparedRevision = 0;
        uint64_t mPresentedRevision = 0;

        struct DrawingHandleEntry
        {
            uint64_t mHandle = 0;
            ImmImporter::Layer * mLayer = nullptr;
            uint32_t mDrawingIndex = 0;
        };

        struct GeometryEdit
        {
            uint32_t mLayerId = 0;
            uint64_t mDrawingId = 0;
            std::vector<AuthoringElementGeometry> mElements;
            ImmImporter::Drawing::ColorSpace mColorSpace = ImmImporter::Drawing::ColorSpace::Linear;
            bool mFlipped = false;
            float mBiggestStroke = 0.0f;
        };

        struct DrawingCreation
        {
            uint32_t mLayerId = 0;
            uint64_t mDrawingId = 0;
        };

        struct FrameMapping
        {
            uint32_t mLayerId = 0;
            uint32_t mFrameIndex = 0;
            uint64_t mDrawingId = 0;
        };

        struct FrameSequence
        {
            uint32_t mLayerId = 0;
            std::vector<uint64_t> mDrawingIds;
        };

        struct DrawingDeletion
        {
            uint32_t mLayerId = 0;
            uint64_t mDrawingId = 0;
        };

        struct LayerPropertyEdit
        {
            uint32_t mLayerId = 0;
            bool mSetVisibility = false;
            bool mVisible = false;
            bool mSetOpacity = false;
            float mOpacity = 1.0f;
            bool mSetTransform = false;
            ImmCore::trans3d mTransform = ImmCore::trans3d::identity();
            bool mSetPaintRepeat = false;
            uint32_t mPaintRepeat = 0;
        };

        struct AnimationKeyEdit
        {
            uint32_t mLayerId = 0;
            ImmImporter::Layer::AnimProperty mProperty =
                ImmImporter::Layer::AnimProperty::Visibility;
            ImmCore::piTick mTime = ImmCore::piTick(0);
            ImmImporter::Layer::AnimValue mValue;
            ImmImporter::Layer::InterpolationType mInterpolation =
                ImmImporter::Layer::InterpolationType::None;
            bool mRemove = false;
        };

        struct InitialSpawnAreaEdit
        {
            uint32_t mLayerId = 0;
        };

        struct SpawnAreaEdit
        {
            uint32_t mLayerId = 0;
            ImmImporter::LayerSpawnArea::Volume mVolume = {};
            ImmImporter::LayerSpawnArea::TrackingLevel mTracking =
                ImmImporter::LayerSpawnArea::TrackingLevel::Floor;
            ImmCore::trans3d mTransform = ImmCore::trans3d::identity();
        };

        struct LayerCreation
        {
            uint32_t mLayerId = 0;
            uint32_t mParentLayerId = 0;
            std::wstring mName;
            ImmImporter::Layer::Type mType = ImmImporter::Layer::Type::Group;
        };

        struct LayerDeletion
        {
            uint32_t mLayerId = 0;
        };

        struct LayerReparent
        {
            uint32_t mLayerId = 0;
            uint32_t mParentLayerId = 0;
            uint32_t mChildIndex = 0;
        };

        struct PreparedLayerFullName
        {
            ImmImporter::Layer * mLayer = nullptr;
            ImmCore::piString mFullName;
            bool mInitialized = false;

            ~PreparedLayerFullName();
            bool Init(ImmImporter::Layer * layer, const wchar_t * fullName);
            void Publish(void);
        };

        struct SealedBatch
        {
            uint64_t mRevision = 0;
            std::vector<DrawingCreation> mDrawingCreations;
            std::vector<GeometryEdit> mGeometryEdits;
            std::vector<FrameMapping> mFrameMappings;
            std::vector<FrameSequence> mFrameSequences;
            std::vector<DrawingDeletion> mDrawingDeletions;
            std::vector<LayerPropertyEdit> mLayerPropertyEdits;
            std::vector<AnimationKeyEdit> mAnimationKeyEdits;
            std::vector<InitialSpawnAreaEdit> mInitialSpawnAreaEdits;
            std::vector<SpawnAreaEdit> mSpawnAreaEdits;
            std::vector<LayerCreation> mLayerCreations;
            std::vector<LayerDeletion> mLayerDeletions;
            std::vector<LayerReparent> mLayerReparents;
        };

        std::vector<DrawingHandleEntry> mDrawingHandles;
        uint64_t mNextDrawingHandle = 1;
        uint32_t mNextLayerHandle = 1;
        std::vector<DrawingCreation> mOpenDrawingCreations;
        std::vector<GeometryEdit> mOpenGeometryEdits;
        std::vector<FrameMapping> mOpenFrameMappings;
        std::vector<FrameSequence> mOpenFrameSequences;
        std::vector<DrawingDeletion> mOpenDrawingDeletions;
        std::vector<LayerPropertyEdit> mOpenLayerPropertyEdits;
        std::vector<AnimationKeyEdit> mOpenAnimationKeyEdits;
        std::vector<InitialSpawnAreaEdit> mOpenInitialSpawnAreaEdits;
        std::vector<SpawnAreaEdit> mOpenSpawnAreaEdits;
        std::vector<LayerCreation> mOpenLayerCreations;
        std::vector<LayerDeletion> mOpenLayerDeletions;
        std::vector<LayerReparent> mOpenLayerReparents;
        std::deque<SealedBatch> mSealedBatches;
        std::vector<AuthoringCommitStatus> mCommitStatuses;
        static constexpr size_t kMaxSealedBatches = 1;

        struct PendingPresentation
        {
            uint64_t mRevision = 0;
            uint64_t mObject = 0;
            bool mIsCreation = false;
            bool mIsFrameMapping = false;
            bool mHasFrameSequence = false;
            std::unique_ptr<ImmImporter::LayerPaintStatic::PreparedFrameTable> mPreparedFrameTable;
            bool mIsDeletion = false;
            bool mIsLayerPropertyEdit = false;
            bool mIsAnimationKeyEdit = false;
            bool mIsInitialSpawnAreaEdit = false;
            bool mIsSpawnAreaEdit = false;
            bool mIsLayerCreation = false;
            bool mHasPaintCreationRendererToken = false;
            uint64_t mCreatedPaintDrawingHandle = 0;
            bool mIsLayerDeletion = false;
            bool mIsLayerReparent = false;
            bool mHasPreparedSpawnAreas = false;
            std::vector<ImmImporter::Layer *> mPreparedSpawnAreas;
            ImmImporter::LayerSpawnArea::Volume mSpawnAreaVolume = {};
            ImmImporter::LayerSpawnArea::TrackingLevel mSpawnAreaTracking =
                ImmImporter::LayerSpawnArea::TrackingLevel::Floor;
            ImmCore::trans3d mSpawnAreaTransform = ImmCore::trans3d::identity();
            ImmImporter::Layer * mLayerCreationParent = nullptr;
            ImmImporter::Layer * mLayerPreviousParent = nullptr;
            uint32_t mLayerChildIndex = 0;
            uint32_t mLayerPreviousChildIndex = 0;
            std::vector<std::unique_ptr<PreparedLayerFullName>> mPreparedLayerFullNames;
            std::unique_ptr<ImmImporter::Layer> mCreatedLayer;
            ImmImporter::Layer::AnimProperty mAnimationProperty =
                ImmImporter::Layer::AnimProperty::Visibility;
            std::vector<ImmImporter::Layer::AnimKey> mAnimationKeys;
            bool mSetLayerVisibility = false;
            bool mLayerVisible = false;
            bool mSetLayerOpacity = false;
            float mLayerOpacity = 1.0f;
            bool mSetLayerTransform = false;
            ImmCore::trans3d mLayerTransform = ImmCore::trans3d::identity();
            bool mSetPaintRepeat = false;
            uint32_t mPaintRepeat = 0;
            ImmImporter::Layer * mLayer = nullptr;
            std::vector<ImmImporter::Layer *> mDeletedLayers;
            std::vector<ImmImporter::Drawing *> mDeletedDrawings;
            uint32_t mDrawingIndex = 0;
            uint32_t mFrameIndex = 0;
            uint32_t mMappedDrawingIndex = 0;
            uint32_t mPreviousFrameDrawingIndex = 0;
            ImmImporter::Drawing * mActive = nullptr;
            std::unique_ptr<ImmImporter::Drawing> mReplacement;
            uint64_t mRendererToken = 0;
            bool mGPUPrepared = false;
        };
        PendingPresentation mPendingPresentation;

        int32_t iQueueLayerCreation(uint32_t parentLayerId, std::wstring name,
            ImmImporter::Layer::Type type, uint32_t * layerIdOut);
        void iBuildDrawingHandleMap(ImmImporter::Layer * layer);
        const DrawingHandleEntry * iFindDrawingHandle(uint32_t layerId, uint64_t drawingId) const;
        const DrawingCreation * iFindOpenDrawingCreation(uint32_t layerId, uint64_t drawingId) const;
        AuthoringCommitStatus * iFindCommitStatus(uint64_t revision);
        void iSetCommitState(AuthoringCommitStatus * status, AuthoringCommitState state);
        void iRejectCommit(uint64_t revision, int32_t result, uint32_t failingCommand, uint64_t object);
        void iInvalidateAuthoringSession(void);
        bool iPrepareSpawnAreaCache(ImmImporter::Layer * removed, ImmImporter::Layer * inserted,
            ImmImporter::Layer * insertionParent, uint32_t insertionIndex,
            std::vector<ImmImporter::Layer *> & prepared);
        void iPublishPreparedSpawnAreaCache(void);
        int32_t iPrepareFrameSequence(const FrameSequence & sequence, const DrawingCreation * creation,
            uint64_t deletedDrawing, std::unique_ptr<ImmImporter::LayerPaintStatic::PreparedFrameTable> & table,
            ImmImporter::Layer ** layerOut);
        void iPrepareAuthoringCommit(LayerRendererPaint * layerPaintRender, ImmCore::piLog * log);
        void iDiscardPendingPresentation(LayerRendererPaint * layerPaintRender,
            ImmCore::piRenderer * renderer, ImmCore::piLog * log);

    private:
        bool iLoadCPU(ImmCore::piLog *log, ImmCore::piSoundEngine* soundEngine,
            LayerRendererPaint *layerPaintRender,
            LayerRendererPicture *layerRenderPicture,
            LayerRendererSound * layerRendererSound,
            LayerRendererModel *layerRenderModel,
            ImmImporter::Drawing::ColorSpace colorSpace,
            ImmImporter::Drawing::PaintRenderingTechnique renderingTechnique);
        bool iLoadSPU(LayerRendererSound *layerRenderSound, ImmCore::piSoundEngine* soundEngine, ImmCore::piLog *log);
        bool iLoadGPU(LayerRendererPaint *layerPaintRender, LayerRendererPicture *layerRenderPicture, LayerRendererModel *layerRenderModel, ImmCore::piRenderer* renderer,
            ImmCore::piLog *log, ImmImporter::Drawing::ColorSpace colorSpace);
        void iUnloadCPU(ImmCore::piLog *log, LayerRendererPaint *layerPaintRender, LayerRendererPicture *layerRenderPicture);
        bool iUnloadSPU(LayerRendererSound *layerRenderSound, ImmCore::piSoundEngine* soundEngine, ImmCore::piLog *log);
        bool iUnloadGPU(LayerRendererPaint *layerPaintRender, LayerRendererPicture *layerRenderPicture, LayerRendererModel *layerRenderModel, ImmCore::piRenderer* renderer, ImmCore::piLog *log);

        ImmCore::trans3d      mDocumentToWorld;
        ImmCore::piString     mFileName;
        ImportType mFileType;
        const uint8_t* mMemoryData;
        uint64_t mMemorySize;
        ImmCore::piTArray<uint8_t>* mMemoryView;
        ImmImporter::Sequence     mSequence;
        MngrPlayer   mPlayerManager;
        float        mMasterVolume;
        bool         mHidden;
        bool         mSuppress360Backdrops;
        bool         mWasPaused;
        bool         mHasAudio = false;
        int          mID;
        int          mCmdID;
        struct
        {
            std::atomic<ErrorState> mErrorState;
            std::atomic<LoadingState> mLoadingState;
            PlaybackState mPlaybackState;
            ImmCore::piMutex       mMutex;
        }mState;

        bool mSequenceReady = false;
    };

}
