#include <stdlib.h>
#include <thread>
#include <chrono>


#include "libImmCore/src/libBasics/piDebug.h"

#include "libImmImporter/src/document/layer.h"
#include "libImmImporter/src/document/layerEffect.h"
#include "libImmImporter/src/document/layerInstance.h"
#include "libImmImporter/src/document/layerPaint.h"
#include "libImmImporter/src/document/layerSound.h"
#include "libImmImporter/src/document/layerSpawnArea.h"
#include "libImmImporter/src/document/layerPaint/drawingStatic.h"
#include "libImmImporter/src/document/sequence.h"
#include "libImmImporter/src/fromImmersive/fromImmersive.h"

#include "document.h"
using namespace ImmCore;
using namespace ImmImporter;


namespace ImmPlayer
{
    static bool iEnvFlagEnabled(const char *name)
    {
        const char *value = getenv(name);
        return value != nullptr && value[0] != '\0' && value[0] != '0';
    }

    Document::Document() {}

    Document::~Document() {}

    bool Document::Init(const wchar_t* name, uint32_t id)
    {
        //mLog = log;

        mID = id;

        if (!mState.mMutex.Init())
            return false;

        if (!mFileName.Init(1024))
            return false;

        mState.mLoadingState = LoadingState::UnloadingCompleted;
        mState.mErrorState = ErrorState::NoError;
        mState.mPlaybackState = PlaybackState::Waiting;
        mHidden = false;
        mDocumentToWorld = trans3d::identity();
        mMasterVolume = 1.0f;
        mCmdID = -1;
        mFileType = ImportType::IMM_disk;
        mMemoryData = nullptr;
        mMemorySize = 0;
        mMemoryView = nullptr;
        mSequenceReady = false;
        mEditing = false;
        mRequestedRevision = 0;
        mPreparedRevision = 0;
        mPresentedRevision = 0;
        mNextDrawingHandle = 1;
        return true;
    }

    void Document::End(void)
    {
        mOpenGeometryEdits.clear();
        mSealedBatches.clear();
        mCommitStatuses.clear();
        mDrawingHandles.clear();
        if (mPendingPresentation.mReplacement)
            mPendingPresentation.mReplacement->Deinit();
        mPendingPresentation = PendingPresentation{};
        delete mMemoryView;
        mMemoryView = nullptr;
        mFileName.End();
        mState.mMutex.End();
    }

    trans3d Document::GetDocumentToWorld(void)const
    {
        return mDocumentToWorld;
    }

    void Document::SetDocumentToWorld(const trans3d & m)
    {
        mDocumentToWorld = m;
    }


    int Document::GetSpawnAreaCount()
    {
        return mPlayerManager.GetSpawnAreaCount();
    }

    int Document::GetSpawnArea()
    {
        return mPlayerManager.GetSpawnArea();
    }

    int Document::GetInitialSpawnArea()
    {
        return mPlayerManager.GetInitialSpawnArea();
    }

    void Document::SetSpawnArea(int spawnAreaId)
    {
        mPlayerManager.SetSpawnArea(spawnAreaId);
    }

    const piImage* Document::GetSpawnAreaScreenshot(int spawnAreaId)
    {
        const Layer *layer = mPlayerManager.GetSpawnAreaLayer(spawnAreaId);
        const LayerSpawnArea* layerSpawnArea = (LayerSpawnArea*)layer->GetImplementation();

        return layerSpawnArea->GetScreenshot();
    }

    void Document::GetSpawnAreaInfo(SpawnAreaInfo& info, int spawnAreaId)
    {
        const Layer *layer = mPlayerManager.GetSpawnAreaLayer(spawnAreaId);
        const LayerSpawnArea* layerSpawnArea = (LayerSpawnArea*)layer->GetImplementation();
        info.mName = layer->GetName().GetS();
        info.mVersion = layerSpawnArea->GetVersion();
        info.mIsFloorLevel = layerSpawnArea->GetTracking() == LayerSpawnArea::TrackingLevel::Floor;
        info.mSpawnAreaToWorld = layer->GetTransformToWorld();
        info.mVolume = layerSpawnArea->GetVolume();

        const bool isSelfAnimated = layer->GetNumAnimKeys(Layer::AnimProperty::Transform) > 1;
        bool areParentsAnimated = false;
        Layer* parent = layer->GetParent();
        while (parent != nullptr)
        {
            if (parent->GetNumAnimKeys(Layer::AnimProperty::Transform) > 1)
            {
                areParentsAnimated = true;
                break;
            }
            parent = parent->GetParent();
        }
        info.mAnimated = isSelfAnimated || areParentsAnimated;
    }

    bool Document::GetSpawnAreaNeedsUpdate()
    {
        return mSequence.GetSpawnAreaNeedsUpdate();
    }

    void Document::SetSpawnAreaNeedsUpdate(bool state)
    {
        mSequence.SetSpawnAreaNeedsUpdate(state);
    }

    void Document::GetTime(piTick now, piTick * timeSinceStart, piTick * timeSinceStop)
    {
        mPlayerManager.GetTime(now, timeSinceStart, timeSinceStop);
    }

    void Document::SetTime(piTick now, piTick timeSinceStart, piTick timeSinceStop)
    {
        mPlayerManager.SetTime(now, timeSinceStart, timeSinceStop);
    }

