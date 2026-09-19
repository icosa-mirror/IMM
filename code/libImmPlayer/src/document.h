#pragma once

#include <deque>
#include <memory>
#include <mutex>
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

namespace ImmPlayer {

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
                AuthoringCommit = 12
            }mType;
            const uint8_t* mMemoryData = nullptr;
            uint64_t mMemorySize = 0;
            ImmCore::piString mStrArg;
            uint64_t mIntArg;
            ImportType mFileType;
        };

        bool Init(const wchar_t* name, uint32_t id);
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
            ImmCore::piSoundEngine* soundEngine, ImmCore::piLog *log, const ImmCore::piTick time, const Command * command);
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
        void SetSpawnArea(int spawnAreaId);
        const ImmCore::piImage* GetSpawnAreaScreenshot(int spawnAreaId);
        void GetSpawnAreaInfo(SpawnAreaInfo &info, int spawnAreaId);
        bool GetSpawnAreaNeedsUpdate();
        void SetSpawnAreaNeedsUpdate(bool state);

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
        };

        struct AuthoringElementGeometry
        {
            ImmImporter::Element::BrushSectionType mBrush = ImmImporter::Element::BrushSectionType::Point;
            ImmImporter::Element::VisibilityType mVisibility = ImmImporter::Element::VisibilityType::Always;
            std::vector<ImmImporter::Element::PointSource> mPoints;
        };

        bool GetDrawingHandle(uint32_t layerId, uint32_t drawingIndex, uint64_t * drawingIdOut) const;
        int32_t QueueDrawingGeometry(uint32_t layerId, uint64_t drawingId,
            std::vector<AuthoringElementGeometry> elements,
            ImmImporter::Drawing::ColorSpace colorSpace, bool flipped, float biggestStroke);
        bool GetAuthoringRevisions(AuthoringRevisions * revisionsOut) const;
        bool GetAuthoringCommitStatus(uint64_t revision, AuthoringCommitStatus * statusOut) const;
        bool HasQueuedAuthoringCommit(void) const { return !mSealedBatches.empty(); }

        bool HasPendingEdits(void) const { return !mSealedBatches.empty() || mPendingPresentation.mRevision != 0; }

        // Publish queued edits. The revision advances once the queues have been applied.
        uint64_t CommitEdits(int32_t * resultOut = nullptr);

    private:
        bool mEditing = false;
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

        struct SealedBatch
        {
            uint64_t mRevision = 0;
            std::vector<GeometryEdit> mGeometryEdits;
        };

        std::vector<DrawingHandleEntry> mDrawingHandles;
        uint64_t mNextDrawingHandle = 1;
        std::vector<GeometryEdit> mOpenGeometryEdits;
        std::deque<SealedBatch> mSealedBatches;
        std::vector<AuthoringCommitStatus> mCommitStatuses;
        static constexpr size_t kMaxSealedBatches = 1;

        struct PendingPresentation
        {
            uint64_t mRevision = 0;
            uint64_t mObject = 0;
            ImmImporter::Drawing * mActive = nullptr;
            std::unique_ptr<ImmImporter::Drawing> mReplacement;
            uint64_t mRendererToken = 0;
        };
        PendingPresentation mPendingPresentation;

        void iBuildDrawingHandleMap(ImmImporter::Layer * layer);
        const DrawingHandleEntry * iFindDrawingHandle(uint32_t layerId, uint64_t drawingId) const;
        AuthoringCommitStatus * iFindCommitStatus(uint64_t revision);
        void iRejectCommit(uint64_t revision, int32_t result, uint32_t failingCommand, uint64_t object);
        void iInvalidateAuthoringSession(void);
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
        bool         mWasPaused;
        bool         mHasAudio = false;
        int          mID;
        int          mCmdID;
        struct
        {
            ErrorState    mErrorState;
            LoadingState  mLoadingState;
            PlaybackState mPlaybackState;
            ImmCore::piMutex       mMutex;
        }mState;

        bool mSequenceReady = false;
    };

}
