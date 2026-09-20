#include <stdlib.h>
#include <thread>
#include <chrono>
#include <cmath>


#include "libImmCore/src/libBasics/piDebug.h"

#include "libImmImporter/src/document/layer.h"
#include "libImmImporter/src/document/layerEffect.h"
#include "libImmImporter/src/document/layerInstance.h"
#include "libImmImporter/src/document/layerPaint.h"
#include "libImmImporter/src/document/layerPaintStatic.h"
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

    static Layer * iFindAuthoringLayerById(Layer * layer, uint32_t layerId)
    {
        if (layer == nullptr)
            return nullptr;
        if (layer->GetID() == layerId)
            return layer;
        for (uint32_t childIndex = 0; childIndex < layer->GetNumChildren(); childIndex++)
        {
            Layer * result = iFindAuthoringLayerById(layer->GetChild(childIndex), layerId);
            if (result != nullptr)
                return result;
        }
        return nullptr;
    }

    static uint32_t iNextAuthoringLayerId(Layer * layer)
    {
        if (layer == nullptr)
            return 1;
        uint32_t next = layer->GetID() + 1;
        for (uint32_t childIndex = 0; childIndex < layer->GetNumChildren(); childIndex++)
            next = std::max(next, iNextAuthoringLayerId(layer->GetChild(childIndex)));
        return next;
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
        mNextLayerHandle = 1;
        return true;
    }

    void Document::End(void)
    {
		mOpenDrawingCreations.clear();
        mOpenGeometryEdits.clear();
        mOpenFrameMappings.clear();
        mOpenDrawingDeletions.clear();
        mOpenLayerPropertyEdits.clear();
        mOpenAnimationKeyEdits.clear();
        mOpenInitialSpawnAreaEdits.clear();
        mOpenSpawnAreaEdits.clear();
        mOpenLayerCreations.clear();
        mSealedBatches.clear();
        mCommitStatuses.clear();
        mDrawingHandles.clear();
        if (mPendingPresentation.mReplacement)
            mPendingPresentation.mReplacement->Deinit();
        if (mPendingPresentation.mCreatedLayer)
            mPendingPresentation.mCreatedLayer->Deinit(nullptr);
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

    int Document::GetInitialSpawnAreaLayerId() const
    {
        const Layer * layer = mSequence.GetInitialSpawnArea();
        return layer != nullptr ? static_cast<int>(layer->GetID()) : -1;
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
            if (mPendingPresentation.mIsLayerCreation)
            {
                Layer * parent = mPendingPresentation.mLayerCreationParent;
                Layer * created = mPendingPresentation.mCreatedLayer.get();
                const uint64_t layerId = mPendingPresentation.mObject;
                bool published = parent != nullptr && created != nullptr &&
                    mSequence.PublishPreparedLayer(created);
                if (published && !parent->PublishPreparedChild(created))
                {
                    mSequence.RollbackPreparedLayer(created);
                    published = false;
                }
                if (!published)
                {
                    iRejectCommit(revision, -5, 0, layerId);
                    if (created != nullptr)
                        created->Deinit(log);
                    mPendingPresentation = PendingPresentation{};
                }
                else
                {
                    mPendingPresentation.mCreatedLayer.release();
                    mPendingPresentation = PendingPresentation{};
                    if (status != nullptr)
                        status->mState = AuthoringCommitState::Presented;
                    mPresentedRevision = revision;
                    log->Printf(LT_MESSAGE,
                        L"[IMM_LIVE_EDIT] presented revision=%llu createdGroupLayer=%llu",
                        static_cast<unsigned long long>(revision),
                        static_cast<unsigned long long>(layerId));
                }
            }
            else if (mPendingPresentation.mIsSpawnAreaEdit)
            {
                Layer * layer = mPendingPresentation.mLayer;
                const uint64_t layerId = mPendingPresentation.mObject;
                LayerSpawnArea * spawnArea = layer != nullptr &&
                    layer->GetType() == Layer::Type::SpawnArea ?
                    (LayerSpawnArea *)layer->GetImplementation() : nullptr;
                if (spawnArea == nullptr)
                {
                    iRejectCommit(revision, layer == nullptr ? -1 : -3, 0, layerId);
                    mPendingPresentation = PendingPresentation{};
                }
                else
                {
                    spawnArea->SetCanonical(mPendingPresentation.mSpawnAreaVolume,
                        mPendingPresentation.mSpawnAreaTracking);
                    layer->SetCanonicalTransform(mPendingPresentation.mSpawnAreaTransform);
                    mSequence.SetSpawnAreaNeedsUpdate(true);
                    mPendingPresentation = PendingPresentation{};
                    if (status != nullptr)
                        status->mState = AuthoringCommitState::Presented;
                    mPresentedRevision = revision;
                    log->Printf(LT_MESSAGE,
                        L"[IMM_LIVE_EDIT] presented revision=%llu spawnAreaLayer=%llu",
                        static_cast<unsigned long long>(revision),
                        static_cast<unsigned long long>(layerId));
                }
            }
            else if (mPendingPresentation.mIsInitialSpawnAreaEdit)
            {
                Layer * layer = mPendingPresentation.mLayer;
                const uint64_t layerId = mPendingPresentation.mObject;
                if (layer == nullptr || layer->GetType() != Layer::Type::SpawnArea)
                {
                    iRejectCommit(revision, layer == nullptr ? -1 : -3, 0, layerId);
                    mPendingPresentation = PendingPresentation{};
                }
                else
                {
                    mSequence.SetInitialSpawnArea(layer);
                    mSequence.SetSpawnAreaNeedsUpdate(true);
                    mPendingPresentation = PendingPresentation{};
                    if (status != nullptr)
                        status->mState = AuthoringCommitState::Presented;
                    mPresentedRevision = revision;
                    log->Printf(LT_MESSAGE,
                        L"[IMM_LIVE_EDIT] presented revision=%llu initialSpawnAreaLayer=%llu",
                        static_cast<unsigned long long>(revision),
                        static_cast<unsigned long long>(layerId));
                }
            }
            else if (mPendingPresentation.mIsAnimationKeyEdit)
            {
                Layer * layer = mPendingPresentation.mLayer;
                const uint64_t layerId = mPendingPresentation.mObject;
                if (layer == nullptr)
                {
                    iRejectCommit(revision, -1, 0, layerId);
                    mPendingPresentation = PendingPresentation{};
                }
                else
                {
                    const Layer::AnimProperty property =
                        mPendingPresentation.mAnimationProperty;
                    layer->ReplaceAnimKeys(
                        property, std::move(mPendingPresentation.mAnimationKeys));
                    mPendingPresentation = PendingPresentation{};
                    if (status != nullptr)
                        status->mState = AuthoringCommitState::Presented;
                    mPresentedRevision = revision;
                    log->Printf(LT_MESSAGE,
                        L"[IMM_LIVE_EDIT] presented revision=%llu layer=%llu animationProperty=%d",
                        static_cast<unsigned long long>(revision),
                        static_cast<unsigned long long>(layerId),
                        static_cast<int>(property));
                }
            }
            else if (mPendingPresentation.mIsLayerPropertyEdit)
            {
                Layer * layer = mPendingPresentation.mLayer;
                const uint64_t layerId = mPendingPresentation.mObject;
                if (layer == nullptr)
                {
                    iRejectCommit(revision, -1, 0, layerId);
                    mPendingPresentation = PendingPresentation{};
                }
                else
                {
                    if (mPendingPresentation.mSetLayerVisibility)
                        layer->SetCanonicalVisible(mPendingPresentation.mLayerVisible);
                    if (mPendingPresentation.mSetLayerOpacity)
                        layer->SetCanonicalOpacity(mPendingPresentation.mLayerOpacity);
                    if (mPendingPresentation.mSetLayerTransform)
                        layer->SetCanonicalTransform(mPendingPresentation.mLayerTransform);
                    mPendingPresentation = PendingPresentation{};
                    if (status != nullptr)
                        status->mState = AuthoringCommitState::Presented;
                    mPresentedRevision = revision;
                    log->Printf(LT_MESSAGE,
                        L"[IMM_LIVE_EDIT] presented revision=%llu layer=%llu canonicalVisible=%d canonicalOpacity=%.3f",
                        static_cast<unsigned long long>(revision),
                        static_cast<unsigned long long>(layerId),
                        layer->GetCanonicalVisible() ? 1 : 0,
                        layer->GetCanonicalOpacity());
                }
            }
            else if (mPendingPresentation.mIsDeletion)
            {
                LayerPaint * paint = mPendingPresentation.mLayer != nullptr ?
                    (LayerPaint *)mPendingPresentation.mLayer->GetImplementation() : nullptr;
                uint32_t * frames = paint != nullptr ? paint->GetFrameBuffer() : nullptr;
                const bool hasValidMapping = !mPendingPresentation.mIsFrameMapping ||
                    (frames != nullptr &&
                        mPendingPresentation.mFrameIndex < paint->GetNumFrames() &&
                        mPendingPresentation.mMappedDrawingIndex < paint->GetNumDrawings());
                if (hasValidMapping && mPendingPresentation.mIsFrameMapping)
                    frames[mPendingPresentation.mFrameIndex] =
                        mPendingPresentation.mMappedDrawingIndex;
                Drawing * removed = paint != nullptr && layerPaintRender != nullptr ?
                    (hasValidMapping ? paint->ExtractDrawing(
                        mPendingPresentation.mDrawingIndex) : nullptr) : nullptr;
                if (removed == nullptr)
                {
                    if (frames != nullptr && mPendingPresentation.mIsFrameMapping &&
                        mPendingPresentation.mFrameIndex < paint->GetNumFrames())
                        frames[mPendingPresentation.mFrameIndex] =
                            mPendingPresentation.mPreviousFrameDrawingIndex;
                    iRejectCommit(revision, layerPaintRender == nullptr ? -10 : -6, 0,
                        mPendingPresentation.mObject);
                    mPendingPresentation = PendingPresentation{};
                }
                else
                {
                    const uint64_t deletedHandle = mPendingPresentation.mObject;
                    Layer * deletedLayer = mPendingPresentation.mLayer;
                    const uint32_t deletedIndex = mPendingPresentation.mDrawingIndex;
                    const bool presentedMapping = mPendingPresentation.mIsFrameMapping;
                    const uint32_t presentedFrame = mPendingPresentation.mFrameIndex;
                    layerPaintRender->PresentDrawingDeletion(removed, log);
                    for (size_t handleIndex = 0; handleIndex < mDrawingHandles.size();)
                    {
                        DrawingHandleEntry & entry = mDrawingHandles[handleIndex];
                        if (entry.mHandle == deletedHandle)
                        {
                            mDrawingHandles.erase(mDrawingHandles.begin() + handleIndex);
                            continue;
                        }
                        if (entry.mLayer == deletedLayer && entry.mDrawingIndex > deletedIndex)
                            entry.mDrawingIndex--;
                        handleIndex++;
                    }
                    mPendingPresentation = PendingPresentation{};
                    if (status != nullptr)
                        status->mState = AuthoringCommitState::Presented;
                    mPresentedRevision = revision;
                    log->Printf(LT_MESSAGE,
                        L"[IMM_LIVE_EDIT] presented revision=%llu deletedDrawing=%llu remappedFrame=%d frame=%u",
                        static_cast<unsigned long long>(revision),
                        static_cast<unsigned long long>(deletedHandle),
                        presentedMapping ? 1 : 0, presentedFrame);
                }
            }
            else if (mPendingPresentation.mIsFrameMapping &&
                !mPendingPresentation.mIsCreation)
            {
                LayerPaint * paint = mPendingPresentation.mLayer != nullptr ?
                    (LayerPaint *)mPendingPresentation.mLayer->GetImplementation() : nullptr;
                uint32_t * frames = paint != nullptr ? paint->GetFrameBuffer() : nullptr;
                if (frames == nullptr || mPendingPresentation.mFrameIndex >= paint->GetNumFrames() ||
                    mPendingPresentation.mDrawingIndex >= paint->GetNumDrawings())
                {
                    iRejectCommit(revision, -6, 0, mPendingPresentation.mObject);
                    mPendingPresentation = PendingPresentation{};
                }
                else
                {
                    frames[mPendingPresentation.mFrameIndex] = mPendingPresentation.mDrawingIndex;
                    const uint32_t presentedFrame = mPendingPresentation.mFrameIndex;
                    const uint64_t presentedDrawing = mPendingPresentation.mObject;
                    mPendingPresentation = PendingPresentation{};
                    if (status != nullptr)
                        status->mState = AuthoringCommitState::Presented;
                    mPresentedRevision = revision;
                    log->Printf(LT_MESSAGE,
                        L"[IMM_LIVE_EDIT] presented revision=%llu frame=%u drawing=%llu",
                        static_cast<unsigned long long>(revision), presentedFrame,
                        static_cast<unsigned long long>(presentedDrawing));
                }
            }
            else if (layerPaintRender == nullptr ||
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
                Drawing * created = nullptr;
                uint32_t * creationFrames = nullptr;
                if (mPendingPresentation.mIsCreation)
                {
                    LayerPaint * paint = mPendingPresentation.mLayer != nullptr ?
                        (LayerPaint *)mPendingPresentation.mLayer->GetImplementation() : nullptr;
                    creationFrames = paint != nullptr ? paint->GetFrameBuffer() : nullptr;
                    if (mPendingPresentation.mIsFrameMapping &&
                        (creationFrames == nullptr ||
                            mPendingPresentation.mFrameIndex >= paint->GetNumFrames()))
                    {
                        iRejectCommit(revision, -6, 0, mPendingPresentation.mObject);
                        iDiscardPendingPresentation(layerPaintRender, renderer, log);
                        return true;
                    }
                    created = paint != nullptr ? paint->AddDrawing() : nullptr;
                    if (created == nullptr)
                    {
                        iRejectCommit(revision, -5, 0, mPendingPresentation.mObject);
                        iDiscardPendingPresentation(layerPaintRender, renderer, log);
                        return true;
                    }
                }

                const bool presented = mPendingPresentation.mIsCreation ?
                    layerPaintRender->PresentDrawingCreation(created, replacement,
                        mPendingPresentation.mRendererToken, revision, log) :
                    layerPaintRender->PresentDrawingReplacement(mPendingPresentation.mActive,
                        replacement, mPendingPresentation.mRendererToken, revision, log);
                if (!presented)
                {
                    if (created != nullptr)
                    {
                        LayerPaint * paint = (LayerPaint *)mPendingPresentation.mLayer->GetImplementation();
                        paint->RemoveLastDrawing(created);
                    }
                    iRejectCommit(revision, -10, 0, mPendingPresentation.mObject);
                    iDiscardPendingPresentation(layerPaintRender, renderer, log);
                }
                else
                {
                    if (mPendingPresentation.mIsCreation)
                    {
                        mDrawingHandles.push_back(DrawingHandleEntry{
                            mPendingPresentation.mObject, mPendingPresentation.mLayer,
                            mPendingPresentation.mDrawingIndex });
                        if (mPendingPresentation.mIsFrameMapping)
                            creationFrames[mPendingPresentation.mFrameIndex] =
                                mPendingPresentation.mDrawingIndex;
                        mPendingPresentation.mReplacement->Deinit();
                    }
                    else
                    {
                        // Replacement presentation owns the object containing the retired
                        // geometry until the renderer retirement policy fires.
                        mPendingPresentation.mReplacement.release();
                    }
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
        mNextLayerHandle = iNextAuthoringLayerId(mSequence.GetRoot());
        mEditing = true;
        return true;
    }

    bool Document::DetachEditing(void)
    {
        if (!mEditing || !mOpenDrawingCreations.empty() || !mOpenGeometryEdits.empty() ||
            !mOpenFrameMappings.empty() || !mSealedBatches.empty() ||
            !mOpenDrawingDeletions.empty() ||
            !mOpenLayerPropertyEdits.empty() ||
            !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() ||
            !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty() ||
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
        mOpenDrawingCreations.clear();
        mOpenGeometryEdits.clear();
        mOpenFrameMappings.clear();
        mOpenDrawingDeletions.clear();
        mOpenLayerPropertyEdits.clear();
        mOpenAnimationKeyEdits.clear();
        mOpenInitialSpawnAreaEdits.clear();
        mOpenSpawnAreaEdits.clear();
        mOpenLayerCreations.clear();
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

    const Document::DrawingCreation * Document::iFindOpenDrawingCreation(
        uint32_t layerId, uint64_t drawingId) const
    {
        for (const DrawingCreation & creation : mOpenDrawingCreations)
        {
            if (creation.mLayerId == layerId && creation.mDrawingId == drawingId)
                return &creation;
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

    bool Document::GetFrameDrawingHandle(
        uint32_t layerId, uint32_t frameIndex, uint64_t * drawingIdOut) const
    {
        if (!mEditing || drawingIdOut == nullptr)
            return false;
        Layer * layer = iFindAuthoringLayerById(mSequence.GetRoot(), layerId);
        if (layer == nullptr || layer->GetType() != Layer::Type::Paint)
            return false;
        LayerPaint * paint = (LayerPaint *)layer->GetImplementation();
        if (paint == nullptr || frameIndex >= paint->GetNumFrames())
            return false;
        const uint32_t * frames = paint->GetFrameBuffer();
        if (frames == nullptr)
            return false;
        return GetDrawingHandle(layerId, frames[frameIndex], drawingIdOut);
    }

    int32_t Document::QueueDrawingCreation(uint32_t layerId, uint64_t * drawingIdOut)
    {
        if (!mEditing)
            return -4;
        if (drawingIdOut == nullptr)
            return -2;
        if (!mOpenDrawingCreations.empty() || !mOpenGeometryEdits.empty() ||
            !mOpenFrameMappings.empty() || !mOpenDrawingDeletions.empty() ||
            !mOpenLayerPropertyEdits.empty() || !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() || !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty())
            return -7;

        Layer * layer = iFindAuthoringLayerById(mSequence.GetRoot(), layerId);
        if (layer == nullptr)
            return -1;
        if (layer->GetType() != Layer::Type::Paint ||
            dynamic_cast<LayerPaintStatic *>((LayerPaint *)layer->GetImplementation()) == nullptr)
            return -3;

        const uint64_t drawingId = mNextDrawingHandle++;
        mOpenDrawingCreations.push_back(DrawingCreation{ layerId, drawingId });
        *drawingIdOut = drawingId;
        return 0;
    }

    int32_t Document::QueueDrawingDeletion(uint32_t layerId, uint64_t drawingId)
    {
        if (!mEditing)
            return -4;
        if (!mOpenDrawingCreations.empty() || !mOpenGeometryEdits.empty() ||
            mOpenFrameMappings.size() > 1 || !mOpenDrawingDeletions.empty() ||
            !mOpenLayerPropertyEdits.empty() || !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() || !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty())
            return -7;

        const DrawingHandleEntry * entry = iFindDrawingHandle(layerId, drawingId);
        if (entry == nullptr || entry->mLayer == nullptr)
            return -1;
        LayerPaint * paint = entry->mLayer->GetType() == Layer::Type::Paint ?
            (LayerPaint *)entry->mLayer->GetImplementation() : nullptr;
        if (paint == nullptr || entry->mDrawingIndex >= paint->GetNumDrawings() ||
            dynamic_cast<DrawingStatic *>(
                paint->GetDrawing(static_cast<int>(entry->mDrawingIndex))) == nullptr)
            return -3;
        const FrameMapping * mapping = mOpenFrameMappings.empty() ?
            nullptr : &mOpenFrameMappings[0];
        const DrawingHandleEntry * mappedEntry = mapping != nullptr ?
            iFindDrawingHandle(mapping->mLayerId, mapping->mDrawingId) : nullptr;
        if (mapping != nullptr && (mapping->mLayerId != layerId || mappedEntry == nullptr ||
            mappedEntry->mLayer != entry->mLayer ||
            mappedEntry->mDrawingIndex == entry->mDrawingIndex))
            return -6;
        const uint32_t * frames = paint->GetFrameBuffer();
        for (uint32_t frameIndex = 0; frameIndex < paint->GetNumFrames(); frameIndex++)
        {
            const bool remapped = mapping != nullptr && mapping->mFrameIndex == frameIndex;
            if (frames == nullptr || (!remapped && frames[frameIndex] == entry->mDrawingIndex))
                return -6;
        }

        mOpenDrawingDeletions.push_back(DrawingDeletion{ layerId, drawingId });
        return 0;
    }

    int32_t Document::QueueDrawingGeometry(uint32_t layerId, uint64_t drawingId,
        std::vector<AuthoringElementGeometry> elements,
        Drawing::ColorSpace colorSpace, bool flipped, float biggestStroke)
    {
        if (!mEditing)
            return -4;
        if (elements.empty() || biggestStroke <= 0.0f)
            return -2;
        if (iFindDrawingHandle(layerId, drawingId) == nullptr &&
            iFindOpenDrawingCreation(layerId, drawingId) == nullptr)
            return -1;
        if (!mOpenFrameMappings.empty() || !mOpenDrawingDeletions.empty() ||
            !mOpenLayerPropertyEdits.empty() || !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() || !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty())
            return -7;

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

    int32_t Document::QueueFrameMapping(
        uint32_t layerId, uint32_t frameIndex, uint64_t drawingId)
    {
        if (!mEditing)
            return -4;
        if (!mOpenFrameMappings.empty() || !mOpenDrawingDeletions.empty() ||
            !mOpenLayerPropertyEdits.empty() || !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() || !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty())
            return -7;

        if (!mOpenDrawingCreations.empty())
        {
            if (mOpenDrawingCreations.size() != 1 || mOpenGeometryEdits.size() != 1 ||
                mOpenDrawingCreations[0].mLayerId != layerId ||
                mOpenDrawingCreations[0].mDrawingId != drawingId ||
                mOpenGeometryEdits[0].mLayerId != layerId ||
                mOpenGeometryEdits[0].mDrawingId != drawingId)
                return -7;
            Layer * layer = iFindAuthoringLayerById(mSequence.GetRoot(), layerId);
            LayerPaint * paint = layer != nullptr && layer->GetType() == Layer::Type::Paint ?
                (LayerPaint *)layer->GetImplementation() : nullptr;
            if (paint == nullptr)
                return -3;
            if (frameIndex >= paint->GetNumFrames())
                return -2;

            mOpenFrameMappings.push_back(FrameMapping{ layerId, frameIndex, drawingId });
            return 0;
        }
        if (!mOpenGeometryEdits.empty())
            return -7;

        const DrawingHandleEntry * entry = iFindDrawingHandle(layerId, drawingId);
        if (entry == nullptr || entry->mLayer == nullptr)
            return -1;
        LayerPaint * paint = entry->mLayer->GetType() == Layer::Type::Paint ?
            (LayerPaint *)entry->mLayer->GetImplementation() : nullptr;
        if (paint == nullptr || entry->mDrawingIndex >= paint->GetNumDrawings() ||
            dynamic_cast<DrawingStatic *>(
            paint->GetDrawing(static_cast<int>(entry->mDrawingIndex))) == nullptr)
            return -3;
        if (frameIndex >= paint->GetNumFrames())
            return -2;

        mOpenFrameMappings.push_back(FrameMapping{ layerId, frameIndex, drawingId });
        return 0;
    }

    int32_t Document::QueueLayerProperties(uint32_t layerId,
        bool setVisibility, bool visible, bool setOpacity, float opacity,
        bool setTransform, const trans3d & transform)
    {
        if (!mEditing)
            return -4;
        if (!mOpenDrawingCreations.empty() || !mOpenGeometryEdits.empty() ||
            !mOpenFrameMappings.empty() || !mOpenDrawingDeletions.empty() ||
            !mOpenLayerPropertyEdits.empty() || !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() || !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty())
            return -7;
        Layer * layer = iFindAuthoringLayerById(mSequence.GetRoot(), layerId);
        if (layer == nullptr)
            return -1;
        if (!setVisibility && !setOpacity && !setTransform)
            return -2;
        if (setOpacity && (!std::isfinite(opacity) || opacity < 0.0f || opacity > 1.0f))
            return -2;
        if (setTransform)
        {
            const double quaternionLengthSquared =
                transform.mRotation.x * transform.mRotation.x +
                transform.mRotation.y * transform.mRotation.y +
                transform.mRotation.z * transform.mRotation.z +
                transform.mRotation.w * transform.mRotation.w;
            if (!std::isfinite(transform.mTranslation.x) ||
                !std::isfinite(transform.mTranslation.y) ||
                !std::isfinite(transform.mTranslation.z) ||
                !std::isfinite(transform.mRotation.x) ||
                !std::isfinite(transform.mRotation.y) ||
                !std::isfinite(transform.mRotation.z) ||
                !std::isfinite(transform.mRotation.w) ||
                !std::isfinite(transform.mScale) || transform.mScale <= 0.0 ||
                std::fabs(quaternionLengthSquared - 1.0) > 0.001 ||
                transform.mFlip != flip3::N)
                return -2;
        }
        mOpenLayerPropertyEdits.push_back(LayerPropertyEdit{
            layerId, setVisibility, visible, setOpacity, opacity, setTransform, transform });
        return 0;
    }

    int32_t Document::QueueAnimationKey(uint32_t layerId, Layer::AnimProperty property,
        piTick time, const Layer::AnimValue & value, Layer::InterpolationType interpolation)
    {
        if (!mEditing)
            return -4;
        if (!mOpenDrawingCreations.empty() || !mOpenGeometryEdits.empty() ||
            !mOpenFrameMappings.empty() || !mOpenDrawingDeletions.empty() ||
            !mOpenLayerPropertyEdits.empty() || !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() || !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty())
            return -7;
        Layer * layer = iFindAuthoringLayerById(mSequence.GetRoot(), layerId);
        if (layer == nullptr)
            return -1;
        if (property != Layer::AnimProperty::Visibility &&
            property != Layer::AnimProperty::Opacity &&
            property != Layer::AnimProperty::Transform)
            return -3;
        if (time < piTick(0))
            return -2;
        if (interpolation < Layer::InterpolationType::None ||
            interpolation >= Layer::InterpolationType::MAX)
            return -2;
        if (property == Layer::AnimProperty::Opacity &&
            (!std::isfinite(value.mFloat) || value.mFloat < 0.0f || value.mFloat > 1.0f))
            return -2;
        if (property == Layer::AnimProperty::Transform)
        {
            const trans3d & transform = value.mTransform;
            const double quaternionLengthSquared =
                transform.mRotation.x * transform.mRotation.x +
                transform.mRotation.y * transform.mRotation.y +
                transform.mRotation.z * transform.mRotation.z +
                transform.mRotation.w * transform.mRotation.w;
            if (!std::isfinite(transform.mTranslation.x) ||
                !std::isfinite(transform.mTranslation.y) ||
                !std::isfinite(transform.mTranslation.z) ||
                !std::isfinite(transform.mRotation.x) ||
                !std::isfinite(transform.mRotation.y) ||
                !std::isfinite(transform.mRotation.z) ||
                !std::isfinite(transform.mRotation.w) ||
                !std::isfinite(transform.mScale) || transform.mScale <= 0.0 ||
                std::fabs(quaternionLengthSquared - 1.0) > 0.001 ||
                transform.mFlip != flip3::N)
                return -2;
        }
        mOpenAnimationKeyEdits.push_back(AnimationKeyEdit{
            layerId, property, time, value, interpolation, false });
        return 0;
    }

    int32_t Document::QueueAnimationKeyRemoval(uint32_t layerId,
        Layer::AnimProperty property, piTick time)
    {
        if (!mEditing)
            return -4;
        if (!mOpenDrawingCreations.empty() || !mOpenGeometryEdits.empty() ||
            !mOpenFrameMappings.empty() || !mOpenDrawingDeletions.empty() ||
            !mOpenLayerPropertyEdits.empty() || !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() || !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty())
            return -7;
        if (iFindAuthoringLayerById(mSequence.GetRoot(), layerId) == nullptr)
            return -1;
        if (property != Layer::AnimProperty::Visibility &&
            property != Layer::AnimProperty::Opacity &&
            property != Layer::AnimProperty::Transform)
            return -3;
        if (time < piTick(0))
            return -2;
        Layer::AnimValue value;
        value.init();
        mOpenAnimationKeyEdits.push_back(AnimationKeyEdit{
            layerId, property, time, value, Layer::InterpolationType::None, true });
        return 0;
    }

    int32_t Document::QueueInitialSpawnArea(uint32_t layerId)
    {
        if (!mEditing)
            return -4;
        if (!mOpenDrawingCreations.empty() || !mOpenGeometryEdits.empty() ||
            !mOpenFrameMappings.empty() || !mOpenDrawingDeletions.empty() ||
            !mOpenLayerPropertyEdits.empty() || !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() || !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty())
            return -7;
        Layer * layer = iFindAuthoringLayerById(mSequence.GetRoot(), layerId);
        if (layer == nullptr)
            return -1;
        if (layer->GetType() != Layer::Type::SpawnArea)
            return -3;
        mOpenInitialSpawnAreaEdits.push_back(InitialSpawnAreaEdit{ layerId });
        return 0;
    }

    int32_t Document::QueueSpawnArea(uint32_t layerId,
        const LayerSpawnArea::Volume & volume, LayerSpawnArea::TrackingLevel tracking,
        const trans3d & transform)
    {
        if (!mEditing)
            return -4;
        if (!mOpenDrawingCreations.empty() || !mOpenGeometryEdits.empty() ||
            !mOpenFrameMappings.empty() || !mOpenDrawingDeletions.empty() ||
            !mOpenLayerPropertyEdits.empty() || !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() || !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty())
            return -7;
        Layer * layer = iFindAuthoringLayerById(mSequence.GetRoot(), layerId);
        if (layer == nullptr)
            return -1;
        if (layer->GetType() != Layer::Type::SpawnArea || layer->GetImplementation() == nullptr)
            return -3;
        if (tracking != LayerSpawnArea::TrackingLevel::Floor &&
            tracking != LayerSpawnArea::TrackingLevel::Eye)
            return -2;
        if (volume.mType == LayerSpawnArea::Volume::Type::Sphere)
        {
            const vec4 & sphere = volume.mShape.mSphere;
            if (!std::isfinite(sphere.x) || !std::isfinite(sphere.y) ||
                !std::isfinite(sphere.z) || !std::isfinite(sphere.w) || sphere.w <= 0.0f)
                return -2;
        }
        else if (volume.mType == LayerSpawnArea::Volume::Type::Box)
        {
            const bound3 & box = volume.mShape.mBox;
            if (!std::isfinite(box.mMinX) || !std::isfinite(box.mMinY) ||
                !std::isfinite(box.mMinZ) || !std::isfinite(box.mMaxX) ||
                !std::isfinite(box.mMaxY) || !std::isfinite(box.mMaxZ) ||
                box.mMinX >= box.mMaxX || box.mMinY >= box.mMaxY ||
                box.mMinZ >= box.mMaxZ)
                return -2;
        }
        else
        {
            return -2;
        }
        const double quaternionLengthSquared =
            transform.mRotation.x * transform.mRotation.x +
            transform.mRotation.y * transform.mRotation.y +
            transform.mRotation.z * transform.mRotation.z +
            transform.mRotation.w * transform.mRotation.w;
        if (!std::isfinite(transform.mTranslation.x) ||
            !std::isfinite(transform.mTranslation.y) ||
            !std::isfinite(transform.mTranslation.z) ||
            !std::isfinite(transform.mRotation.x) ||
            !std::isfinite(transform.mRotation.y) ||
            !std::isfinite(transform.mRotation.z) ||
            !std::isfinite(transform.mRotation.w) ||
            !std::isfinite(transform.mScale) || transform.mScale <= 0.0 ||
            std::fabs(quaternionLengthSquared - 1.0) > 0.001 ||
            transform.mFlip != flip3::N)
            return -2;
        mOpenSpawnAreaEdits.push_back(SpawnAreaEdit{ layerId, volume, tracking, transform });
        return 0;
    }

    int32_t Document::QueueGroupLayerCreation(uint32_t parentLayerId,
        std::wstring name, uint32_t * layerIdOut)
    {
        if (!mEditing)
            return -4;
        if (layerIdOut == nullptr || name.empty())
            return -2;
        if (!mOpenDrawingCreations.empty() || !mOpenGeometryEdits.empty() ||
            !mOpenFrameMappings.empty() || !mOpenDrawingDeletions.empty() ||
            !mOpenLayerPropertyEdits.empty() || !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() || !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty())
            return -7;
        Layer * parent = iFindAuthoringLayerById(mSequence.GetRoot(), parentLayerId);
        if (parent == nullptr)
            return -1;
        if (parent->GetType() != Layer::Type::Group)
            return -3;
        if (mNextLayerHandle > static_cast<uint32_t>(INT32_MAX))
            return -5;
        const uint32_t layerId = mNextLayerHandle;
        mOpenLayerCreations.push_back(LayerCreation{
            layerId, parentLayerId, std::move(name) });
        mNextLayerHandle++;
        *layerIdOut = layerId;
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

        mOpenDrawingCreations.clear();
        mOpenGeometryEdits.clear();
        mOpenFrameMappings.clear();
        mOpenDrawingDeletions.clear();
        mOpenLayerPropertyEdits.clear();
        mOpenAnimationKeyEdits.clear();
        mOpenInitialSpawnAreaEdits.clear();
        mOpenSpawnAreaEdits.clear();
        mOpenLayerCreations.clear();
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

        if (batch.mLayerCreations.size() == 1 && batch.mSpawnAreaEdits.empty() &&
            batch.mInitialSpawnAreaEdits.empty() && batch.mAnimationKeyEdits.empty() &&
            batch.mLayerPropertyEdits.empty() && batch.mDrawingDeletions.empty() &&
            batch.mFrameMappings.empty() && batch.mGeometryEdits.empty() &&
            batch.mDrawingCreations.empty())
        {
            const LayerCreation & creation = batch.mLayerCreations[0];
            Layer * parent = iFindAuthoringLayerById(
                mSequence.GetRoot(), creation.mParentLayerId);
            if (parent == nullptr)
            {
                iRejectCommit(batch.mRevision, -1, 0, creation.mParentLayerId);
                return;
            }
            if (parent->GetType() != Layer::Type::Group)
            {
                iRejectCommit(batch.mRevision, -3, 0, creation.mParentLayerId);
                return;
            }
            if (!mSequence.PrepareLayerPublication() ||
                !parent->PrepareChildPublication())
            {
                iRejectCommit(batch.mRevision, -5, 0, creation.mLayerId);
                return;
            }
            std::unique_ptr<Layer> layer(new (std::nothrow) Layer(
                &mSequence, parent, creation.mLayerId));
            if (!layer || !layer->Init(Layer::Type::Group, creation.mName.c_str(), true,
                trans3d::identity(), trans3d::identity(), 1.0f, false, piTick(0), 0,
                UINT32_MAX, log, false))
            {
                iRejectCommit(batch.mRevision, -5, 0, creation.mLayerId);
                return;
            }
            if (status != nullptr)
            {
                status->mState = AuthoringCommitState::Prepared;
                status->mObject = creation.mLayerId;
            }
            mPreparedRevision = batch.mRevision;
            mPendingPresentation.mRevision = batch.mRevision;
            mPendingPresentation.mObject = creation.mLayerId;
            mPendingPresentation.mIsLayerCreation = true;
            mPendingPresentation.mLayerCreationParent = parent;
            mPendingPresentation.mCreatedLayer = std::move(layer);
            return;
        }

        if (batch.mSpawnAreaEdits.size() == 1 &&
            batch.mLayerCreations.empty() &&
            batch.mInitialSpawnAreaEdits.empty() && batch.mAnimationKeyEdits.empty() &&
            batch.mLayerPropertyEdits.empty() && batch.mDrawingDeletions.empty() &&
            batch.mFrameMappings.empty() && batch.mGeometryEdits.empty() &&
            batch.mDrawingCreations.empty())
        {
            const SpawnAreaEdit & edit = batch.mSpawnAreaEdits[0];
            Layer * layer = iFindAuthoringLayerById(mSequence.GetRoot(), edit.mLayerId);
            if (layer == nullptr)
            {
                iRejectCommit(batch.mRevision, -1, 0, edit.mLayerId);
                return;
            }
            if (layer->GetType() != Layer::Type::SpawnArea ||
                layer->GetImplementation() == nullptr)
            {
                iRejectCommit(batch.mRevision, -3, 0, edit.mLayerId);
                return;
            }
            if (status != nullptr)
            {
                status->mState = AuthoringCommitState::Prepared;
                status->mObject = edit.mLayerId;
            }
            mPreparedRevision = batch.mRevision;
            mPendingPresentation.mRevision = batch.mRevision;
            mPendingPresentation.mObject = edit.mLayerId;
            mPendingPresentation.mIsSpawnAreaEdit = true;
            mPendingPresentation.mSpawnAreaVolume = edit.mVolume;
            mPendingPresentation.mSpawnAreaTracking = edit.mTracking;
            mPendingPresentation.mSpawnAreaTransform = edit.mTransform;
            mPendingPresentation.mLayer = layer;
            return;
        }

        if (batch.mInitialSpawnAreaEdits.size() == 1 &&
            batch.mSpawnAreaEdits.empty() &&
            batch.mLayerCreations.empty() &&
            batch.mAnimationKeyEdits.empty() && batch.mLayerPropertyEdits.empty() &&
            batch.mDrawingDeletions.empty() && batch.mFrameMappings.empty() &&
            batch.mGeometryEdits.empty() && batch.mDrawingCreations.empty())
        {
            const InitialSpawnAreaEdit & edit = batch.mInitialSpawnAreaEdits[0];
            Layer * layer = iFindAuthoringLayerById(mSequence.GetRoot(), edit.mLayerId);
            if (layer == nullptr)
            {
                iRejectCommit(batch.mRevision, -1, 0, edit.mLayerId);
                return;
            }
            if (layer->GetType() != Layer::Type::SpawnArea)
            {
                iRejectCommit(batch.mRevision, -3, 0, edit.mLayerId);
                return;
            }
            if (status != nullptr)
            {
                status->mState = AuthoringCommitState::Prepared;
                status->mObject = edit.mLayerId;
            }
            mPreparedRevision = batch.mRevision;
            mPendingPresentation.mRevision = batch.mRevision;
            mPendingPresentation.mObject = edit.mLayerId;
            mPendingPresentation.mIsInitialSpawnAreaEdit = true;
            mPendingPresentation.mLayer = layer;
            return;
        }

        if (batch.mAnimationKeyEdits.size() == 1 && batch.mLayerPropertyEdits.empty() &&
            batch.mInitialSpawnAreaEdits.empty() &&
            batch.mSpawnAreaEdits.empty() &&
            batch.mLayerCreations.empty() &&
            batch.mDrawingDeletions.empty() && batch.mFrameMappings.empty() &&
            batch.mGeometryEdits.empty() && batch.mDrawingCreations.empty())
        {
            const AnimationKeyEdit & edit = batch.mAnimationKeyEdits[0];
            Layer * layer = iFindAuthoringLayerById(mSequence.GetRoot(), edit.mLayerId);
            std::vector<Layer::AnimKey> keys;
            if (layer == nullptr)
            {
                iRejectCommit(batch.mRevision, -1, 0, edit.mLayerId);
                return;
            }
            if (!layer->CopyAnimKeys(edit.mProperty, keys))
            {
                iRejectCommit(batch.mRevision, -5, 0, edit.mLayerId);
                return;
            }
            if (edit.mRemove)
            {
                if (!Layer::RemoveAnimKey(keys, edit.mTime))
                {
                    iRejectCommit(batch.mRevision, -1, 0, edit.mLayerId);
                    return;
                }
            }
            else if (!Layer::SetAnimKey(
                keys, edit.mTime, edit.mValue, edit.mInterpolation))
            {
                iRejectCommit(batch.mRevision, -5, 0, edit.mLayerId);
                return;
            }
            if (status != nullptr)
            {
                status->mState = AuthoringCommitState::Prepared;
                status->mObject = edit.mLayerId;
            }
            mPreparedRevision = batch.mRevision;
            mPendingPresentation.mRevision = batch.mRevision;
            mPendingPresentation.mObject = edit.mLayerId;
            mPendingPresentation.mIsAnimationKeyEdit = true;
            mPendingPresentation.mAnimationProperty = edit.mProperty;
            mPendingPresentation.mAnimationKeys = std::move(keys);
            mPendingPresentation.mLayer = layer;
            return;
        }

        if (batch.mLayerPropertyEdits.size() == 1 && batch.mAnimationKeyEdits.empty() &&
            batch.mInitialSpawnAreaEdits.empty() &&
            batch.mSpawnAreaEdits.empty() &&
            batch.mLayerCreations.empty() &&
            batch.mDrawingDeletions.empty() &&
            batch.mFrameMappings.empty() && batch.mGeometryEdits.empty() &&
            batch.mDrawingCreations.empty())
        {
            const LayerPropertyEdit & edit = batch.mLayerPropertyEdits[0];
            Layer * layer = iFindAuthoringLayerById(mSequence.GetRoot(), edit.mLayerId);
            if (layer == nullptr)
            {
                iRejectCommit(batch.mRevision, -1, 0, edit.mLayerId);
                return;
            }
            if (status != nullptr)
            {
                status->mState = AuthoringCommitState::Prepared;
                status->mObject = edit.mLayerId;
            }
            mPreparedRevision = batch.mRevision;
            mPendingPresentation.mRevision = batch.mRevision;
            mPendingPresentation.mObject = edit.mLayerId;
            mPendingPresentation.mIsLayerPropertyEdit = true;
            mPendingPresentation.mSetLayerVisibility = edit.mSetVisibility;
            mPendingPresentation.mLayerVisible = edit.mVisible;
            mPendingPresentation.mSetLayerOpacity = edit.mSetOpacity;
            mPendingPresentation.mLayerOpacity = edit.mOpacity;
            mPendingPresentation.mSetLayerTransform = edit.mSetTransform;
            mPendingPresentation.mLayerTransform = edit.mTransform;
            mPendingPresentation.mLayer = layer;
            return;
        }

        if (batch.mDrawingDeletions.size() == 1 && batch.mFrameMappings.size() <= 1 &&
            batch.mLayerPropertyEdits.empty() && batch.mAnimationKeyEdits.empty() &&
            batch.mInitialSpawnAreaEdits.empty() &&
            batch.mSpawnAreaEdits.empty() &&
            batch.mLayerCreations.empty() &&
            batch.mGeometryEdits.empty() && batch.mDrawingCreations.empty())
        {
            const DrawingDeletion & deletion = batch.mDrawingDeletions[0];
            const DrawingHandleEntry * entry = iFindDrawingHandle(
                deletion.mLayerId, deletion.mDrawingId);
            LayerPaint * paint = entry != nullptr && entry->mLayer != nullptr &&
                entry->mLayer->GetType() == Layer::Type::Paint ?
                (LayerPaint *)entry->mLayer->GetImplementation() : nullptr;
            Drawing * active = paint != nullptr && entry->mDrawingIndex < paint->GetNumDrawings() ?
                paint->GetDrawing(static_cast<int>(entry->mDrawingIndex)) : nullptr;
            const uint32_t * frames = paint != nullptr ? paint->GetFrameBuffer() : nullptr;
            const FrameMapping * mapping = batch.mFrameMappings.empty() ?
                nullptr : &batch.mFrameMappings[0];
            const DrawingHandleEntry * mappedEntry = mapping != nullptr ?
                iFindDrawingHandle(mapping->mLayerId, mapping->mDrawingId) : nullptr;
            const bool validMapping = mapping == nullptr ||
                (paint != nullptr && mappedEntry != nullptr &&
                    mapping->mLayerId == deletion.mLayerId &&
                    mappedEntry->mLayer == entry->mLayer &&
                    mapping->mFrameIndex < paint->GetNumFrames() &&
                    mappedEntry->mDrawingIndex < paint->GetNumDrawings() &&
                    mappedEntry->mDrawingIndex != entry->mDrawingIndex);
            bool referenced = frames == nullptr;
            for (uint32_t frameIndex = 0;
                paint != nullptr && frameIndex < paint->GetNumFrames(); frameIndex++)
            {
                const bool remapped = validMapping && mapping != nullptr &&
                    mapping->mFrameIndex == frameIndex;
                referenced = referenced ||
                    (!remapped && frames[frameIndex] == entry->mDrawingIndex);
            }
            if (paint == nullptr || !validMapping || referenced || layerPaintRender == nullptr ||
                !layerPaintRender->PrepareDrawingDeletion(active, log))
            {
                iRejectCommit(batch.mRevision,
                    (!validMapping || referenced) ? -6 : -10, 0, deletion.mDrawingId);
                return;
            }

            if (status != nullptr)
            {
                status->mState = AuthoringCommitState::Prepared;
                status->mObject = deletion.mDrawingId;
            }
            mPreparedRevision = batch.mRevision;
            mPendingPresentation.mRevision = batch.mRevision;
            mPendingPresentation.mObject = deletion.mDrawingId;
            mPendingPresentation.mIsDeletion = true;
            mPendingPresentation.mIsFrameMapping = mapping != nullptr;
            mPendingPresentation.mLayer = entry->mLayer;
            mPendingPresentation.mDrawingIndex = entry->mDrawingIndex;
            if (mapping != nullptr)
            {
                mPendingPresentation.mFrameIndex = mapping->mFrameIndex;
                mPendingPresentation.mMappedDrawingIndex = mappedEntry->mDrawingIndex;
                mPendingPresentation.mPreviousFrameDrawingIndex =
                    frames[mapping->mFrameIndex];
            }
            mPendingPresentation.mActive = active;
            return;
        }

        if (batch.mFrameMappings.size() == 1 && batch.mDrawingDeletions.empty() &&
            batch.mLayerPropertyEdits.empty() && batch.mAnimationKeyEdits.empty() &&
            batch.mInitialSpawnAreaEdits.empty() &&
            batch.mSpawnAreaEdits.empty() &&
            batch.mLayerCreations.empty() &&
            batch.mGeometryEdits.empty() && batch.mDrawingCreations.empty())
        {
            const FrameMapping & mapping = batch.mFrameMappings[0];
            const DrawingHandleEntry * entry = iFindDrawingHandle(
                mapping.mLayerId, mapping.mDrawingId);
            LayerPaint * paint = entry != nullptr && entry->mLayer != nullptr &&
                entry->mLayer->GetType() == Layer::Type::Paint ?
                (LayerPaint *)entry->mLayer->GetImplementation() : nullptr;
            if (paint == nullptr || mapping.mFrameIndex >= paint->GetNumFrames() ||
                entry->mDrawingIndex >= paint->GetNumDrawings())
            {
                iRejectCommit(batch.mRevision, -6, 0, mapping.mDrawingId);
                return;
            }

            if (status != nullptr)
            {
                status->mState = AuthoringCommitState::Prepared;
                status->mObject = mapping.mDrawingId;
            }
            mPreparedRevision = batch.mRevision;
            mPendingPresentation.mRevision = batch.mRevision;
            mPendingPresentation.mObject = mapping.mDrawingId;
            mPendingPresentation.mIsFrameMapping = true;
            mPendingPresentation.mLayer = entry->mLayer;
            mPendingPresentation.mDrawingIndex = entry->mDrawingIndex;
            mPendingPresentation.mFrameIndex = mapping.mFrameIndex;
            return;
        }

        const bool hasCreationFrameMapping = batch.mFrameMappings.size() == 1 &&
            batch.mDrawingCreations.size() == 1 &&
            batch.mFrameMappings[0].mLayerId == batch.mDrawingCreations[0].mLayerId &&
            batch.mFrameMappings[0].mDrawingId == batch.mDrawingCreations[0].mDrawingId;
        if (!batch.mLayerPropertyEdits.empty() || !batch.mAnimationKeyEdits.empty() ||
            !batch.mInitialSpawnAreaEdits.empty() ||
            !batch.mSpawnAreaEdits.empty() ||
            !batch.mLayerCreations.empty() ||
            !batch.mDrawingDeletions.empty() ||
            (!batch.mFrameMappings.empty() && !hasCreationFrameMapping) ||
            batch.mGeometryEdits.size() != 1 ||
            batch.mDrawingCreations.size() > 1 ||
            layerPaintRender == nullptr)
        {
            iRejectCommit(batch.mRevision, -3, 0, 0);
            return;
        }

        GeometryEdit & edit = batch.mGeometryEdits[0];
        if (status != nullptr)
            status->mObject = edit.mDrawingId;
        const DrawingCreation * creation = nullptr;
        if (!batch.mDrawingCreations.empty() &&
            batch.mDrawingCreations[0].mLayerId == edit.mLayerId &&
            batch.mDrawingCreations[0].mDrawingId == edit.mDrawingId)
            creation = &batch.mDrawingCreations[0];

        const DrawingHandleEntry * entry = iFindDrawingHandle(edit.mLayerId, edit.mDrawingId);
        Layer * layer = creation != nullptr ?
            iFindAuthoringLayerById(mSequence.GetRoot(), creation->mLayerId) :
            (entry != nullptr ? entry->mLayer : nullptr);
        if (layer == nullptr)
        {
            iRejectCommit(batch.mRevision, -1, 0, edit.mDrawingId);
            return;
        }

        LayerPaint * paint = (LayerPaint *)layer->GetImplementation();
        Drawing * active = creation == nullptr && paint != nullptr ?
            paint->GetDrawing(static_cast<int>(entry->mDrawingIndex)) : nullptr;
        DrawingStatic * activeStatic = dynamic_cast<DrawingStatic *>(active);
        const FrameMapping * creationMapping = hasCreationFrameMapping ?
            &batch.mFrameMappings[0] : nullptr;
        if (paint == nullptr || (creation == nullptr && activeStatic == nullptr) ||
            (creationMapping != nullptr && creationMapping->mFrameIndex >= paint->GetNumFrames()))
        {
            iRejectCommit(batch.mRevision,
                creationMapping != nullptr ? -6 : -3, 0, edit.mDrawingId);
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
        replacement->SetLoaded(activeStatic != nullptr ? activeStatic->GetLoaded() : true);

        if (creation != nullptr)
        {
            try
            {
                mDrawingHandles.reserve(mDrawingHandles.size() + 1);
            }
            catch (const std::bad_alloc &)
            {
                replacement->Deinit();
                iRejectCommit(batch.mRevision, -5, 0, edit.mDrawingId);
                return;
            }
        }

        uint64_t rendererToken = 0;
        if (!layerPaintRender->PrepareDrawingReplacementInCPU(replacement.get(), &rendererToken, log))
        {
            replacement->Deinit();
            iRejectCommit(batch.mRevision, -10, 0, edit.mDrawingId);
            return;
        }

        mPendingPresentation.mRevision = batch.mRevision;
        mPendingPresentation.mObject = edit.mDrawingId;
        mPendingPresentation.mIsCreation = creation != nullptr;
        mPendingPresentation.mIsFrameMapping = creationMapping != nullptr;
        mPendingPresentation.mLayer = layer;
        mPendingPresentation.mDrawingIndex = creation != nullptr ? paint->GetNumDrawings() : entry->mDrawingIndex;
        if (creationMapping != nullptr)
            mPendingPresentation.mFrameIndex = creationMapping->mFrameIndex;
        mPendingPresentation.mActive = activeStatic;
        mPendingPresentation.mReplacement = std::move(replacement);
        mPendingPresentation.mRendererToken = rendererToken;
    }

    uint64_t Document::CommitEdits(int32_t * resultOut)
    {
        if (resultOut != nullptr)
            *resultOut = 0;
        const bool creationTargetsGeometry = mOpenDrawingCreations.size() == 1 &&
            mOpenGeometryEdits.size() == 1 &&
            mOpenDrawingCreations[0].mLayerId == mOpenGeometryEdits[0].mLayerId &&
            mOpenDrawingCreations[0].mDrawingId == mOpenGeometryEdits[0].mDrawingId;
        const bool creationTargetsMapping = mOpenFrameMappings.empty() ||
            (mOpenFrameMappings.size() == 1 && creationTargetsGeometry &&
                mOpenFrameMappings[0].mLayerId == mOpenDrawingCreations[0].mLayerId &&
                mOpenFrameMappings[0].mDrawingId == mOpenDrawingCreations[0].mDrawingId);
        const bool hasGeometryEdit = mOpenGeometryEdits.size() == 1 &&
            mOpenDrawingDeletions.empty() && mOpenLayerPropertyEdits.empty() &&
            mOpenAnimationKeyEdits.empty() && mOpenInitialSpawnAreaEdits.empty() &&
            mOpenSpawnAreaEdits.empty() &&
            mOpenLayerCreations.empty() &&
            (mOpenFrameMappings.empty() || creationTargetsMapping);
        const bool hasFrameMapping = mOpenFrameMappings.size() == 1 &&
            mOpenGeometryEdits.empty() && mOpenDrawingCreations.empty() &&
            mOpenDrawingDeletions.size() <= 1 && mOpenLayerPropertyEdits.empty() &&
            mOpenAnimationKeyEdits.empty() && mOpenInitialSpawnAreaEdits.empty() &&
            mOpenSpawnAreaEdits.empty() && mOpenLayerCreations.empty();
        const bool hasDrawingDeletion = mOpenDrawingDeletions.size() == 1 &&
            mOpenFrameMappings.empty() && mOpenGeometryEdits.empty() &&
            mOpenDrawingCreations.empty() && mOpenLayerPropertyEdits.empty() &&
            mOpenAnimationKeyEdits.empty() && mOpenInitialSpawnAreaEdits.empty() &&
            mOpenSpawnAreaEdits.empty() && mOpenLayerCreations.empty();
        const bool hasLayerPropertyEdit = mOpenLayerPropertyEdits.size() == 1 &&
            mOpenDrawingDeletions.empty() && mOpenFrameMappings.empty() &&
            mOpenGeometryEdits.empty() && mOpenDrawingCreations.empty() &&
            mOpenAnimationKeyEdits.empty() && mOpenInitialSpawnAreaEdits.empty() &&
            mOpenSpawnAreaEdits.empty() && mOpenLayerCreations.empty();
        const bool hasAnimationKeyEdit = mOpenAnimationKeyEdits.size() == 1 &&
            mOpenLayerPropertyEdits.empty() && mOpenDrawingDeletions.empty() &&
            mOpenFrameMappings.empty() && mOpenGeometryEdits.empty() &&
            mOpenDrawingCreations.empty() && mOpenInitialSpawnAreaEdits.empty() &&
            mOpenSpawnAreaEdits.empty() && mOpenLayerCreations.empty();
        const bool hasInitialSpawnAreaEdit = mOpenInitialSpawnAreaEdits.size() == 1 &&
            mOpenAnimationKeyEdits.empty() && mOpenLayerPropertyEdits.empty() &&
            mOpenDrawingDeletions.empty() && mOpenFrameMappings.empty() &&
            mOpenGeometryEdits.empty() && mOpenDrawingCreations.empty() &&
            mOpenSpawnAreaEdits.empty() && mOpenLayerCreations.empty();
        const bool hasSpawnAreaEdit = mOpenSpawnAreaEdits.size() == 1 &&
            mOpenInitialSpawnAreaEdits.empty() && mOpenAnimationKeyEdits.empty() &&
            mOpenLayerPropertyEdits.empty() && mOpenDrawingDeletions.empty() &&
            mOpenFrameMappings.empty() && mOpenGeometryEdits.empty() &&
            mOpenDrawingCreations.empty() && mOpenLayerCreations.empty();
        const bool hasLayerCreation = mOpenLayerCreations.size() == 1 &&
            mOpenSpawnAreaEdits.empty() && mOpenInitialSpawnAreaEdits.empty() &&
            mOpenAnimationKeyEdits.empty() && mOpenLayerPropertyEdits.empty() &&
            mOpenDrawingDeletions.empty() && mOpenFrameMappings.empty() &&
            mOpenGeometryEdits.empty() && mOpenDrawingCreations.empty();
        if (!mEditing ||
            (!hasGeometryEdit && !hasFrameMapping && !hasDrawingDeletion &&
                !hasLayerPropertyEdit && !hasAnimationKeyEdit &&
                !hasInitialSpawnAreaEdit && !hasSpawnAreaEdit && !hasLayerCreation) ||
            (!mOpenDrawingCreations.empty() &&
                (!creationTargetsGeometry || !creationTargetsMapping)))
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
        batch.mDrawingCreations.swap(mOpenDrawingCreations);
        batch.mGeometryEdits.swap(mOpenGeometryEdits);
        batch.mFrameMappings.swap(mOpenFrameMappings);
        batch.mDrawingDeletions.swap(mOpenDrawingDeletions);
        batch.mLayerPropertyEdits.swap(mOpenLayerPropertyEdits);
        batch.mAnimationKeyEdits.swap(mOpenAnimationKeyEdits);
        batch.mInitialSpawnAreaEdits.swap(mOpenInitialSpawnAreaEdits);
        batch.mSpawnAreaEdits.swap(mOpenSpawnAreaEdits);
        batch.mLayerCreations.swap(mOpenLayerCreations);
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
        if (layerPaintRender != nullptr && !mPendingPresentation.mIsFrameMapping &&
            !mPendingPresentation.mIsDeletion && !mPendingPresentation.mIsLayerPropertyEdit &&
            !mPendingPresentation.mIsAnimationKeyEdit &&
            !mPendingPresentation.mIsInitialSpawnAreaEdit &&
            !mPendingPresentation.mIsSpawnAreaEdit &&
            !mPendingPresentation.mIsLayerCreation)
            layerPaintRender->CancelDrawingReplacement(
                renderer, mPendingPresentation.mRendererToken, log);
        if (mPendingPresentation.mReplacement)
            mPendingPresentation.mReplacement->Deinit();
        if (mPendingPresentation.mCreatedLayer)
            mPendingPresentation.mCreatedLayer->Deinit(log);
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