    //----------------------------------------------------

    Document::PlaybackState Document::GetPlaybackState(void)
    {
        return mState.mPlaybackState;
    }

    Sequence *Document::GetSequence(void)
    {
        return &mSequence;
    }


    Document::LoadingState Document::GetLoadingState(void)
    {
        LoadingState res;
        mState.mMutex.Lock();
        res = mState.mLoadingState;
        mState.mMutex.UnLock();
        return res;
    }

    Document::ErrorState Document::GetErrorState(void)
    {
        ErrorState res;
        mState.mMutex.Lock();
        res = mState.mErrorState;
        mState.mMutex.UnLock();
        return res;
    }

    //----------------------------------------------------

    bool Document::UpdateStateCPU(
        LayerRendererSound *layerRenderSound,
        LayerRendererPaint *layerPaintRender,
        LayerRendererPicture *layerRenderPicture,
        LayerRendererModel *layerRendererModel,
        Drawing::ColorSpace colorSpace,
        Drawing::PaintRenderingTechnique renderingTechnique,
        piSoundEngine* soundEngine, piLog *log, const piTick now,
        const Command * command)
    {
        // Authoring work enters through the same per-document command slot as playback and
        // loading commands. ABI mutation calls only copy into the open batch.
        if (mEditing && mState.mLoadingState == LoadingState::Loaded && command != nullptr &&
            command->mType == Command::Type::AuthoringCommit)
            iPrepareAuthoringCommit(layerPaintRender, log);

        //======================================================================
        // 1. process loading commands
        //======================================================================

        if (command != nullptr)
        {
            if (command->mType == Command::Type::Load)
            {
                if (mState.mLoadingState == Document::LoadingState::LoadingPending || mState.mLoadingState == Document::LoadingState::UnloadingCompleted)
                {
                    iInvalidateAuthoringSession();
                    mState.mLoadingState = LoadingState::LoadingPending;
                    mMemoryData = command->mMemoryData;
                    mMemorySize = command->mMemorySize;
                    if (!mFileName.Copy(&command->mStrArg))
                        return false;
                    mFileType = command->mFileType;
                }
            }
            else if (command->mType == Command::Type::Unload)
            {
                if (mState.mLoadingState == Document::LoadingState::Loaded)
                {
                    iInvalidateAuthoringSession();
                    if (IsLoadingAsync())
                    {
                        StopLoadingAsync();
                        mState.mLoadingState = LoadingState::StopingLoadingAsync;
                    }
                    else
                    {
                        mPlayerManager.Exit();
                        mState.mLoadingState = LoadingState::UnloadingGPU;
                    }
                }
            }
        }

        //======================================================================
        // 2. process loading state machine
        //======================================================================

        const LoadingState st = mState.mLoadingState;
        if (st == LoadingState::LoadingPending)
        {
            mState.mLoadingState = LoadingState::LoadingCPU;

            std::thread loadingThread([this, log, soundEngine, layerPaintRender, layerRenderPicture, layerRenderSound, layerRendererModel, colorSpace, renderingTechnique]()
            {
                if (!iLoadCPU(log, soundEngine, layerPaintRender, layerRenderPicture, layerRenderSound, layerRendererModel, colorSpace, renderingTechnique))
                {
                    mState.mLoadingState = LoadingState::UnloadingCompleted;
                    mState.mErrorState = ErrorState::FailedCPU;
                }
                else
                {
                    mState.mLoadingState = LoadingState::LoadingSPU;
                }
            });

            loadingThread.detach();
        }
        else if (st == LoadingState::LoadingCPU)
        {
            // do nothing, keep waiting
        }
        else if (st == LoadingState::LoadingSPU)
        {
            //LoadSPU(&mLayerRenderSound, mSoundEngine, mLog);
            if (!iLoadSPU(layerRenderSound, soundEngine, log))
            {
                mState.mLoadingState = LoadingState::UnloadingCPU;
                mState.mErrorState = ErrorState::FailedSPU;
            }
            else
            {
                mState.mLoadingState = LoadingState::LoadingGPU;
            }
        }
        else if (st == LoadingState::LoadingGPU)
        {
            // do nothing, handled in UpdateStateGPU
        }
        else if (st == LoadingState::LoadingComplete)
        {
            // after a new document is loaded
            // update the viewer position in player to rencenter
            // the player needs to save this to do origin calibration when changing spawn areas
            mPlayerManager.Enter(&mSequence, now );
            mState.mLoadingState = LoadingState::Loaded;
        }
        else if (st == LoadingState::Loaded)
        {
        }
        else if (st == LoadingState::StopingLoadingAsync)
        {
            // wait for the loading thread to finish
            if (!IsLoadingAsync())
            {
                mPlayerManager.Exit();
                mState.mLoadingState = LoadingState::UnloadingGPU;
            }
        }
        else if (st == LoadingState::UnloadingGPU)
        {
            // do nothing, handled in UpdateStateGPU
        }
        else if (st == LoadingState::UnloadingSPU)
        {
            iUnloadSPU(layerRenderSound, soundEngine, log);
            mState.mLoadingState = LoadingState::UnloadingCPU;
        }
        else if (st == LoadingState::UnloadingCPU)
        {
            iUnloadCPU(log, layerPaintRender, layerRenderPicture);
            mState.mLoadingState = LoadingState::UnloadingCompleted;
        }
        else if (st == LoadingState::UnloadingCompleted)
        {
            // do nothing
        }



        //======================================================================
        // 3. process playback commands
        //======================================================================
        if (command != nullptr)
        {
            if (command->mType == Command::Type::SkipForward)
            {
                if (mState.mLoadingState == LoadingState::Loaded)
                {
                    mPlayerManager.SkipForward(now);
                }
            }
            else if (command->mType == Command::Type::SkipBack)
            {
                if (mState.mLoadingState == LoadingState::Loaded)
                {
                    mPlayerManager.SkipBack(now);
                }
            }
            else if (command->mType == Command::Type::SetChapter)
            {
                if (mState.mLoadingState == LoadingState::Loaded)
                {
                    mPlayerManager.SetChapter(now, static_cast<size_t>(command->mIntArg));
                }
            }
            else if (command->mType == Command::Type::Pause)
            {
                if (mState.mLoadingState == LoadingState::Loaded && !mHidden)
                {
                    const bool isPaused = mPlayerManager.GetIsPaused();
                    if (!isPaused)
                    {
                        if (command->mIntArg != 0)
                        {
                            mPlayerManager.Pause(piTick(static_cast<int64_t>(command->mIntArg)));
                        }
                        else
                        {
                            mPlayerManager.Pause(now);
                        }
                    }
                }
            }
            else if (command->mType == Command::Type::Resume)
            {
                if (mState.mLoadingState == LoadingState::Loaded && !mHidden)
                {
                    const bool isPaused = mPlayerManager.GetIsPaused();
                    if (isPaused)
                    {
                        if (command->mIntArg != 0)
                        {
                            mPlayerManager.Resume(piTick(static_cast<int64_t>(command->mIntArg)));
                        }
                        else
                        {
                            mPlayerManager.Resume(now);
                        }
                    }
                }
            }
            else if (command->mType == Command::Type::Show)
            {
                if (mState.mLoadingState == LoadingState::Loaded)
                {
                    mHidden = false;
                    if( !mWasPaused )
                    {
                        mPlayerManager.Resume(now);
                    }
                }
            }
            else if (command->mType == Command::Type::Hide)
            {
                if (mState.mLoadingState == LoadingState::Loaded)
                {
                    const bool isPaused = mPlayerManager.GetIsPaused();
                    if (!isPaused)
                    {
                        mPlayerManager.Pause(now);
                    }
                    mWasPaused = isPaused;
                    mHidden = true;
                }
            }
            else if (command->mType == Command::Type::Continue)
            {
                if (mState.mLoadingState == LoadingState::Loaded)
                {
                    mPlayerManager.Continue(now);
                }
            }
            else if (command->mType == Command::Type::Restart)
            {
                if (mState.mLoadingState == LoadingState::Loaded)
                {
                    mPlayerManager.Restart(now);
                }
            }
        }


        {
            if (mState.mLoadingState != LoadingState::Loaded) mState.mPlaybackState = PlaybackState::PausedAndHidden;
            else if (mHidden) mState.mPlaybackState = PlaybackState::PausedAndHidden;
            else if (mPlayerManager.GetIsFinished()) mState.mPlaybackState = PlaybackState::Finished;  // story has reached the end, need to check this before waiting!!!
            else if (mPlayerManager.GetIsPaused()) mState.mPlaybackState = PlaybackState::Paused;
            else if (mPlayerManager.GetIsWaiting()) mState.mPlaybackState = PlaybackState::Waiting;   // story driven pause/wait to proceed, still playing
            else
                mState.mPlaybackState = PlaybackState::Playing;
        }

        const bool shouldRender = mState.mLoadingState == Document::LoadingState::Loaded && mState.mPlaybackState != Document::PlaybackState::PausedAndHidden;


        //======================================================================
        // 4. animation
        //======================================================================

        if (shouldRender)
        {
            if (mPlayerManager.GetIsPlaying())
                mPlayerManager.Update(now);
        }


        // should we animate+render?
        return shouldRender;
    }

    bool Document::UpdateStateGPU(LayerRendererPaint *layerPaintRender,  LayerRendererPicture *layerRenderPicture, LayerRendererModel *layerRenderModel,
        piRenderer* renderer, piLog *log, Drawing::ColorSpace colorSpace)
    {
        Document::LoadingState st = mState.mLoadingState;
        if (mEditing && st == LoadingState::Loaded && mPendingPresentation.mRevision != 0)
        {
            const uint64_t revision = mPendingPresentation.mRevision;
            AuthoringCommitStatus * status = iFindCommitStatus(revision);
            if (layerPaintRender == nullptr ||
                !layerPaintRender->PrepareDrawingReplacementInGPU(
                    renderer, mPendingPresentation.mRendererToken, log))
            {
                iRejectCommit(revision, -10, 0, mPendingPresentation.mObject);
                iDiscardPendingPresentation(layerPaintRender, renderer, log);
            }
            else
            {
                if (status != nullptr)
                    status->mState = AuthoringCommitState::Prepared;
                mPreparedRevision = revision;

                Drawing * replacement = mPendingPresentation.mReplacement.get();
                if (!layerPaintRender->PresentDrawingReplacement(
                    mPendingPresentation.mActive, replacement,
                    mPendingPresentation.mRendererToken, revision, log))
                {
                    iRejectCommit(revision, -10, 0, mPendingPresentation.mObject);
                    iDiscardPendingPresentation(layerPaintRender, renderer, log);
                }
                else
                {
                    // PresentDrawingReplacement now owns the object containing the retired
                    // geometry and keeps it alive until its renderer retirement policy fires.
                    mPendingPresentation.mReplacement.release();
                    mPendingPresentation = PendingPresentation{};
                    if (status != nullptr)
                        status->mState = AuthoringCommitState::Presented;
                    mPresentedRevision = revision;
                    log->Printf(LT_MESSAGE, L"[IMM_LIVE_EDIT] presented revision=%llu drawing=%llu",
                        static_cast<unsigned long long>(revision),
                        static_cast<unsigned long long>(status != nullptr ? status->mObject : 0));
                }
            }
        }

        if (st == LoadingState::LoadingGPU)
        {
            if (!iLoadGPU(layerPaintRender, layerRenderPicture, layerRenderModel, renderer, log, colorSpace))
            {
                mState.mLoadingState = LoadingState::UnloadingSPU;
                mState.mErrorState = ErrorState::FailedGPU;
            }
            else
            {
                mState.mLoadingState = LoadingState::LoadingComplete;
            }
        }
        else if (st == LoadingState::UnloadingGPU)
        {
            if (mPendingPresentation.mRevision != 0)
            {
                iRejectCommit(mPendingPresentation.mRevision, -9, 0, mPendingPresentation.mObject);
                iDiscardPendingPresentation(layerPaintRender, renderer, log);
            }
            iUnloadGPU(layerPaintRender, layerRenderPicture, layerRenderModel, renderer, log);
            mState.mLoadingState = LoadingState::UnloadingSPU;
        }

        // should we animate+render?
        return (mState.mLoadingState == Document::LoadingState::Loaded && mState.mPlaybackState != Document::PlaybackState::PausedAndHidden);
    }

    //--------------------------------------------------------------------------
    // Live editing
    //--------------------------------------------------------------------------

    bool Document::AttachEditing(void)
    {
        // A document can be edited once it is either fully loaded or idle (created empty).
        if (mState.mLoadingState != LoadingState::Loaded &&
            mState.mLoadingState != LoadingState::UnloadingCompleted)
            return false;

        if (mEditing)
            return true;

        mDrawingHandles.clear();
        mNextDrawingHandle = 1;
        iBuildDrawingHandleMap(mSequence.GetRoot());
        mEditing = true;
        return true;
    }

    bool Document::DetachEditing(void)
    {
        if (!mEditing || !mOpenGeometryEdits.empty() || !mSealedBatches.empty() ||
            mPendingPresentation.mRevision != 0)
            return false;

        mDrawingHandles.clear();
        mCommitStatuses.clear();
        mEditing = false;
        return true;
    }

    bool Document::DiscardPendingEdits(void)
    {
        if (!mEditing)
            return false;
        mOpenGeometryEdits.clear();
        return true;
    }

    void Document::iBuildDrawingHandleMap(ImmImporter::Layer * layer)
    {
        if (layer == nullptr)
            return;

        if (layer->GetType() == Layer::Type::Paint)
        {
            LayerPaint * paint = (LayerPaint *)layer->GetImplementation();
            if (paint != nullptr)
            {
                for (uint32_t drawingIndex = 0; drawingIndex < paint->GetNumDrawings(); drawingIndex++)
                {
                    mDrawingHandles.push_back(DrawingHandleEntry{ mNextDrawingHandle++, layer, drawingIndex });
                }
            }
        }

        for (uint32_t childIndex = 0; childIndex < layer->GetNumChildren(); childIndex++)
            iBuildDrawingHandleMap(layer->GetChild(childIndex));
    }

    const Document::DrawingHandleEntry * Document::iFindDrawingHandle(uint32_t layerId, uint64_t drawingId) const
    {
        for (const DrawingHandleEntry & entry : mDrawingHandles)
        {
            if (entry.mHandle == drawingId && entry.mLayer != nullptr && entry.mLayer->GetID() == layerId)
                return &entry;
        }
        return nullptr;
    }

    bool Document::GetDrawingHandle(uint32_t layerId, uint32_t drawingIndex, uint64_t * drawingIdOut) const
    {
        if (!mEditing || drawingIdOut == nullptr)
            return false;

        for (const DrawingHandleEntry & entry : mDrawingHandles)
        {
            if (entry.mLayer != nullptr && entry.mLayer->GetID() == layerId && entry.mDrawingIndex == drawingIndex)
            {
                *drawingIdOut = entry.mHandle;
                return true;
            }
        }
        return false;
    }

    int32_t Document::QueueDrawingGeometry(uint32_t layerId, uint64_t drawingId,
        std::vector<AuthoringElementGeometry> elements,
        Drawing::ColorSpace colorSpace, bool flipped, float biggestStroke)
    {
        if (!mEditing)
            return -4;
        if (elements.empty() || biggestStroke <= 0.0f)
            return -2;
        if (iFindDrawingHandle(layerId, drawingId) == nullptr)
            return -1;

        // This first vertical slice intentionally permits one replacement per batch. It keeps
        // failure atomic until replacement layer bundles support multi-command preparation.
        if (!mOpenGeometryEdits.empty())
            return -7;

        GeometryEdit edit;
        edit.mLayerId = layerId;
        edit.mDrawingId = drawingId;
        edit.mElements = std::move(elements);
        edit.mColorSpace = colorSpace;
        edit.mFlipped = flipped;
        edit.mBiggestStroke = biggestStroke;
        mOpenGeometryEdits.push_back(std::move(edit));
        return 0;
    }

    bool Document::GetAuthoringRevisions(AuthoringRevisions * revisionsOut) const
    {
        if (!mEditing || revisionsOut == nullptr)
            return false;
        revisionsOut->mRequested = mRequestedRevision;
        revisionsOut->mPrepared = mPreparedRevision;
        revisionsOut->mPresented = mPresentedRevision;
        return true;
    }

    bool Document::GetAuthoringCommitStatus(uint64_t revision, AuthoringCommitStatus * statusOut) const
    {
        if (!mEditing || statusOut == nullptr)
            return false;
        for (const AuthoringCommitStatus & status : mCommitStatuses)
        {
            if (status.mRevision == revision)
            {
                *statusOut = status;
                return true;
            }
        }
        return false;
    }

    Document::AuthoringCommitStatus * Document::iFindCommitStatus(uint64_t revision)
    {
        for (AuthoringCommitStatus & status : mCommitStatuses)
        {
            if (status.mRevision == revision)
                return &status;
        }
        return nullptr;
    }

    void Document::iRejectCommit(uint64_t revision, int32_t result, uint32_t failingCommand, uint64_t object)
    {
        AuthoringCommitStatus * status = iFindCommitStatus(revision);
        if (status == nullptr)
            return;
        status->mState = AuthoringCommitState::Rejected;
        status->mResult = result;
        status->mFailingCommand = failingCommand;
        status->mObject = object;
    }

    void Document::iInvalidateAuthoringSession(void)
    {
        if (!mEditing)
            return;

        mOpenGeometryEdits.clear();
        mSealedBatches.clear();
        mCommitStatuses.clear();
        mDrawingHandles.clear();
        mEditing = false;
    }

    void Document::iPrepareAuthoringCommit(LayerRendererPaint * layerPaintRender, piLog * log)
    {
        if (mSealedBatches.empty() || mPendingPresentation.mRevision != 0)
            return;

        SealedBatch batch = std::move(mSealedBatches.front());
        mSealedBatches.pop_front();
        AuthoringCommitStatus * status = iFindCommitStatus(batch.mRevision);
        if (status != nullptr)
            status->mState = AuthoringCommitState::Preparing;

        if (batch.mGeometryEdits.size() != 1 || layerPaintRender == nullptr)
        {
            iRejectCommit(batch.mRevision, -3, 0, 0);
            return;
        }

        GeometryEdit & edit = batch.mGeometryEdits[0];
        if (status != nullptr)
            status->mObject = edit.mDrawingId;
        const DrawingHandleEntry * entry = iFindDrawingHandle(edit.mLayerId, edit.mDrawingId);
        if (entry == nullptr || entry->mLayer == nullptr)
        {
            iRejectCommit(batch.mRevision, -1, 0, edit.mDrawingId);
            return;
        }

        LayerPaint * paint = (LayerPaint *)entry->mLayer->GetImplementation();
        Drawing * active = paint != nullptr ? paint->GetDrawing(static_cast<int>(entry->mDrawingIndex)) : nullptr;
        DrawingStatic * activeStatic = dynamic_cast<DrawingStatic *>(active);
        if (activeStatic == nullptr)
        {
            iRejectCommit(batch.mRevision, -3, 0, edit.mDrawingId);
            return;
        }

        std::unique_ptr<DrawingStatic> replacement(new (std::nothrow) DrawingStatic());
        if (!replacement || !replacement->Init(static_cast<uint32_t>(edit.mElements.size())) ||
            !replacement->StartAdding(edit.mBiggestStroke))
        {
            iRejectCommit(batch.mRevision, -5, 0, edit.mDrawingId);
            return;
        }

        std::unique_ptr<Element> element(new (std::nothrow) Element());
        bool geometryBuilt = element != nullptr;
        for (const AuthoringElementGeometry & source : edit.mElements)
        {
            geometryBuilt = geometryBuilt && element->Set(source.mPoints.data(),
                static_cast<int>(source.mPoints.size()), source.mBrush, source.mVisibility,
                edit.mBiggestStroke);
            geometryBuilt = geometryBuilt && replacement->Add(
                element.get(), edit.mColorSpace, edit.mFlipped);
            if (!geometryBuilt)
                break;
        }
        if (!geometryBuilt)
        {
            replacement->Deinit();
            iRejectCommit(batch.mRevision, -5, 0, edit.mDrawingId);
            return;
        }
        replacement->StopAdding();
        replacement->SetLoaded(activeStatic->GetLoaded());

        uint64_t rendererToken = 0;
        if (!layerPaintRender->PrepareDrawingReplacementInCPU(replacement.get(), &rendererToken, log))
        {
            replacement->Deinit();
            iRejectCommit(batch.mRevision, -10, 0, edit.mDrawingId);
            return;
        }

        mPendingPresentation.mRevision = batch.mRevision;
        mPendingPresentation.mObject = edit.mDrawingId;
        mPendingPresentation.mActive = activeStatic;
        mPendingPresentation.mReplacement = std::move(replacement);
        mPendingPresentation.mRendererToken = rendererToken;
    }

    uint64_t Document::CommitEdits(int32_t * resultOut)
    {
        if (resultOut != nullptr)
            *resultOut = 0;
        if (!mEditing || mOpenGeometryEdits.empty())
        {
            if (resultOut != nullptr)
                *resultOut = -4;
            return 0;
        }
        if (mSealedBatches.size() >= kMaxSealedBatches || mPendingPresentation.mRevision != 0)
        {
            if (resultOut != nullptr)
                *resultOut = -7;
            return 0;
        }

        SealedBatch batch;
        batch.mRevision = ++mRequestedRevision;
        batch.mGeometryEdits.swap(mOpenGeometryEdits);
        mSealedBatches.push_back(std::move(batch));
        mCommitStatuses.push_back(AuthoringCommitStatus{
            mRequestedRevision, AuthoringCommitState::Queued, 0, 0, 0 });
        if (mCommitStatuses.size() > 64)
            mCommitStatuses.erase(mCommitStatuses.begin());
        return mRequestedRevision;
    }

    void Document::iDiscardPendingPresentation(LayerRendererPaint * layerPaintRender,
        piRenderer * renderer, piLog * log)
    {
        if (mPendingPresentation.mRevision == 0)
            return;
        if (layerPaintRender != nullptr)
            layerPaintRender->CancelDrawingReplacement(
                renderer, mPendingPresentation.mRendererToken, log);
        if (mPendingPresentation.mReplacement)
            mPendingPresentation.mReplacement->Deinit();
        mPendingPresentation = PendingPresentation{};
    }

    void Document::UnloadSync(
        LayerRendererSound *layerRenderSound,
        LayerRendererPaint *layerPaintRender,
        LayerRendererPicture *layerRenderPicture,
        LayerRendererModel *layerRendererModel,
        Drawing::ColorSpace colorSpace,
        Drawing::PaintRenderingTechnique renderingTechnique,
        piSoundEngine *soundEngine,
        piRenderer *renderer,
        piLog *log,
        const piTick now)
    {
        iInvalidateAuthoringSession();
        if (IsLoadingAsync())
        {
            StopLoadingAsync();
        }

        int guard = 0;
        while (mState.mLoadingState != LoadingState::UnloadingCompleted && guard < 256)
        {
            if (mState.mLoadingState == LoadingState::Loaded)
            {
                mPlayerManager.Exit();
                mState.mLoadingState = LoadingState::UnloadingGPU;
            }

            UpdateStateGPU(layerPaintRender, layerRenderPicture, layerRendererModel, renderer, log, colorSpace);
            UpdateStateCPU(layerRenderSound,
                           layerPaintRender,
                           layerRenderPicture,
                           layerRendererModel,
                           colorSpace,
                           renderingTechnique,
                           soundEngine,
                           log,
                           now,
                           nullptr);
            ++guard;
        }

        if (mState.mLoadingState != LoadingState::UnloadingCompleted && log)
        {
            log->Printf(LT_ERROR, L"Document synchronous unload did not complete for id=%d state=%d", mID, static_cast<int>(mState.mLoadingState));
        }
    }

    //===============================================
    bool Document::iLoadSPU(LayerRendererSound *layerRenderSound, piSoundEngine* soundEngine, piLog *log)
    {
        std::chrono::steady_clock::time_point timeStart = std::chrono::steady_clock::now();

        log->Printf(LT_MESSAGE, L"Loading in SPU...");

        auto loadInSPU = [this, log, layerRenderSound, soundEngine](Layer* layer, int level, int child, bool instance) -> bool
        {
            if (layer->GetType() == Layer::Type::Sound)
            {
                if (!mHasAudio)
                    mHasAudio = true;
                if (!layerRenderSound->LoadInCPU(log, layer))
                {
                    log->Printf(LT_ERROR, L"Could not load in CPU Sound layer %s", layer->GetFullName()->GetS());
                    return false;
                }
#ifndef UNLOAD_SOUNDS
                // When voice management is on, we don't preload sounds in the sound engine
                if (!layerRenderSound->LoadInSPU(soundEngine, log, layer))
                {
                    log->Printf(LT_ERROR, L"Could not load in SPU Sound layer %s", layer->GetFullName()->GetS());
                    return false;
                }
#endif
            }
            return true;
        };
        if (!mSequence.Recurse(loadInSPU, false, false, false, false))
        {
            log->Printf(LT_ERROR, L"Error loading in SPU...");
            return false;
        }

        std::chrono::steady_clock::time_point timeEnd = std::chrono::steady_clock::now();

        log->Printf(LT_MESSAGE, L"Loaded in SPU! (%d ms)", static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(timeEnd - timeStart).count()));

        return true;
    }

    bool Document::iUnloadSPU(LayerRendererSound *layerRenderSound, piSoundEngine* soundEngine, piLog *log)
    {
        std::chrono::steady_clock::time_point timeStart = std::chrono::steady_clock::now();

        log->Printf(LT_MESSAGE, L"Unloading SPU [%d]...", mID);

        auto unloadInSPU = [this, log, layerRenderSound, soundEngine](Layer* layer, int level, int child, bool instance) -> bool
        {
            if (layer->GetType() == Layer::Type::Sound)
            {
                if (!layerRenderSound->UnloadInSPU(soundEngine, layer))
                {
                    log->Printf(LT_ERROR, L"Could not unload in SPU Sound layer %s", layer->GetFullName()->GetS());
                    //return false;
                }
            }
            return true;
        };

        if (!mSequence.Recurse(unloadInSPU, false, false, false, false))
        {
            log->Printf(LT_ERROR, L"Error unloading in SPU...");
            return false;
        }

        std::chrono::steady_clock::time_point timeEnd = std::chrono::steady_clock::now();

        log->Printf(LT_MESSAGE, L"Unloaded in SPU! (%d ms)", static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(timeEnd - timeStart).count()));

        return true;
    }

    bool Document::iLoadCPU( piLog *log, piSoundEngine* soundEngine,
        LayerRendererPaint *layerPaintRender,
        LayerRendererPicture *layerRenderPicture,
        LayerRendererSound * layerRendererSound,
        LayerRendererModel *layerRenderModel,
        Drawing::ColorSpace colorSpace,
        Drawing::PaintRenderingTechnique renderingTechnique)
    {
        std::chrono::steady_clock::time_point timeStart = std::chrono::steady_clock::now();

        log->Printf(LT_MESSAGE, L"Loading in CPU...");
        //----------------------
        // sequence
        //----------------------

        switch (mFileType)
        {
        case ImportType::IMM_disk:
        {
            if (!ImportFromDisk(&mSequence, log, mFileName.GetS(), colorSpace, renderingTechnique))
            {
                log->Printf(LT_ERROR, L"Could not load IMM from disk : %s", mFileName.GetS());
                return false;
            }
            break;
        }
        case ImportType::IMM_memory:
        {
            if (mMemoryData == nullptr || mMemorySize == 0)
            {
                log->Printf(LT_ERROR, L"IMM memory source is empty");
                return false;
            }

            delete mMemoryView;
            mMemoryView = new piTArray<uint8_t>();
            if (mMemoryView == nullptr || !mMemoryView->Init(0, false))
                return false;
            mMemoryView->Set(const_cast<uint8_t*>(mMemoryData), mMemorySize);
            if (!ImportFromMemory(mMemoryView, &mSequence, log, colorSpace, renderingTechnique))
            {
                delete mMemoryView;
                mMemoryView = nullptr;
                log->Printf(LT_ERROR, L"Could not load IMM from memory");
                return false;
            }
            break;
        }
        }

        mSequenceReady = true;

        //----------------------
        //
        //----------------------
        if (!mPlayerManager.Init(log))
            return false;

        //----------------------
        // layer renderers
        //----------------------

        if (!mSequence.Recurse(
            [this, log, layerPaintRender, layerRenderPicture, layerRenderModel](Layer* layer, int level, int child, bool instance) -> bool
            {
                if (IsStoppedLoading()) return false;

                const Layer::Type layerType = layer->GetType();
                bool res = true;
                if (layerType == Layer::Type::Paint)   res = layerPaintRender->LoadInCPU(log, layer);
                if (layerType == Layer::Type::Picture) res = layerRenderPicture->LoadInCPU(log, layer);
                if (layerType == Layer::Type::Model) res = layerRenderModel->LoadInCPU(log, layer);
                if (!res) log->Printf(LT_ERROR, L"Could not load LayerRenderer [%d] for layer %s", mID, layer->GetFullName()->GetS());
                return res;
            }, false, false, false, false))
                return false;

        //----------------------

        std::chrono::steady_clock::time_point timeEnd = std::chrono::steady_clock::now();

        log->Printf(LT_MESSAGE, L"Loaded in CPU! (%d ms), file: %s", static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(timeEnd - timeStart).count()), mFileName.GetS());

        return true;
    }

    void Document::iUnloadCPU(piLog *log,
        LayerRendererPaint *layerPaintRender,
        LayerRendererPicture *layerRenderPicture)
    {
        std::chrono::steady_clock::time_point timeStart = std::chrono::steady_clock::now();

        // rendering relevant CPU data
        mSequence.Recurse(
            [this, log, layerPaintRender, layerRenderPicture](Layer* layer, int level, int child, bool instance) -> bool
        {
            const Layer::Type layerType = layer->GetType();
            if (layerType == Layer::Type::Paint)   layerPaintRender->UnloadInCPU(log, layer);
            if (layerType == Layer::Type::Picture) layerRenderPicture->UnloadInCPU(log, layer);
            return true;
        }, false, false, false, false);

        // player
        mPlayerManager.Deinit();

        // sequence
        mSequence.Deinit(log);

        std::chrono::steady_clock::time_point timeEnd = std::chrono::steady_clock::now();

        log->Printf(LT_MESSAGE, L"Unloaded in CPU [%d]! (%d ms)", mID, static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(timeEnd - timeStart).count()));
    }


    bool Document::iLoadGPU(LayerRendererPaint *layerPaintRender,
        LayerRendererPicture *layerRenderPicture,
        LayerRendererModel *layerRenderModel,
        piRenderer* renderer,
        piLog *log, Drawing::ColorSpace colorSpace)
    {
        std::chrono::steady_clock::time_point timeStart = std::chrono::steady_clock::now();

        log->Printf(LT_MESSAGE, L"Loading in GPU [%d]...", mID);
        auto loadInGPU = [this, colorSpace, renderer, log, layerPaintRender, layerRenderPicture, layerRenderModel](Layer* layer, int level, int child, bool instance) -> bool {
            const Layer::Type layerType = layer->GetType();
            switch (layerType)
            {
            case Layer::Type::Paint:
                if (iEnvFlagEnabled("IMM_UNITY_SKIP_GPU_LOAD_PAINT"))
                {
                    break;
                }
                if (!layerPaintRender->LoadInGPU(renderer, nullptr, log, layer))
                {
                    log->Printf(LT_ERROR, L"Could not load in GPU [%d] Paint layer %s", mID, layer->GetFullName()->GetS());
                    return false;
                }
                break;
            case Layer::Type::Picture:
                if (iEnvFlagEnabled("IMM_UNITY_SKIP_GPU_LOAD_PICTURE"))
                {
                    break;
                }
                if (!layerRenderPicture->LoadInGPU(renderer, nullptr, log, layer))
                {
                    log->Printf(LT_ERROR, L"Could not load in GPU [%d] Picture layer %s", mID, layer->GetFullName()->GetS());
                    return false;
                }
                break;
            case Layer::Type::Model:
                if (iEnvFlagEnabled("IMM_UNITY_SKIP_GPU_LOAD_MODEL"))
                {
                    break;
                }
                if (!layerRenderModel->LoadInGPU(renderer, nullptr, log, layer))
                {
                    log->Printf(LT_ERROR, L"Could not load in GPU [%d] Model layer %s", mID, layer->GetFullName()->GetS());
                    return false;
                }
                break;
            default:
                break;
            }
            return true;
        };
        if (!mSequence.Recurse(loadInGPU, false, false, false, false))
        {
            log->Printf(LT_ERROR, L"Error loading in GPU [%d]...", mID);
            return false;
        }

        std::chrono::steady_clock::time_point timeEnd = std::chrono::steady_clock::now();

        const int ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(timeEnd - timeStart).count());
        log->Printf(LT_MESSAGE, L"Loaded in GPU [%d]! (%d ms)", mID, ms);

        return true;
    }

    bool Document::iUnloadGPU(LayerRendererPaint *layerPaintRender,
        LayerRendererPicture *layerRenderPicture,
        LayerRendererModel *layerRenderModel,
        piRenderer* renderer, piLog *log)
    {
        std::chrono::steady_clock::time_point timeStart = std::chrono::steady_clock::now();

        log->Printf(LT_MESSAGE, L"Unloading GPU [%d]...", mID);

        auto unloadInGPU = [this, renderer, log, layerPaintRender, layerRenderPicture, layerRenderModel ](Layer* layer, int level, int child, bool instance) -> bool {
            const Layer::Type layerType = layer->GetType();
            switch (layerType)
            {
            case Layer::Type::Paint:
                if (!layerPaintRender->UnloadInGPU(renderer, nullptr, log, layer)) return false;
                break;
            case Layer::Type::Picture:
                if (!layerRenderPicture->UnloadInGPU(renderer, nullptr, log, layer)) return false;
                break;
            case Layer::Type::Model:
                if (!layerRenderModel->UnloadInGPU(renderer, nullptr, log, layer)) return false;
                break;
            default:
                break;
            }
            return true;
        };
        if (!mSequence.Recurse(unloadInGPU, false, false, false, false))
        {
            log->Printf(LT_ERROR, L"Error loading in GPU [%d]...", mID);
            return false;
        }



        std::chrono::steady_clock::time_point timeEnd = std::chrono::steady_clock::now();

        log->Printf(LT_MESSAGE, L"Unloaded in GPU [%d]! (%d ms)", mID, static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(timeEnd - timeStart).count()));

        return true;
    }

    int Document::GetChapterCount(void) const
    {
        return static_cast<int>(mPlayerManager.GetChapterCount());
    }

    int Document::GetCurrentChapter(void) const
    {
        return static_cast<int>(mPlayerManager.GetCurrentChapter());
    }

	bool Document::GetHasPlays(void) const
	{
		return mPlayerManager.GetHasPlays();
	}

    bool Document::GetHasAudio(void)
    {
        return mHasAudio;
    }

    void Document::CancelLoading(void)
    {
        StopLoadingAsync();
    }

    float Document::GetVolume(void) const
    {
        return mMasterVolume;
    }

    void Document::SetVolume( float volume, piLog *log )
    {
        // clamp01
        if (volume > 1.0f) volume = 1.0f;
        if (volume < 0.0f) volume = 0.0f;

        mMasterVolume = volume;
    }

    bound3d Document::GetBBox(void) const
    {
        bound3d bbox = mSequence.GetRoot()->GetBBox();
        bbox = btransform(bbox, GetDocumentToWorld());
        return bbox;
    }

    void Document::UnloadCPU(LayerRendererPaint * layerRendererPaint, LayerRendererPicture * layerRendererPicture, piLog * log)
    {
        iUnloadCPU(log, layerRendererPaint, layerRendererPicture);
    }

    void Document::SetCommandId(int id)
    {
        mCmdID = id;
    }

    int Document::GetCommandId(void)
    {
        return mCmdID;
    }
}
