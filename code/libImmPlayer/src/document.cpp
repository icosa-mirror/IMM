#include <stdlib.h>
#include <thread>
#include <chrono>
#include <cmath>
#include <unordered_map>


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
    // Instance links can reach into a subtree from outside its parent hierarchy.
    // Reject those deletions until reference retargeting is part of the contract.
    static bool iHasInstanceReferenceToSubtree(Layer * current, Layer * subtree)
    {
        if (current->GetType() == Layer::Type::Instance)
        {
            const auto * instance = static_cast<const LayerInstance *>(current->GetImplementation());
            for (Layer * target = instance != nullptr ? instance->GetInstance() : nullptr;
                target != nullptr; target = target->GetParent())
                if (target == subtree) return true;
        }
        if (current->GetType() == Layer::Type::Group)
            for (uint32_t i = 0; i < current->GetNumChildren(); ++i)
                if (iHasInstanceReferenceToSubtree(current->GetChild(i), subtree)) return true;
        return false;
    }

    static bool iSubtreeHasSpawnArea(Layer * layer)
    {
        if (layer->GetType() == Layer::Type::SpawnArea)
            return true;
        if (layer->GetType() == Layer::Type::Group)
            for (uint32_t index = 0; index < layer->GetNumChildren(); index++)
                if (iSubtreeHasSpawnArea(layer->GetChild(index)))
                    return true;
        return false;
    }

    Document::PreparedLayerFullName::~PreparedLayerFullName()
    {
        if (mInitialized)
            mFullName.End();
        if (mHasName)
            mName.End();
    }

    bool Document::PreparedLayerFullName::Init(Layer * layer, const wchar_t * fullName, const wchar_t * name)
    {
        mLayer = layer;
        mInitialized = true;
        if (!mFullName.InitCopyW(fullName)) return false;
        if (name == nullptr) return true;
        mHasName = true;
        return mName.InitCopyW(name);
    }

    void Document::PreparedLayerFullName::Publish(void)
    {
        piAssert(mLayer != nullptr && mInitialized);
        if (mHasName) mLayer->PublishPreparedName(&mName);
        mLayer->PublishPreparedFullName(&mFullName);
        mInitialized = false;
        mHasName = false;
    }

    bool Document::iPrepareLayerFullNames(Layer * layer, const std::wstring & prefix,
        std::vector<std::unique_ptr<PreparedLayerFullName>> & prepared, const wchar_t * newName)
    {
        std::wstring fullName = prefix;
        fullName.push_back(L'/');
        fullName.append(newName != nullptr ? newName : layer->GetName().GetS());
        auto entry = std::make_unique<PreparedLayerFullName>();
        if (!entry->Init(layer, fullName.c_str(), newName)) return false;
        prepared.push_back(std::move(entry));
        if (layer->GetType() == Layer::Type::Group)
            for (uint32_t index = 0; index < layer->GetNumChildren(); ++index)
                if (!iPrepareLayerFullNames(layer->GetChild(index), fullName, prepared)) return false;
        return true;
    }

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

    bool Document::Init(const wchar_t* name, uint32_t id, const LoadOptions & options)
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
        mHidden = options.initiallyHidden;
        mSuppress360Backdrops = options.suppress360Backdrops;
        mWasPaused = false;
        mDocumentToWorld = trans3d::identity();
        mMasterVolume = 1.0f;
        mCmdID = -1;
        mFileType = ImportType::IMM_disk;
        mMemoryData = nullptr;
        mMemorySize = 0;
        mMemoryView = nullptr;
        mSequenceReady = false;
        mEditing = false;
        mEvaluationRevision = 0;
        mLastCPUTime = piTick(0);
        mRequestedPlaybackCommand = 0;
        mCompletedPlaybackCommand = 0;
        mPlaybackRequestStatus = PlaybackRequestStatus::None;
        mRequestedRevision = 0;
        mPreparedRevision = 0;
        mPresentedRevision = 0;
        mNextDrawingHandle = 1;
        mNextLayerHandle = 1;
        return true;
    }

    void Document::End(void)
    {
        mLastCPUTime = piTick(0);
		mOpenDrawingCreations.clear();
        mOpenGeometryEdits.clear();
        mOpenFrameMappings.clear();
        mOpenFrameSequences.clear();
        mOpenDrawingDeletions.clear();
        mOpenLayerPropertyEdits.clear();
        mOpenAnimationKeyEdits.clear();
        mOpenInitialSpawnAreaEdits.clear();
        mOpenSpawnAreaEdits.clear();
        mOpenLayerCreations.clear();
        mOpenLayerDeletions.clear();
        mOpenLayerReparents.clear();
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
        return mState.mLoadingState == LoadingState::Loaded ? mPlayerManager.GetSpawnAreaCount() : 0;
    }

    int Document::GetSpawnArea()
    {
        return mState.mLoadingState == LoadingState::Loaded ? mPlayerManager.GetSpawnArea() : -1;
    }

    int Document::GetInitialSpawnArea()
    {
        return mState.mLoadingState == LoadingState::Loaded ? mPlayerManager.GetInitialSpawnArea() : -1;
    }

    int Document::GetInitialSpawnAreaLayerId() const
    {
        if (mState.mLoadingState != LoadingState::Loaded)
            return -1;
        const Layer * layer = mSequence.GetInitialSpawnArea();
        return layer != nullptr ? static_cast<int>(layer->GetID()) : -1;
    }

    bool Document::IsSpawnAreaAvailable(int spawnAreaId)
    {
        if (mState.mLoadingState != LoadingState::Loaded || mEvaluationRevision == 0 ||
            spawnAreaId < 0 || spawnAreaId >= GetSpawnAreaCount())
            return false;
        const Layer * layer = mPlayerManager.GetSpawnAreaLayer(spawnAreaId);
        return layer != nullptr && layer->GetWorldVisible();
    }

    int Document::GetAvailableSpawnAreaIds(int capacity, int * ids)
    {
        if (capacity < 0 || (capacity > 0 && ids == nullptr) ||
            mState.mLoadingState != LoadingState::Loaded || mEvaluationRevision == 0)
            return -1;
        int required = 0;
        for (int id = 0; id < GetSpawnAreaCount(); ++id)
        {
            if (!IsSpawnAreaAvailable(id)) continue;
            if (required < capacity) ids[required] = id;
            ++required;
        }
        return required;
    }

    void Document::GetSpawnAreaPoseState(int spawnAreaId, SpawnAreaPoseState & pose)
    {
        pose = {};
        if (spawnAreaId < 0 || spawnAreaId >= GetSpawnAreaCount()) return;
        SpawnAreaInfo info;
        GetSpawnAreaInfo(info, spawnAreaId);
        const trans3d & transform = info.mSpawnAreaToWorld;
        pose.posx = static_cast<float>(transform.mTranslation.x);
        pose.posy = static_cast<float>(transform.mTranslation.y);
        pose.posz = static_cast<float>(transform.mTranslation.z);
        pose.rotx = static_cast<float>(transform.mRotation.x);
        pose.roty = static_cast<float>(transform.mRotation.y);
        pose.rotz = static_cast<float>(transform.mRotation.z);
        pose.rotw = static_cast<float>(transform.mRotation.w);
        pose.sca = static_cast<float>(transform.mScale);
        pose.animated = info.mAnimated ? 1 : 0;
        pose.isFloorLevel = info.mIsFloorLevel ? 1 : 0;
        pose.locomotion = (info.mVolume.mAllowTranslationX ? 4 : 0) |
            (info.mVolume.mAllowTranslationY ? 2 : 0) | (info.mVolume.mAllowTranslationZ ? 1 : 0);
    }

    int Document::GetSpawnAreaSnapshot(SpawnAreaSnapshot & snapshot, int capacity, int * ids)
    {
        const int count = GetAvailableSpawnAreaIds(capacity, ids);
        if (count < 0) return -1;
        snapshot = {};
        snapshot.structSize = sizeof(SpawnAreaSnapshot);
        snapshot.structVersion = 1;
        snapshot.evaluationRevision = mEvaluationRevision;
        snapshot.requestedCommand = mRequestedPlaybackCommand;
        snapshot.completedCommand = mCompletedPlaybackCommand;
        snapshot.commandStatus = static_cast<int32_t>(mPlaybackRequestStatus);
        snapshot.chapter = GetCurrentChapter();
        const int active = GetSpawnArea();
        const int preferred = GetInitialSpawnArea();
        snapshot.activeId = active >= 0 && active < GetSpawnAreaCount() ? active : -1;
        snapshot.preferredId = preferred >= 0 && preferred < GetSpawnAreaCount() ? preferred : -1;
        snapshot.availableCount = count;
        snapshot.activeAvailable = IsSpawnAreaAvailable(snapshot.activeId) ? 1 : 0;
        snapshot.preferredAvailable = IsSpawnAreaAvailable(snapshot.preferredId) ? 1 : 0;
        snapshot.needsUpdate = GetSpawnAreaNeedsUpdate() ? 1 : 0;
        GetSpawnAreaPoseState(snapshot.activeId, snapshot.activePose);
        GetSpawnAreaPoseState(snapshot.preferredId, snapshot.preferredPose);
        return count;
    }

    bool Document::TrySetActiveSpawnAreaId(int spawnAreaId, uint64_t expectedEvaluationRevision,
        SpawnAreaPoseState & pose)
    {
        pose = {};
        if (mPlaybackRequestStatus == PlaybackRequestStatus::Pending ||
            (expectedEvaluationRevision != 0 && expectedEvaluationRevision != mEvaluationRevision) ||
            !IsSpawnAreaAvailable(spawnAreaId))
            return false;
        GetSpawnAreaPoseState(spawnAreaId, pose);
        mPlayerManager.SetSpawnArea(spawnAreaId);
        return true;
    }

    bool Document::CanRequestPlaybackEvaluation() const
    {
        return mState.mLoadingState == LoadingState::Loaded && !mHidden &&
            mPlaybackRequestStatus != PlaybackRequestStatus::Pending;
    }

    uint64_t Document::BeginPlaybackEvaluationRequest()
    {
        mPlaybackRequestStatus = PlaybackRequestStatus::Pending;
        return ++mRequestedPlaybackCommand;
    }

    void Document::SetSpawnArea(int spawnAreaId)
    {
        mPlayerManager.SetSpawnArea(spawnAreaId);
    }

    const piImage* Document::GetSpawnAreaScreenshot(int spawnAreaId)
    {
        if (mState.mLoadingState != LoadingState::Loaded)
            return nullptr;
        const Layer *layer = mPlayerManager.GetSpawnAreaLayer(spawnAreaId);
        if (layer == nullptr || layer->GetType() != Layer::Type::SpawnArea || layer->GetImplementation() == nullptr)
            return nullptr;
        const LayerSpawnArea* layerSpawnArea = (LayerSpawnArea*)layer->GetImplementation();

        return layerSpawnArea->GetScreenshot();
    }

    void Document::GetSpawnAreaInfo(SpawnAreaInfo& info, int spawnAreaId)
    {
        info = SpawnAreaInfo{};
        if (mState.mLoadingState != LoadingState::Loaded)
            return;
        const Layer *layer = mPlayerManager.GetSpawnAreaLayer(spawnAreaId);
        if (layer == nullptr || layer->GetType() != Layer::Type::SpawnArea || layer->GetImplementation() == nullptr)
            return;
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
        if (mState.mLoadingState != LoadingState::Loaded ||
            !mPlayerManager.GetIsPlaying() || mSequence.GetRoot() == nullptr)
            return;
        mPlayerManager.SetTime(now, timeSinceStart, timeSinceStop);
        // A seek resets keyed playback state. Evaluate its requested pose now,
        // before a subsequent queued Pause can suppress the ordinary update.
        // Update also preserves an existing pause, as the tracked seek path does.
        mPlayerManager.Update(now, true);
        mLastCPUTime = now;
        ++mEvaluationRevision;
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
        const Command * command, const Command * queuedCommand)
    {
        mLastCPUTime = now;
        const bool trackedCommand = mPlaybackRequestStatus == PlaybackRequestStatus::Pending &&
            command != nullptr &&
            (command->mType == Command::Type::RequestSetChapter ||
             command->mType == Command::Type::RequestSetTime) &&
            command->mEvaluationRequest == mRequestedPlaybackCommand;
        bool trackedCommandApplied = false;
        const bool trackedStillQueued = queuedCommand != nullptr &&
            (queuedCommand->mType == Command::Type::RequestSetChapter ||
             queuedCommand->mType == Command::Type::RequestSetTime) &&
            queuedCommand->mEvaluationRequest == mRequestedPlaybackCommand;
        if (mPlaybackRequestStatus == PlaybackRequestStatus::Pending &&
            !trackedCommand && !trackedStillQueued)
        {
            // A legacy command may replace the existing single command slot.
            mCompletedPlaybackCommand = mRequestedPlaybackCommand;
            mPlaybackRequestStatus = PlaybackRequestStatus::Superseded;
        }

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
                    if (IsLoadingAsync(&mSequence))
                    {
                        StopLoadingAsync(&mSequence);
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

            mSequence.GetImportState().continueLoading.store(true);
            auto loadActivity = std::make_shared<ImportActivity>(&mSequence);
            std::thread loadingThread([this, log, soundEngine, layerPaintRender, layerRenderPicture, layerRenderSound, layerRendererModel, colorSpace, renderingTechnique, loadActivity = std::move(loadActivity)]() mutable
            {
                const bool loaded = iLoadCPU(log, soundEngine, layerPaintRender, layerRenderPicture, layerRenderSound, layerRendererModel, colorSpace, renderingTechnique);
                if (!loaded) mState.mErrorState = ErrorState::FailedCPU;
                loadActivity.reset();
                // Publish completion last: unloading can reclaim this Document immediately.
                mState.mLoadingState = loaded ? LoadingState::LoadingSPU : LoadingState::UnloadingCompleted;
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
            if (mHidden)
            {
                // Match Hide()/Show(): hold the initial pose while the host places the reference.
                mWasPaused = mPlayerManager.GetIsPaused();
                if (!mWasPaused) mPlayerManager.Pause(now);
            }
            mState.mLoadingState = LoadingState::Loaded;
        }
        else if (st == LoadingState::Loaded)
        {
        }
        else if (st == LoadingState::StopingLoadingAsync)
        {
            // wait for the loading thread to finish
            if (!IsLoadingAsync(&mSequence))
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
            else if (trackedCommand)
            {
                if (mState.mLoadingState == LoadingState::Loaded && !mHidden)
                {
                    if (command->mType == Command::Type::RequestSetChapter)
                        trackedCommandApplied = mPlayerManager.SetChapter(now,
                            static_cast<size_t>(command->mIntArg));
                    else
                    {
                        mPlayerManager.SetTime(now, piTick(command->mSeekStartTicks),
                            piTick(command->mSeekStopTicks));
                        trackedCommandApplied = true;
                    }
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
            {
                const bool evaluates = !mPlayerManager.GetIsPaused() || trackedCommandApplied;
                mPlayerManager.Update(now, trackedCommandApplied);
                if (evaluates) ++mEvaluationRevision;
            }
        }


        if (trackedCommand)
        {
            mCompletedPlaybackCommand = mRequestedPlaybackCommand;
            mPlaybackRequestStatus = trackedCommandApplied && shouldRender &&
                mPlayerManager.GetIsPlaying() ? PlaybackRequestStatus::Applied :
                PlaybackRequestStatus::Rejected;
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
            if ((mPendingPresentation.mIsCreation || mPendingPresentation.mIsDeletion ||
                (mPendingPresentation.mReplacement != nullptr && !mPendingPresentation.mIsLayerCreation)) &&
                IsLoadingAsync(&mSequence))
            {
                iRejectCommit(revision, -7, 0, mPendingPresentation.mObject);
                iDiscardPendingPresentation(layerPaintRender, renderer, log);
                return mState.mPlaybackState != PlaybackState::PausedAndHidden;
            }
            if (mPendingPresentation.mHasFrameSequence && std::getenv("IMM_LIVE_EDIT_HOLD_FRAME_SEQUENCE") != nullptr)
                return mState.mPlaybackState != PlaybackState::PausedAndHidden;
            if (mPendingPresentation.mHasFrameSequence && std::getenv("IMM_LIVE_EDIT_FAIL_PRESENT") != nullptr)
            {
                iRejectCommit(revision, -10, 0, mPendingPresentation.mObject);
                iDiscardPendingPresentation(layerPaintRender, renderer, log);
                return mState.mPlaybackState != PlaybackState::PausedAndHidden;
            }
            if (mPendingPresentation.mHasPreparedSpawnAreas &&
                !mPlayerManager.PrepareSpawnAreaPublication(
                    static_cast<uint32_t>(mPendingPresentation.mPreparedSpawnAreas.size())))
            {
                iRejectCommit(revision, -5, 0, mPendingPresentation.mObject);
                iDiscardPendingPresentation(layerPaintRender, renderer, log);
                return mState.mPlaybackState != PlaybackState::PausedAndHidden;
            }
            if (mPendingPresentation.mIsLayerReparent)
            {
                Layer * layer = mPendingPresentation.mLayer;
                Layer * parent = mPendingPresentation.mLayerCreationParent;
                Layer * previousParent = mPendingPresentation.mLayerPreviousParent;
                const uint64_t layerId = mPendingPresentation.mObject;
                bool published = layer != nullptr && parent != nullptr &&
                    previousParent != nullptr;
                if (published && parent == previousParent)
                {
                    published = parent->ReorderPublishedChild(
                        layer, mPendingPresentation.mLayerChildIndex);
                }
                else if (published)
                {
                    const bool removed = previousParent->RemovePublishedChild(layer);
                    published = removed;
                    if (removed)
                    {
                        layer->SetPublishedParent(parent);
                        published = parent->InsertPreparedChild(
                            layer, mPendingPresentation.mLayerChildIndex);
                    }
                    if (!published && removed)
                    {
                        layer->SetPublishedParent(previousParent);
                        previousParent->InsertPreparedChild(
                            layer, mPendingPresentation.mLayerPreviousChildIndex);
                    }
                }
                if (!published)
                {
                    iRejectCommit(revision, -6, 0, layerId);
                    mPendingPresentation = PendingPresentation{};
                }
                else
                {
                    for (const std::unique_ptr<PreparedLayerFullName> & prepared :
                        mPendingPresentation.mPreparedLayerFullNames)
                        prepared->Publish();
                    const bool changedParent = parent != previousParent;
                    iPublishPreparedSpawnAreaCache();
                    mPendingPresentation = PendingPresentation{};
                    if (status != nullptr)
                        iSetCommitState(status, AuthoringCommitState::Presented);
                    mPresentedRevision = revision;
                    log->Printf(LT_MESSAGE,
                        L"[IMM_LIVE_EDIT] presented revision=%llu %lsLayer=%llu",
                        static_cast<unsigned long long>(revision),
                        changedParent ? L"reparented" : L"reordered",
                        static_cast<unsigned long long>(layerId));
                }
            }
            else if (mPendingPresentation.mIsLayerDeletion)
            {
                Layer * layer = mPendingPresentation.mLayer;
                Layer * parent = mPendingPresentation.mLayerCreationParent;
                const uint64_t layerId = mPendingPresentation.mObject;
                const auto & drawings = mPendingPresentation.mDeletedDrawings;
                // Publication is serialized on the render thread. Recheck the
                // complete retirement budget immediately before any ownership change.
                if (!drawings.empty() && (layerPaintRender == nullptr ||
                    !layerPaintRender->PrepareDrawingsDeletion(drawings.data(),
                        static_cast<uint32_t>(drawings.size()), log)))
                {
                    iRejectCommit(revision, -5, 0, layerId);
                    mPendingPresentation = PendingPresentation{};
                    return mState.mPlaybackState != PlaybackState::PausedAndHidden;
                }
                bool removed = layer != nullptr && parent != nullptr &&
                    !mPendingPresentation.mDeletedLayers.empty() &&
                    parent->RemovePublishedChild(layer);
                if (removed && !mSequence.RemovePublishedLayers(
                    mPendingPresentation.mDeletedLayers.data(),
                    static_cast<uint32_t>(mPendingPresentation.mDeletedLayers.size())))
                {
                    parent->InsertPreparedChild(
                        layer, mPendingPresentation.mLayerPreviousChildIndex);
                    removed = false;
                }
                if (!removed)
                {
                    iRejectCommit(revision, -6, 0, layerId);
                    mPendingPresentation = PendingPresentation{};
                }
                else
                {
                    iPublishPreparedSpawnAreaCache();
                    for (Layer * deleted : mPendingPresentation.mDeletedLayers)
                    {
                        if (deleted->GetType() == Layer::Type::Paint)
                            static_cast<LayerPaintStatic *>(deleted->GetImplementation())->ReleaseDrawingsForRetirement();
                    }
                    for (Drawing * drawing : drawings)
                        layerPaintRender->PresentDrawingDeletion(drawing, log);
                    for (size_t i = 0; i < mDrawingHandles.size();)
                    {
                        const Layer * owner = mDrawingHandles[i].mLayer;
                        bool deleted = false;
                        for (Layer * candidate : mPendingPresentation.mDeletedLayers)
                            deleted = deleted || candidate == owner;
                        if (deleted) mDrawingHandles.erase(mDrawingHandles.begin() + i);
                        else ++i;
                    }
                    const uint32_t retiredDrawingCount = static_cast<uint32_t>(drawings.size());
                    layer->Deinit(log);
                    const uint32_t deletedLayerCount = static_cast<uint32_t>(
                        mPendingPresentation.mDeletedLayers.size());
                    for (auto deleted = mPendingPresentation.mDeletedLayers.rbegin();
                        deleted != mPendingPresentation.mDeletedLayers.rend(); deleted++)
                        delete *deleted;
                    mPendingPresentation = PendingPresentation{};
                    if (status != nullptr)
                        iSetCommitState(status, AuthoringCommitState::Presented);
                    mPresentedRevision = revision;
                    log->Printf(LT_MESSAGE,
                        L"[IMM_LIVE_EDIT] presented revision=%llu deletedGroupLayer=%llu subtreeLayers=%u retiredDrawings=%u",
                        static_cast<unsigned long long>(revision),
                        static_cast<unsigned long long>(layerId), deletedLayerCount, retiredDrawingCount);
                }
            }
            else if (mPendingPresentation.mIsLayerCreation)
            {
                Layer * parent = mPendingPresentation.mLayerCreationParent;
                Layer * created = mPendingPresentation.mCreatedLayer.get();
                const uint64_t layerId = mPendingPresentation.mObject;
                const bool paintCreation = mPendingPresentation.mHasPaintCreationRendererToken;
                if (paintCreation && std::getenv("IMM_LIVE_EDIT_HOLD_PAINT_CREATION") != nullptr)
                    return mState.mPlaybackState != PlaybackState::PausedAndHidden;
                if (paintCreation)
                {
                    auto * paint = created != nullptr ?
                        static_cast<LayerPaintStatic *>(created->GetImplementation()) : nullptr;
                    if (paint == nullptr || layerPaintRender == nullptr ||
                        !layerPaintRender->PrepareDrawingReplacementInGPU(renderer, mPendingPresentation.mRendererToken, log))
                    {
                        iRejectCommit(revision, -10, 0, layerId);
                        iDiscardPendingPresentation(layerPaintRender, renderer, log);
                        return mState.mPlaybackState != PlaybackState::PausedAndHidden;
                    }
                    if (status != nullptr)
                        iSetCommitState(status, AuthoringCommitState::Prepared);
                    mPreparedRevision = revision;
                    if (!layerPaintRender->PresentDrawingCreation(paint->GetDrawing(0),
                        mPendingPresentation.mReplacement.get(), mPendingPresentation.mRendererToken, revision, log))
                    {
                        iRejectCommit(revision, -10, 0, layerId);
                        iDiscardPendingPresentation(layerPaintRender, renderer, log);
                        return mState.mPlaybackState != PlaybackState::PausedAndHidden;
                    }
                }
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
                    iDiscardPendingPresentation(layerPaintRender, renderer, log);
                }
                else
                {
                    if (paintCreation)
                    {
                        mDrawingHandles.push_back(DrawingHandleEntry{
                            mPendingPresentation.mCreatedPaintDrawingHandle, created, 0 });
                        mPendingPresentation.mReplacement->Deinit();
                    }
                    const bool spawnCreation = created->GetType() == Layer::Type::SpawnArea;
                    iPublishPreparedSpawnAreaCache();
                    mPendingPresentation.mCreatedLayer.release();
                    mPendingPresentation = PendingPresentation{};
                    if (status != nullptr)
                        iSetCommitState(status, AuthoringCommitState::Presented);
                    mPresentedRevision = revision;
                    if (spawnCreation)
                        log->Printf(LT_MESSAGE,
                            L"[IMM_LIVE_EDIT] presented revision=%llu createdSpawnLayer=%llu",
                            static_cast<unsigned long long>(revision), static_cast<unsigned long long>(layerId));
                    else if (paintCreation)
                        log->Printf(LT_MESSAGE,
                            L"[IMM_LIVE_EDIT] presented revision=%llu createdPaintLayer=%llu",
                            static_cast<unsigned long long>(revision), static_cast<unsigned long long>(layerId));
                    else
                        log->Printf(LT_MESSAGE,
                            L"[IMM_LIVE_EDIT] presented revision=%llu createdGroupLayer=%llu",
                            static_cast<unsigned long long>(revision), static_cast<unsigned long long>(layerId));
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
                        iSetCommitState(status, AuthoringCommitState::Presented);
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
                        iSetCommitState(status, AuthoringCommitState::Presented);
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
                const Layer::AnimProperty property = mPendingPresentation.mAnimationProperty;
                const bool paintPlaybackKey = property == Layer::AnimProperty::Loop || property == Layer::AnimProperty::Offset;
                if (paintPlaybackKey && std::getenv("IMM_LIVE_EDIT_HOLD_PAINT_PLAYBACK_KEYS") != nullptr)
                    return mState.mPlaybackState != PlaybackState::PausedAndHidden;
                if (paintPlaybackKey && std::getenv("IMM_LIVE_EDIT_FAIL_PRESENT") != nullptr)
                {
                    iRejectCommit(revision, -10, 0, layerId);
                    mPendingPresentation = PendingPresentation{};
                    return mState.mPlaybackState != PlaybackState::PausedAndHidden;
                }
                if (layer == nullptr)
                {
                    iRejectCommit(revision, -1, 0, layerId);
                    mPendingPresentation = PendingPresentation{};
                }
                else
                {
                    layer->ReplaceAnimKeys(
                        property, std::move(mPendingPresentation.mAnimationKeys));
                    if (paintPlaybackKey)
                    {
                        layer->ReevaluatePaintPlaybackAt(mLastCPUTime, log);
                        ++mEvaluationRevision;
                    }
                    mPendingPresentation = PendingPresentation{};
                    if (status != nullptr)
                        iSetCommitState(status, AuthoringCommitState::Presented);
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
                const bool rename = !mPendingPresentation.mPreparedLayerFullNames.empty();
                if (rename && std::getenv("IMM_LIVE_EDIT_HOLD_LAYER_RENAME") != nullptr)
                    return mState.mPlaybackState != PlaybackState::PausedAndHidden;
                if (rename && IsLoadingAsync(&mSequence))
                {
                    iRejectCommit(revision, -7, 0, layerId);
                    mPendingPresentation = PendingPresentation{};
                    return mState.mPlaybackState != PlaybackState::PausedAndHidden;
                }
                if (mPendingPresentation.mSetPaintRepeat &&
                    std::getenv("IMM_LIVE_EDIT_HOLD_PAINT_PLAYBACK_KEYS") != nullptr)
                    return mState.mPlaybackState != PlaybackState::PausedAndHidden;
                if ((mPendingPresentation.mSetPaintRepeat || rename) &&
                    std::getenv("IMM_LIVE_EDIT_FAIL_PRESENT") != nullptr)
                {
                    iRejectCommit(revision, -10, 0, layerId);
                    mPendingPresentation = PendingPresentation{};
                    return mState.mPlaybackState != PlaybackState::PausedAndHidden;
                }
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
                    if (rename)
                    {
                        for (const auto & name : mPendingPresentation.mPreparedLayerFullNames) name->Publish();
                        if (iSubtreeHasSpawnArea(layer)) mSequence.SetSpawnAreaNeedsUpdate(true);
                        ++mEvaluationRevision;
                    }
                    if (mPendingPresentation.mSetPaintRepeat)
                    {
                        auto * paint = static_cast<LayerPaintStatic *>((LayerPaint *)layer->GetImplementation());
                        paint->SetMaxRepeatCount(mPendingPresentation.mPaintRepeat);
                        layer->ReevaluatePaintPlaybackAt(mLastCPUTime, log);
                        ++mEvaluationRevision;
                    }
                    mPendingPresentation = PendingPresentation{};
                    if (status != nullptr)
                        iSetCommitState(status, AuthoringCommitState::Presented);
                    mPresentedRevision = revision;
                    log->Printf(LT_MESSAGE,
                        L"[IMM_LIVE_EDIT] presented revision=%llu layer=%llu canonicalVisible=%d canonicalOpacity=%.3f",
                        static_cast<unsigned long long>(revision),
                        static_cast<unsigned long long>(layerId),
                        layer->GetCanonicalVisible() ? 1 : 0,
                        layer->GetCanonicalOpacity());
                }
            }
            else if (mPendingPresentation.mHasFrameSequence && !mPendingPresentation.mIsCreation &&
                !mPendingPresentation.mIsDeletion)
            {
                auto * paint = mPendingPresentation.mLayer != nullptr ?
                    dynamic_cast<LayerPaintStatic *>((LayerPaint *)mPendingPresentation.mLayer->GetImplementation()) : nullptr;
                if (paint == nullptr || !mPendingPresentation.mPreparedFrameTable ||
                    !paint->PublishFrameTable(*mPendingPresentation.mPreparedFrameTable))
                    iRejectCommit(revision, -6, 0, mPendingPresentation.mObject);
                else
                {
                    if (status != nullptr)
                        iSetCommitState(status, AuthoringCommitState::Presented);
                    mPresentedRevision = revision;
                    log->Printf(LT_MESSAGE, L"[IMM_LIVE_EDIT] presented revision=%llu frameSequenceLayer=%llu frames=%u",
                        static_cast<unsigned long long>(revision), static_cast<unsigned long long>(mPendingPresentation.mObject),
                        paint->GetNumFrames());
                }
                mPendingPresentation = PendingPresentation{};
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
                const bool retirementReady = hasValidMapping && layerPaintRender != nullptr &&
                    layerPaintRender->PrepareDrawingDeletion(mPendingPresentation.mActive, log);
                if (retirementReady && mPendingPresentation.mIsFrameMapping)
                    frames[mPendingPresentation.mFrameIndex] =
                        mPendingPresentation.mMappedDrawingIndex;
                bool tableReady = !mPendingPresentation.mHasFrameSequence;
                if (retirementReady && mPendingPresentation.mHasFrameSequence)
                {
                    auto * staticPaint = dynamic_cast<LayerPaintStatic *>(paint);
                    tableReady = staticPaint != nullptr && mPendingPresentation.mPreparedFrameTable &&
                        staticPaint->PublishFrameTable(*mPendingPresentation.mPreparedFrameTable);
                }
                Drawing * removed = paint != nullptr && layerPaintRender != nullptr ?
                    (retirementReady && tableReady ? paint->ExtractDrawing(
                        mPendingPresentation.mDrawingIndex) : nullptr) : nullptr;
                if (removed == nullptr)
                {
                    if (mPendingPresentation.mHasFrameSequence && tableReady)
                        static_cast<LayerPaintStatic *>(paint)->PublishFrameTable(*mPendingPresentation.mPreparedFrameTable);
                    if (frames != nullptr && mPendingPresentation.mIsFrameMapping &&
                        mPendingPresentation.mFrameIndex < paint->GetNumFrames())
                        frames[mPendingPresentation.mFrameIndex] =
                            mPendingPresentation.mPreviousFrameDrawingIndex;
                    iRejectCommit(revision, layerPaintRender == nullptr ? -10 :
                        (hasValidMapping && !retirementReady ? -5 : -6), 0,
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
                        iSetCommitState(status, AuthoringCommitState::Presented);
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
                        iSetCommitState(status, AuthoringCommitState::Presented);
                    mPresentedRevision = revision;
                    log->Printf(LT_MESSAGE,
                        L"[IMM_LIVE_EDIT] presented revision=%llu frame=%u drawing=%llu",
                        static_cast<unsigned long long>(revision), presentedFrame,
                        static_cast<unsigned long long>(presentedDrawing));
                }
            }
            else if (layerPaintRender == nullptr ||
                (!mPendingPresentation.mGPUPrepared &&
                    !layerPaintRender->PrepareDrawingReplacementInGPU(
                        renderer, mPendingPresentation.mRendererToken, log)))
            {
                iRejectCommit(revision, -10, 0, mPendingPresentation.mObject);
                iDiscardPendingPresentation(layerPaintRender, renderer, log);
            }
            else
            {
                const bool holdGPUPrepared = std::getenv("IMM_LIVE_EDIT_HOLD_GPU_PREPARED") != nullptr;
                if (!mPendingPresentation.mGPUPrepared)
                {
                    // Upload owns graphics resources already. A diagnostic hold must
                    // retain that slot rather than upload over its handles each frame.
                    mPendingPresentation.mGPUPrepared = true;
                    if (status != nullptr)
                        iSetCommitState(status, AuthoringCommitState::Prepared);
                    mPreparedRevision = revision;
                    if (holdGPUPrepared)
                    {
                        uint64_t points = 0;
                        uint64_t indices = 0;
                        const auto * drawing = dynamic_cast<const DrawingStatic *>(mPendingPresentation.mReplacement.get());
                        const auto * geometry = drawing != nullptr ? drawing->GetGeometry() : nullptr;
                        if (geometry != nullptr)
                            for (const auto & buffer : geometry->mBuffers)
                            {
                                points += buffer.mPoints.GetLength();
                                indices += buffer.mIndices.GetLength();
                            }
                        log->Printf(LT_MESSAGE,
                            L"[IMM_LIVE_GPU_PREPARED] revision=%llu object=%llu token=%llu points=%llu indices=%llu",
                            static_cast<unsigned long long>(revision),
                            static_cast<unsigned long long>(mPendingPresentation.mObject),
                            static_cast<unsigned long long>(mPendingPresentation.mRendererToken),
                            static_cast<unsigned long long>(points),
                            static_cast<unsigned long long>(indices));
                    }
                }
                if (holdGPUPrepared)
                    return mState.mPlaybackState != PlaybackState::PausedAndHidden;

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

                bool presented = mPendingPresentation.mIsCreation ?
                    layerPaintRender->PresentDrawingCreation(created, replacement,
                        mPendingPresentation.mRendererToken, revision, log) :
                    layerPaintRender->PresentDrawingReplacement(mPendingPresentation.mActive,
                        replacement, mPendingPresentation.mRendererToken, revision, log);
                if (presented && mPendingPresentation.mHasFrameSequence)
                {
                    auto * paint = dynamic_cast<LayerPaintStatic *>((LayerPaint *)mPendingPresentation.mLayer->GetImplementation());
                    if (paint == nullptr || !mPendingPresentation.mPreparedFrameTable ||
                        !paint->PublishFrameTable(*mPendingPresentation.mPreparedFrameTable))
                    {
                        // Release the renderer slot before destroying the appended drawing.
                        layerPaintRender->CancelDrawingReplacement(renderer, mPendingPresentation.mRendererToken, log);
                        presented = false;
                    }
                }
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
                        iSetCommitState(status, AuthoringCommitState::Presented);
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
            (!mOpenFrameMappings.empty() || !mOpenFrameSequences.empty()) || !mSealedBatches.empty() ||
            !mOpenDrawingDeletions.empty() ||
            !mOpenLayerPropertyEdits.empty() ||
            !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() ||
            !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty() ||
            !mOpenLayerDeletions.empty() ||
            !mOpenLayerReparents.empty() ||
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
        mOpenFrameSequences.clear();
        mOpenDrawingDeletions.clear();
        mOpenLayerPropertyEdits.clear();
        mOpenAnimationKeyEdits.clear();
        mOpenInitialSpawnAreaEdits.clear();
        mOpenSpawnAreaEdits.clear();
        mOpenLayerCreations.clear();
        mOpenLayerDeletions.clear();
        mOpenLayerReparents.clear();
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
        // Async import holds private drawing indices and reads this owning vector.
        if (IsLoadingAsync(&mSequence))
            return -7;
        if (!mOpenDrawingCreations.empty() || !mOpenGeometryEdits.empty() ||
            (!mOpenFrameMappings.empty() || !mOpenFrameSequences.empty()) || !mOpenDrawingDeletions.empty() ||
            !mOpenLayerPropertyEdits.empty() || !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() || !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty() || !mOpenLayerDeletions.empty() ||
            !mOpenLayerReparents.empty())
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
        if (IsLoadingAsync(&mSequence))
            return -7;
        if (!mOpenDrawingCreations.empty() || !mOpenGeometryEdits.empty() ||
            mOpenFrameMappings.size() > 1 || mOpenFrameSequences.size() > 1 ||
            (!mOpenFrameMappings.empty() && !mOpenFrameSequences.empty()) || !mOpenDrawingDeletions.empty() ||
            !mOpenLayerPropertyEdits.empty() || !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() || !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty() || !mOpenLayerDeletions.empty() ||
            !mOpenLayerReparents.empty())
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
        if (!mOpenFrameSequences.empty())
        {
            const FrameSequence & sequence = mOpenFrameSequences[0];
            if (sequence.mLayerId != layerId)
                return -6;
            for (uint64_t handle : sequence.mDrawingIds)
                if (handle == drawingId)
                    return -6;
        }
        else
        {
            const uint32_t * frames = paint->GetFrameBuffer();
            for (uint32_t frameIndex = 0; frameIndex < paint->GetNumFrames(); frameIndex++)
            {
                const bool remapped = mapping != nullptr && mapping->mFrameIndex == frameIndex;
                if (frames == nullptr || (!remapped && frames[frameIndex] == entry->mDrawingIndex))
                    return -6;
            }
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
        // Background ReadDrawing may still be building this drawing's buffers.
        if (IsLoadingAsync(&mSequence))
            return -7;
        if (iFindDrawingHandle(layerId, drawingId) == nullptr &&
            iFindOpenDrawingCreation(layerId, drawingId) == nullptr)
            return -1;
        if ((!mOpenFrameMappings.empty() || !mOpenFrameSequences.empty()) || !mOpenDrawingDeletions.empty() ||
            !mOpenLayerPropertyEdits.empty() || !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() || !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty() || !mOpenLayerDeletions.empty() ||
            !mOpenLayerReparents.empty())
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
        if ((!mOpenFrameMappings.empty() || !mOpenFrameSequences.empty()) || !mOpenDrawingDeletions.empty() ||
            !mOpenLayerPropertyEdits.empty() || !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() || !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty() || !mOpenLayerDeletions.empty() ||
            !mOpenLayerReparents.empty())
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

    int32_t Document::QueueFrameSequence(uint32_t layerId, std::vector<uint64_t> drawingIds)
    {
        if (!mEditing)
            return -4;
        if (drawingIds.size() > kMaxFrameSequenceLength)
            return -2;
        const bool creationTargetsGeometry = mOpenDrawingCreations.size() == 1 &&
            mOpenGeometryEdits.size() == 1 && mOpenDrawingCreations[0].mLayerId == layerId &&
            mOpenGeometryEdits[0].mLayerId == layerId &&
            mOpenDrawingCreations[0].mDrawingId == mOpenGeometryEdits[0].mDrawingId;
        if (!mOpenFrameMappings.empty() || !mOpenFrameSequences.empty() ||
            !mOpenLayerPropertyEdits.empty() || !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() || !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty() || !mOpenLayerDeletions.empty() || !mOpenLayerReparents.empty() ||
            ((!mOpenDrawingCreations.empty() || !mOpenGeometryEdits.empty()) && !creationTargetsGeometry) ||
            mOpenDrawingDeletions.size() > 1 || (creationTargetsGeometry && !mOpenDrawingDeletions.empty()))
            return -7;
        Layer * layer = iFindAuthoringLayerById(mSequence.GetRoot(), layerId);
        if (layer == nullptr)
            return -1;
        if (layer->GetType() != Layer::Type::Paint ||
            dynamic_cast<LayerPaintStatic *>((LayerPaint *)layer->GetImplementation()) == nullptr)
            return -3;
        if (!mOpenDrawingDeletions.empty() && mOpenDrawingDeletions[0].mLayerId != layerId)
            return -6;
        try
        {
            // Tables may contain one million held frames. Resolve each handle once
            // instead of scanning every drawing for every repeated frame.
            std::unordered_map<uint64_t, uint32_t> indices;
            const auto * paint = static_cast<LayerPaintStatic *>(layer->GetImplementation());
            indices.reserve(static_cast<size_t>(paint->GetNumDrawings()) + 1);
            for (const DrawingHandleEntry & entry : mDrawingHandles)
                if (entry.mLayer == layer && entry.mDrawingIndex < paint->GetNumDrawings())
                    indices.emplace(entry.mHandle, entry.mDrawingIndex);
            if (creationTargetsGeometry)
                indices.emplace(mOpenDrawingCreations[0].mDrawingId, paint->GetNumDrawings());
            for (uint64_t handle : drawingIds)
            {
                if (handle == 0 || indices.find(handle) == indices.end())
                    return -1;
                if (!mOpenDrawingDeletions.empty() && handle == mOpenDrawingDeletions[0].mDrawingId)
                    return -6;
            }
            mOpenFrameSequences.push_back(FrameSequence{layerId, std::move(drawingIds)});
            return 0;
        }
        catch (const std::bad_alloc &)
        {
            return -5;
        }
    }

    int32_t Document::QueueLayerProperties(uint32_t layerId,
        bool setVisibility, bool visible, bool setOpacity, float opacity,
        bool setTransform, const trans3d & transform, bool setPaintRepeat, uint32_t paintRepeat,
        const wchar_t * newName)
    {
        if (!mEditing)
            return -4;
        if (!mOpenDrawingCreations.empty() || !mOpenGeometryEdits.empty() ||
            (!mOpenFrameMappings.empty() || !mOpenFrameSequences.empty()) || !mOpenDrawingDeletions.empty() ||
            !mOpenLayerPropertyEdits.empty() || !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() || !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty() || !mOpenLayerDeletions.empty() ||
            !mOpenLayerReparents.empty())
            return -7;
        Layer * layer = iFindAuthoringLayerById(mSequence.GetRoot(), layerId);
        if (layer == nullptr)
            return -1;
        if (!setVisibility && !setOpacity && !setTransform && !setPaintRepeat && newName == nullptr)
            return -2;
        if (newName != nullptr)
        {
            if (*newName == 0) return -2;
            if (layer->GetParent() == nullptr || (layer->GetType() != Layer::Type::Group &&
                layer->GetType() != Layer::Type::Paint && layer->GetType() != Layer::Type::SpawnArea)) return -3;
            if (IsLoadingAsync(&mSequence)) return -7;
        }
        if (setPaintRepeat && (layer->GetType() != Layer::Type::Paint ||
            dynamic_cast<LayerPaintStatic *>((LayerPaint *)layer->GetImplementation()) == nullptr))
            return -3;
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
            layerId, setVisibility, visible, setOpacity, opacity, setTransform, transform,
            setPaintRepeat, paintRepeat, newName != nullptr ? newName : L"" });
        return 0;
    }

    int32_t Document::QueueAnimationKey(uint32_t layerId, Layer::AnimProperty property,
        piTick time, const Layer::AnimValue & value, Layer::InterpolationType interpolation)
    {
        if (!mEditing)
            return -4;
        if (!mOpenDrawingCreations.empty() || !mOpenGeometryEdits.empty() ||
            (!mOpenFrameMappings.empty() || !mOpenFrameSequences.empty()) || !mOpenDrawingDeletions.empty() ||
            !mOpenLayerPropertyEdits.empty() || !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() || !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty() || !mOpenLayerDeletions.empty() ||
            !mOpenLayerReparents.empty())
            return -7;
        Layer * layer = iFindAuthoringLayerById(mSequence.GetRoot(), layerId);
        if (layer == nullptr)
            return -1;
        const bool paintPlaybackKey = property == Layer::AnimProperty::Loop || property == Layer::AnimProperty::Offset;
        if (paintPlaybackKey && (layer->GetType() != Layer::Type::Paint ||
            dynamic_cast<LayerPaintStatic *>((LayerPaint *)layer->GetImplementation()) == nullptr))
            return -3;
        if (!paintPlaybackKey && property != Layer::AnimProperty::Visibility &&
            property != Layer::AnimProperty::Opacity &&
            property != Layer::AnimProperty::Transform)
            return -3;
        if (time < piTick(0))
            return -2;
        if (interpolation < Layer::InterpolationType::None ||
            interpolation >= Layer::InterpolationType::MAX)
            return -2;
        if (paintPlaybackKey && interpolation != Layer::InterpolationType::None)
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
            (!mOpenFrameMappings.empty() || !mOpenFrameSequences.empty()) || !mOpenDrawingDeletions.empty() ||
            !mOpenLayerPropertyEdits.empty() || !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() || !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty() || !mOpenLayerDeletions.empty() ||
            !mOpenLayerReparents.empty())
            return -7;
        Layer * layer = iFindAuthoringLayerById(mSequence.GetRoot(), layerId);
        if (layer == nullptr)
            return -1;
        const bool paintPlaybackKey = property == Layer::AnimProperty::Loop || property == Layer::AnimProperty::Offset;
        if (paintPlaybackKey && (layer->GetType() != Layer::Type::Paint ||
            dynamic_cast<LayerPaintStatic *>((LayerPaint *)layer->GetImplementation()) == nullptr))
            return -3;
        if (!paintPlaybackKey && property != Layer::AnimProperty::Visibility &&
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
            (!mOpenFrameMappings.empty() || !mOpenFrameSequences.empty()) || !mOpenDrawingDeletions.empty() ||
            !mOpenLayerPropertyEdits.empty() || !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() || !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty() || !mOpenLayerDeletions.empty() ||
            !mOpenLayerReparents.empty())
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
            (!mOpenFrameMappings.empty() || !mOpenFrameSequences.empty()) || !mOpenDrawingDeletions.empty() ||
            !mOpenLayerPropertyEdits.empty() || !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() || !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty() || !mOpenLayerDeletions.empty() ||
            !mOpenLayerReparents.empty())
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
        return iQueueLayerCreation(parentLayerId, std::move(name), Layer::Type::Group, layerIdOut);
    }

    int32_t Document::QueuePaintLayerCreation(uint32_t parentLayerId,
        std::wstring name, uint32_t * layerIdOut)
    {
        return iQueueLayerCreation(parentLayerId, std::move(name), Layer::Type::Paint, layerIdOut);
    }

    int32_t Document::QueueSpawnLayerCreation(uint32_t parentLayerId,
        std::wstring name, uint32_t * layerIdOut)
    {
        return iQueueLayerCreation(parentLayerId, std::move(name), Layer::Type::SpawnArea, layerIdOut);
    }

    int32_t Document::iQueueLayerCreation(uint32_t parentLayerId,
        std::wstring name, Layer::Type type, uint32_t * layerIdOut)
    {
        if (!mEditing)
            return -4;
        if (layerIdOut == nullptr || name.empty())
            return -2;
        if (!mOpenDrawingCreations.empty() || !mOpenGeometryEdits.empty() ||
            (!mOpenFrameMappings.empty() || !mOpenFrameSequences.empty()) || !mOpenDrawingDeletions.empty() ||
            !mOpenLayerPropertyEdits.empty() || !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() || !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty() || !mOpenLayerDeletions.empty() ||
            !mOpenLayerReparents.empty())
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
            layerId, parentLayerId, std::move(name), type });
        mNextLayerHandle++;
        *layerIdOut = layerId;
        return 0;
    }

    int32_t Document::QueueLayerDeletion(uint32_t layerId)
    {
        if (!mEditing)
            return -4;
        if (!mOpenDrawingCreations.empty() || !mOpenGeometryEdits.empty() ||
            (!mOpenFrameMappings.empty() || !mOpenFrameSequences.empty()) || !mOpenDrawingDeletions.empty() ||
            !mOpenLayerPropertyEdits.empty() || !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() || !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty() || !mOpenLayerDeletions.empty() ||
            !mOpenLayerReparents.empty())
            return -7;
        Layer * layer = iFindAuthoringLayerById(mSequence.GetRoot(), layerId);
        if (layer == nullptr)
            return -1;
        if (layer == mSequence.GetRoot() || layer->GetParent() == nullptr)
            return -3;
        if (IsLoadingAsync(&mSequence)) return -7;
        if (iHasInstanceReferenceToSubtree(mSequence.GetRoot(), layer)) return -6;
        std::function<bool(Layer *)> isDeletableGroupSubtree;
        isDeletableGroupSubtree = [&](Layer * current)
        {
            if (current->GetType() == Layer::Type::SpawnArea && !current->GetIsTimeline())
                return current->GetImplementation() != nullptr;
            if (current->GetType() == Layer::Type::Paint && !current->GetIsTimeline())
                return dynamic_cast<LayerPaintStatic *>(
                    static_cast<LayerPaint *>(current->GetImplementation())) != nullptr;
            if (current->GetType() != Layer::Type::Group || current->GetIsTimeline())
                return false;
            for (uint32_t index = 0; index < current->GetNumChildren(); index++)
            {
                if (!isDeletableGroupSubtree(current->GetChild(index)))
                    return false;
            }
            return true;
        };
        if (!isDeletableGroupSubtree(layer))
            return -3;
        mOpenLayerDeletions.push_back(LayerDeletion{ layerId });
        return 0;
    }

    int32_t Document::QueueLayerReparent(uint32_t layerId, uint32_t parentLayerId,
        uint32_t childIndex)
    {
        if (!mEditing)
            return -4;
        if (!mOpenDrawingCreations.empty() || !mOpenGeometryEdits.empty() ||
            (!mOpenFrameMappings.empty() || !mOpenFrameSequences.empty()) || !mOpenDrawingDeletions.empty() ||
            !mOpenLayerPropertyEdits.empty() || !mOpenAnimationKeyEdits.empty() ||
            !mOpenInitialSpawnAreaEdits.empty() || !mOpenSpawnAreaEdits.empty() ||
            !mOpenLayerCreations.empty() || !mOpenLayerDeletions.empty() ||
            !mOpenLayerReparents.empty())
            return -7;
        Layer * layer = iFindAuthoringLayerById(mSequence.GetRoot(), layerId);
        Layer * parent = iFindAuthoringLayerById(mSequence.GetRoot(), parentLayerId);
        if (layer == nullptr || parent == nullptr)
            return -1;
        if (layer == mSequence.GetRoot() || parent->GetType() != Layer::Type::Group)
            return -3;
        for (Layer * ancestor = parent; ancestor != nullptr; ancestor = ancestor->GetParent())
        {
            if (ancestor == layer)
                return -3;
        }
        const bool sameParent = layer->GetParent() == parent;
        if ((sameParent && childIndex >= parent->GetNumChildren()) ||
            (!sameParent && childIndex > parent->GetNumChildren()))
            return -2;
        mOpenLayerReparents.push_back(LayerReparent{ layerId, parentLayerId, childIndex });
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

    static uint64_t iAuthoringClockNs()
    {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    void Document::iSetCommitState(AuthoringCommitStatus * status, AuthoringCommitState state)
    {
        if (status == nullptr) return;
        status->mState = state;
        const uint64_t now = iAuthoringClockNs();
        // Keep the first observation of a transition if preparation spans frames.
        switch (state)
        {
        case AuthoringCommitState::Preparing: if (status->mPreparingNs == 0) status->mPreparingNs = now; break;
        case AuthoringCommitState::Prepared: if (status->mPreparedNs == 0) status->mPreparedNs = now; break;
        case AuthoringCommitState::Presented: if (status->mPresentedNs == 0) status->mPresentedNs = now; break;
        case AuthoringCommitState::Rejected: if (status->mRejectedNs == 0) status->mRejectedNs = now; break;
        default: break;
        }
    }

    void Document::iRejectCommit(uint64_t revision, int32_t result, uint32_t failingCommand, uint64_t object)
    {
        AuthoringCommitStatus * status = iFindCommitStatus(revision);
        if (status == nullptr)
            return;
        iSetCommitState(status, AuthoringCommitState::Rejected);
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
        mOpenFrameSequences.clear();
        mOpenDrawingDeletions.clear();
        mOpenLayerPropertyEdits.clear();
        mOpenAnimationKeyEdits.clear();
        mOpenInitialSpawnAreaEdits.clear();
        mOpenSpawnAreaEdits.clear();
        mOpenLayerCreations.clear();
        mOpenLayerDeletions.clear();
        mOpenLayerReparents.clear();
        mSealedBatches.clear();
        mCommitStatuses.clear();
        mDrawingHandles.clear();
        mEditing = false;
    }

    bool Document::iPrepareSpawnAreaCache(Layer * removed, Layer * inserted,
        Layer * insertionParent, uint32_t insertionIndex, std::vector<Layer *> & prepared)
    {
        try
        {
            prepared.reserve(static_cast<size_t>(mPlayerManager.GetSpawnAreaCount()) + (inserted != nullptr ? 1 : 0));
            bool insertionVisited = inserted == nullptr;
            std::function<bool(Layer *, bool)> visit;
            visit = [&](Layer * current, bool isInserted)
            {
                if (current == removed && !isInserted)
                    return true;
                if (current->GetType() == Layer::Type::SpawnArea)
                {
                    if (current->GetImplementation() == nullptr)
                        return false;
                    prepared.push_back(current);
                }
                if (current->GetType() != Layer::Type::Group)
                    return true;
                uint32_t finalIndex = 0;
                const auto insertHere = [&]()
                {
                    if (current != insertionParent || finalIndex != insertionIndex || insertionVisited)
                        return true;
                    insertionVisited = true;
                    finalIndex++;
                    return visit(inserted, true);
                };
                for (uint32_t index = 0; index < current->GetNumChildren(); index++)
                {
                    Layer * child = current->GetChild(index);
                    if (child == removed)
                        continue;
                    if (!insertHere() || !visit(child, false))
                        return false;
                    finalIndex++;
                }
                return insertHere();
            };
            if (!visit(mSequence.GetRoot(), false) || !insertionVisited || prepared.size() > UINT32_MAX)
                return false;
            return mPlayerManager.PrepareSpawnAreaPublication(static_cast<uint32_t>(prepared.size()));
        }
        catch (const std::bad_alloc &)
        {
            return false;
        }
    }

    void Document::iPublishPreparedSpawnAreaCache(void)
    {
        if (!mPendingPresentation.mHasPreparedSpawnAreas)
            return;
        const Layer * active = mPlayerManager.GetSpawnAreaLayer(mPlayerManager.GetSpawnArea());
        const Layer * preferred = mSequence.GetInitialSpawnArea();
        bool preferredSurvives = preferred == nullptr;
        for (const Layer * layer : mPendingPresentation.mPreparedSpawnAreas)
            preferredSurvives = preferredSurvives || layer == preferred;
        if (!preferredSurvives)
            mSequence.SetInitialSpawnArea(nullptr);
        mPlayerManager.PublishPreparedSpawnAreas(mPendingPresentation.mPreparedSpawnAreas.data(),
            static_cast<uint32_t>(mPendingPresentation.mPreparedSpawnAreas.size()), active);
        mSequence.SetSpawnAreaNeedsUpdate(true);
        // Structural ordinal changes invalidate checked selection even while paused.
        ++mEvaluationRevision;
    }

    int32_t Document::iPrepareFrameSequence(const FrameSequence & sequence,
        const DrawingCreation * creation, uint64_t deletedDrawing,
        std::unique_ptr<LayerPaintStatic::PreparedFrameTable> & table, Layer ** layerOut)
    {
        if (std::getenv("IMM_LIVE_EDIT_FAIL_PREPARE_CPU") != nullptr)
            return -10;
        if (sequence.mDrawingIds.size() > kMaxFrameSequenceLength)
            return -2;
        Layer * layer = iFindAuthoringLayerById(mSequence.GetRoot(), sequence.mLayerId);
        if (layer == nullptr)
            return -1;
        auto * paint = layer->GetType() == Layer::Type::Paint ?
            dynamic_cast<LayerPaintStatic *>((LayerPaint *)layer->GetImplementation()) : nullptr;
        if (paint == nullptr)
            return -3;
        if (creation != nullptr && (creation->mLayerId != sequence.mLayerId || paint->GetNumDrawings() == UINT32_MAX))
            return -6;
        try
        {
            std::unordered_map<uint64_t, uint32_t> drawingIndices;
            drawingIndices.reserve(static_cast<size_t>(paint->GetNumDrawings()) + 1);
            for (const DrawingHandleEntry & entry : mDrawingHandles)
                if (entry.mLayer == layer && entry.mDrawingIndex < paint->GetNumDrawings())
                    drawingIndices.emplace(entry.mHandle, entry.mDrawingIndex);
            if (creation != nullptr)
                drawingIndices.emplace(creation->mDrawingId, paint->GetNumDrawings());
            std::vector<uint32_t> indices;
            indices.reserve(sequence.mDrawingIds.size());
            for (uint64_t handle : sequence.mDrawingIds)
            {
                if (handle == 0)
                    return -1;
                if (handle == deletedDrawing)
                    return -6;
                const auto found = drawingIndices.find(handle);
                if (found == drawingIndices.end())
                    return -1;
                indices.push_back(found->second);
            }
            table = LayerPaintStatic::PrepareFrameTable(std::move(indices),
                paint->GetNumDrawings() + (creation != nullptr ? 1 : 0));
            if (!table)
                return -5;
            *layerOut = layer;
            return 0;
        }
        catch (const std::bad_alloc &)
        {
            return -5;
        }
    }

    void Document::iPrepareAuthoringCommit(LayerRendererPaint * layerPaintRender, piLog * log)
    {
        if (mSealedBatches.empty() || mPendingPresentation.mRevision != 0)
            return;

        SealedBatch batch = std::move(mSealedBatches.front());
        mSealedBatches.pop_front();
        AuthoringCommitStatus * status = iFindCommitStatus(batch.mRevision);
        if (status != nullptr)
            iSetCommitState(status, AuthoringCommitState::Preparing);

        // Revalidate sealed drawing-vector/buffer mutations before preparation.
        // Table-only and independent layer creation do not touch imported indices.
        if ((!batch.mDrawingCreations.empty() || !batch.mDrawingDeletions.empty() ||
            !batch.mGeometryEdits.empty()) && IsLoadingAsync(&mSequence))
        {
            const uint64_t object = !batch.mGeometryEdits.empty() ? batch.mGeometryEdits[0].mDrawingId :
                (!batch.mDrawingCreations.empty() ? batch.mDrawingCreations[0].mDrawingId :
                    batch.mDrawingDeletions[0].mDrawingId);
            iRejectCommit(batch.mRevision, -7, 0, object);
            return;
        }

        if (!batch.mFrameSequences.empty() && batch.mGeometryEdits.empty())
        {
            const FrameSequence & sequence = batch.mFrameSequences[0];
            const DrawingDeletion * deletion = batch.mDrawingDeletions.empty() ? nullptr : &batch.mDrawingDeletions[0];
            Layer * layer = nullptr;
            std::unique_ptr<LayerPaintStatic::PreparedFrameTable> table;
            int32_t result = iPrepareFrameSequence(sequence, nullptr,
                deletion != nullptr ? deletion->mDrawingId : 0, table, &layer);
            const DrawingHandleEntry * entry = deletion != nullptr ?
                iFindDrawingHandle(deletion->mLayerId, deletion->mDrawingId) : nullptr;
            if (result == 0 && deletion != nullptr &&
                (deletion->mLayerId != sequence.mLayerId || entry == nullptr || entry->mLayer != layer))
                result = -6;
            Drawing * active = result == 0 && entry != nullptr ?
                ((LayerPaint *)layer->GetImplementation())->GetDrawing(static_cast<int>(entry->mDrawingIndex)) : nullptr;
            if (result == 0 && deletion != nullptr && (layerPaintRender == nullptr ||
                !layerPaintRender->PrepareDrawingDeletion(active, log)))
                result = -10;
            if (result != 0)
            {
                iRejectCommit(batch.mRevision, result, 0, sequence.mLayerId);
                return;
            }
            if (status != nullptr)
            {
                iSetCommitState(status, AuthoringCommitState::Prepared);
                status->mObject = deletion != nullptr ? deletion->mDrawingId : sequence.mLayerId;
            }
            mPreparedRevision = batch.mRevision;
            mPendingPresentation.mRevision = batch.mRevision;
            mPendingPresentation.mObject = deletion != nullptr ? deletion->mDrawingId : sequence.mLayerId;
            mPendingPresentation.mLayer = layer;
            mPendingPresentation.mHasFrameSequence = true;
            mPendingPresentation.mPreparedFrameTable = std::move(table);
            mPendingPresentation.mIsDeletion = deletion != nullptr;
            mPendingPresentation.mActive = active;
            if (entry != nullptr)
                mPendingPresentation.mDrawingIndex = entry->mDrawingIndex;
            return;
        }

        if (batch.mLayerReparents.size() == 1 && batch.mLayerDeletions.empty() &&
            batch.mLayerCreations.empty() && batch.mSpawnAreaEdits.empty() &&
            batch.mInitialSpawnAreaEdits.empty() && batch.mAnimationKeyEdits.empty() &&
            batch.mLayerPropertyEdits.empty() && batch.mDrawingDeletions.empty() &&
            batch.mFrameMappings.empty() && batch.mGeometryEdits.empty() &&
            batch.mDrawingCreations.empty())
        {
            const LayerReparent & reparent = batch.mLayerReparents[0];
            Layer * layer = iFindAuthoringLayerById(mSequence.GetRoot(), reparent.mLayerId);
            Layer * parent = iFindAuthoringLayerById(
                mSequence.GetRoot(), reparent.mParentLayerId);
            if (layer == nullptr || parent == nullptr)
            {
                iRejectCommit(batch.mRevision, -1, 0, reparent.mLayerId);
                return;
            }
            if (layer == mSequence.GetRoot() || parent->GetType() != Layer::Type::Group)
            {
                iRejectCommit(batch.mRevision, -3, 0, reparent.mLayerId);
                return;
            }
            for (Layer * ancestor = parent; ancestor != nullptr;
                ancestor = ancestor->GetParent())
            {
                if (ancestor == layer)
                {
                    iRejectCommit(batch.mRevision, -3, 0, reparent.mLayerId);
                    return;
                }
            }
            Layer * previousParent = layer->GetParent();
            if (previousParent == nullptr)
            {
                iRejectCommit(batch.mRevision, -3, 0, reparent.mLayerId);
                return;
            }
            uint32_t previousChildIndex = UINT32_MAX;
            for (uint32_t index = 0; index < previousParent->GetNumChildren(); index++)
            {
                if (previousParent->GetChild(index) == layer)
                {
                    previousChildIndex = index;
                    break;
                }
            }
            if (previousChildIndex == UINT32_MAX)
            {
                iRejectCommit(batch.mRevision, -6, 0, reparent.mLayerId);
                return;
            }
            const bool sameParent = previousParent == parent;
            if ((sameParent && reparent.mChildIndex >= parent->GetNumChildren()) ||
                (!sameParent && reparent.mChildIndex > parent->GetNumChildren()))
            {
                iRejectCommit(batch.mRevision, -2, 0, reparent.mLayerId);
                return;
            }
            std::vector<std::unique_ptr<PreparedLayerFullName>> preparedFullNames;
            if (!sameParent)
            {
                if (!parent->PrepareChildPublication())
                {
                    iRejectCommit(batch.mRevision, -5, 0, reparent.mLayerId);
                    return;
                }
                try
                {
                    if (!iPrepareLayerFullNames(layer, parent->GetFullName()->GetS(), preparedFullNames))
                    {
                        iRejectCommit(batch.mRevision, -5, 0, reparent.mLayerId);
                        return;
                    }
                }
                catch (const std::bad_alloc &)
                {
                    iRejectCommit(batch.mRevision, -5, 0, reparent.mLayerId);
                    return;
                }
            }
            std::vector<Layer *> preparedSpawnAreas;
            const bool spawnCacheChanged = iSubtreeHasSpawnArea(layer);
            if (spawnCacheChanged && !iPrepareSpawnAreaCache(layer, layer, parent,
                reparent.mChildIndex, preparedSpawnAreas))
            {
                iRejectCommit(batch.mRevision, -5, 0, reparent.mLayerId);
                return;
            }
            mPendingPresentation.mHasPreparedSpawnAreas = spawnCacheChanged;
            mPendingPresentation.mPreparedSpawnAreas = std::move(preparedSpawnAreas);
            if (status != nullptr)
            {
                iSetCommitState(status, AuthoringCommitState::Prepared);
                status->mObject = reparent.mLayerId;
            }
            mPreparedRevision = batch.mRevision;
            mPendingPresentation.mRevision = batch.mRevision;
            mPendingPresentation.mObject = reparent.mLayerId;
            mPendingPresentation.mIsLayerReparent = true;
            mPendingPresentation.mLayer = layer;
            mPendingPresentation.mLayerCreationParent = parent;
            mPendingPresentation.mLayerPreviousParent = previousParent;
            mPendingPresentation.mLayerChildIndex = reparent.mChildIndex;
            mPendingPresentation.mLayerPreviousChildIndex = previousChildIndex;
            mPendingPresentation.mPreparedLayerFullNames = std::move(preparedFullNames);
            return;
        }

        if (batch.mLayerDeletions.size() == 1 && batch.mLayerCreations.empty() &&
            batch.mLayerReparents.empty() &&
            batch.mSpawnAreaEdits.empty() && batch.mInitialSpawnAreaEdits.empty() &&
            batch.mAnimationKeyEdits.empty() && batch.mLayerPropertyEdits.empty() &&
            batch.mDrawingDeletions.empty() && batch.mFrameMappings.empty() &&
            batch.mGeometryEdits.empty() && batch.mDrawingCreations.empty())
        {
            const LayerDeletion & deletion = batch.mLayerDeletions[0];
            Layer * layer = iFindAuthoringLayerById(mSequence.GetRoot(), deletion.mLayerId);
            Layer * parent = layer != nullptr ? layer->GetParent() : nullptr;
            if (layer == nullptr)
            {
                iRejectCommit(batch.mRevision, -1, 0, deletion.mLayerId);
                return;
            }
            if (layer == mSequence.GetRoot() || parent == nullptr)
            {
                iRejectCommit(batch.mRevision, -3, 0, deletion.mLayerId);
                return;
            }
            uint32_t previousChildIndex = UINT32_MAX;
            for (uint32_t index = 0; index < parent->GetNumChildren(); index++)
            {
                if (parent->GetChild(index) == layer)
                {
                    previousChildIndex = index;
                    break;
                }
            }
            std::vector<Layer *> deletedLayers;
            std::vector<Drawing *> deletedDrawings;
            try
            {
                std::function<bool(Layer *)> collectSubtree;
                collectSubtree = [&](Layer * current)
                {
                    if (current->GetIsTimeline())
                        return false;
                    if (current->GetType() == Layer::Type::SpawnArea)
                    {
                        if (current->GetImplementation() == nullptr)
                            return false;
                        deletedLayers.push_back(current);
                        return true;
                    }
                    if (current->GetType() == Layer::Type::Paint)
                    {
                        auto * paint = dynamic_cast<LayerPaintStatic *>(
                            static_cast<LayerPaint *>(current->GetImplementation()));
                        if (paint == nullptr) return false;
                        deletedLayers.push_back(current);
                        for (uint32_t i = 0; i < paint->GetNumDrawings(); ++i)
                        {
                            Drawing * drawing = paint->GetDrawing(i);
                            if (dynamic_cast<DrawingStatic *>(drawing) == nullptr) return false;
                            deletedDrawings.push_back(drawing);
                        }
                        return true;
                    }
                    if (current->GetType() != Layer::Type::Group) return false;
                    deletedLayers.push_back(current);
                    for (uint32_t index = 0; index < current->GetNumChildren(); index++)
                    {
                        if (!collectSubtree(current->GetChild(index)))
                            return false;
                    }
                    return true;
                };
                if (IsLoadingAsync(&mSequence))
                {
                    iRejectCommit(batch.mRevision, -7, 0, deletion.mLayerId);
                    return;
                }
                if (iHasInstanceReferenceToSubtree(mSequence.GetRoot(), layer))
                {
                    iRejectCommit(batch.mRevision, -6, 0, deletion.mLayerId);
                    return;
                }
                if (previousChildIndex == UINT32_MAX || !collectSubtree(layer) ||
                    deletedDrawings.size() > UINT32_MAX)
                {
                    iRejectCommit(batch.mRevision, -3, 0, deletion.mLayerId);
                    return;
                }
            }
            catch (const std::bad_alloc &)
            {
                iRejectCommit(batch.mRevision, -5, 0, deletion.mLayerId);
                return;
            }
            std::vector<Layer *> preparedSpawnAreas;
            const bool spawnCacheChanged = iSubtreeHasSpawnArea(layer);
            if (spawnCacheChanged && !iPrepareSpawnAreaCache(layer, nullptr, nullptr, 0, preparedSpawnAreas))
            {
                iRejectCommit(batch.mRevision, -5, 0, deletion.mLayerId);
                return;
            }
            mPendingPresentation.mHasPreparedSpawnAreas = spawnCacheChanged;
            mPendingPresentation.mPreparedSpawnAreas = std::move(preparedSpawnAreas);
            if (status != nullptr)
            {
                iSetCommitState(status, AuthoringCommitState::Prepared);
                status->mObject = deletion.mLayerId;
            }
            mPreparedRevision = batch.mRevision;
            mPendingPresentation.mRevision = batch.mRevision;
            mPendingPresentation.mObject = deletion.mLayerId;
            mPendingPresentation.mIsLayerDeletion = true;
            mPendingPresentation.mLayer = layer;
            mPendingPresentation.mLayerCreationParent = parent;
            mPendingPresentation.mLayerPreviousChildIndex = previousChildIndex;
            mPendingPresentation.mDeletedLayers = std::move(deletedLayers);
            mPendingPresentation.mDeletedDrawings = std::move(deletedDrawings);
            return;
        }

        if (batch.mLayerCreations.size() == 1 && batch.mSpawnAreaEdits.empty() &&
            batch.mLayerDeletions.empty() &&
            batch.mLayerReparents.empty() &&
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
            if (!layer || !layer->Init(creation.mType, creation.mName.c_str(), true,
                trans3d::identity(), trans3d::identity(), 1.0f, false, piTick(0),
                creation.mType == Layer::Type::Paint ? 1 : 0,
                UINT32_MAX, log, false))
            {
                iRejectCommit(batch.mRevision, -5, 0, creation.mLayerId);
                return;
            }
            std::vector<Layer *> preparedSpawnAreas;
            const bool spawnCreation = creation.mType == Layer::Type::SpawnArea;
            if (spawnCreation)
            {
                std::unique_ptr<LayerSpawnArea> spawn(new (std::nothrow) LayerSpawnArea());
                LayerSpawnArea::Volume volume = {};
                volume.mType = LayerSpawnArea::Volume::Type::Sphere;
                volume.mShape.mSphere = vec4(0.0f, 0.0f, 0.0f, 0.01f);
                if (!spawn || !spawn->Init(volume, LayerSpawnArea::TrackingLevel::Floor))
                {
                    layer->Deinit(log);
                    iRejectCommit(batch.mRevision, -5, 0, creation.mLayerId);
                    return;
                }
                layer->SetImplementation(spawn.release());
                layer->SetLoaded(true);
                if (!iPrepareSpawnAreaCache(nullptr, layer.get(), parent, parent->GetNumChildren(), preparedSpawnAreas))
                {
                    layer->Deinit(log);
                    iRejectCommit(batch.mRevision, -5, 0, creation.mLayerId);
                    return;
                }
            }
            std::unique_ptr<DrawingStatic> prepared;
            bool preparedInitialized = false;
            uint64_t rendererToken = 0;
            uint64_t drawingHandle = 0;
            if (creation.mType == Layer::Type::Paint)
            {
                // Offline import synthesizes this visibility span for an empty
                // visibility track. Live creation bypasses that importer fallback.
                Layer::AnimValue visible;
                visible.init();
                visible.mBool = true;
                if (!layer->AddKey(piTick(0), Layer::AnimProperty::Visibility, visible, Layer::InterpolationType::None))
                {
                    layer->Deinit(log);
                    iRejectCommit(batch.mRevision, -5, 0, creation.mLayerId);
                    return;
                }
                bool initialized = false;
                try
                {
                    std::unique_ptr<LayerPaintStatic> paint(new (std::nothrow) LayerPaintStatic());
                    const uint32_t frameRate = mSequence.GetFrameRate() != 0 ? mSequence.GetFrameRate() : 30;
                    if (paint && paint->Init(0, 1, 1, frameRate, 1))
                    {
                        layer->SetImplementation(paint.release());
                        layer->SetLoaded(true);
                        auto * implementation = static_cast<LayerPaintStatic *>(layer->GetImplementation());
                        implementation->GetFrameBuffer()[0] = 0;
                        auto * drawing = static_cast<DrawingStatic *>(implementation->AddDrawing());
                        prepared.reset(new (std::nothrow) DrawingStatic());
                        initialized = drawing != nullptr &&
                            drawing->ReplaceGeometry(nullptr, 0, Drawing::ColorSpace::Linear, false, 0.01f) &&
                            prepared && (preparedInitialized = prepared->Init(0)) &&
                            prepared->ReplaceGeometry(nullptr, 0, Drawing::ColorSpace::Linear, false, 0.01f);
                        if (initialized)
                        {
                            drawing->SetLoaded(true);
                            prepared->SetLoaded(true);
                            mDrawingHandles.reserve(mDrawingHandles.size() + 1);
                        }
                    }
                }
                catch (const std::bad_alloc &)
                {
                    initialized = false;
                }
                if (!initialized)
                {
                    if (preparedInitialized)
                        prepared->Deinit();
                    layer->Deinit(log);
                    iRejectCommit(batch.mRevision, -5, 0, creation.mLayerId);
                    return;
                }
                if (layerPaintRender == nullptr ||
                    !layerPaintRender->PrepareDrawingReplacementInCPU(prepared.get(), &rendererToken, log))
                {
                    prepared->Deinit();
                    layer->Deinit(log);
                    iRejectCommit(batch.mRevision, -10, 0, creation.mLayerId);
                    return;
                }
                drawingHandle = mNextDrawingHandle++;
            }
            if (status != nullptr)
            {
                if (creation.mType != Layer::Type::Paint)
                    iSetCommitState(status, AuthoringCommitState::Prepared);
                status->mObject = creation.mLayerId;
            }
            if (creation.mType != Layer::Type::Paint)
                mPreparedRevision = batch.mRevision;
            mPendingPresentation.mHasPreparedSpawnAreas = spawnCreation;
            mPendingPresentation.mPreparedSpawnAreas = std::move(preparedSpawnAreas);
            mPendingPresentation.mHasPaintCreationRendererToken = creation.mType == Layer::Type::Paint;
            mPendingPresentation.mRendererToken = rendererToken;
            mPendingPresentation.mReplacement = std::move(prepared);
            mPendingPresentation.mCreatedPaintDrawingHandle = drawingHandle;
            mPendingPresentation.mRevision = batch.mRevision;
            mPendingPresentation.mObject = creation.mLayerId;
            mPendingPresentation.mIsLayerCreation = true;
            mPendingPresentation.mLayerCreationParent = parent;
            mPendingPresentation.mCreatedLayer = std::move(layer);
            return;
        }

        if (batch.mSpawnAreaEdits.size() == 1 &&
            batch.mLayerCreations.empty() &&
            batch.mLayerDeletions.empty() &&
            batch.mLayerReparents.empty() &&
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
                iSetCommitState(status, AuthoringCommitState::Prepared);
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
            batch.mLayerDeletions.empty() &&
            batch.mLayerReparents.empty() &&
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
                iSetCommitState(status, AuthoringCommitState::Prepared);
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
            batch.mLayerDeletions.empty() &&
            batch.mLayerReparents.empty() &&
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
            const bool paintPlaybackKey = edit.mProperty == Layer::AnimProperty::Loop || edit.mProperty == Layer::AnimProperty::Offset;
            if (paintPlaybackKey && (layer->GetType() != Layer::Type::Paint ||
                dynamic_cast<LayerPaintStatic *>((LayerPaint *)layer->GetImplementation()) == nullptr))
            {
                iRejectCommit(batch.mRevision, -3, 0, edit.mLayerId);
                return;
            }
            if (paintPlaybackKey && edit.mInterpolation != Layer::InterpolationType::None)
            {
                iRejectCommit(batch.mRevision, -2, 0, edit.mLayerId);
                return;
            }
            if (paintPlaybackKey && std::getenv("IMM_LIVE_EDIT_FAIL_PREPARE_CPU") != nullptr)
            {
                iRejectCommit(batch.mRevision, -10, 0, edit.mLayerId);
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
                iSetCommitState(status, AuthoringCommitState::Prepared);
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
            batch.mLayerDeletions.empty() &&
            batch.mLayerReparents.empty() &&
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
            std::vector<std::unique_ptr<PreparedLayerFullName>> preparedNames;
            if (!edit.mName.empty())
            {
                if (layer->GetParent() == nullptr || (layer->GetType() != Layer::Type::Group &&
                    layer->GetType() != Layer::Type::Paint && layer->GetType() != Layer::Type::SpawnArea))
                { iRejectCommit(batch.mRevision, -3, 0, edit.mLayerId); return; }
                if (IsLoadingAsync(&mSequence))
                { iRejectCommit(batch.mRevision, -7, 0, edit.mLayerId); return; }
                try
                {
                    if (!iPrepareLayerFullNames(layer, layer->GetParent()->GetFullName()->GetS(), preparedNames, edit.mName.c_str()))
                    { iRejectCommit(batch.mRevision, -5, 0, edit.mLayerId); return; }
                }
                catch (const std::bad_alloc &)
                { iRejectCommit(batch.mRevision, -5, 0, edit.mLayerId); return; }
            }
            if (edit.mSetPaintRepeat && (layer->GetType() != Layer::Type::Paint ||
                dynamic_cast<LayerPaintStatic *>((LayerPaint *)layer->GetImplementation()) == nullptr))
            {
                iRejectCommit(batch.mRevision, -3, 0, edit.mLayerId);
                return;
            }
            if ((edit.mSetPaintRepeat || !edit.mName.empty()) && std::getenv("IMM_LIVE_EDIT_FAIL_PREPARE_CPU") != nullptr)
            {
                iRejectCommit(batch.mRevision, -10, 0, edit.mLayerId);
                return;
            }
            if (status != nullptr)
            {
                iSetCommitState(status, AuthoringCommitState::Prepared);
                status->mObject = edit.mLayerId;
            }
            mPreparedRevision = batch.mRevision;
            mPendingPresentation.mRevision = batch.mRevision;
            mPendingPresentation.mObject = edit.mLayerId;
            mPendingPresentation.mIsLayerPropertyEdit = true;
            mPendingPresentation.mPreparedLayerFullNames = std::move(preparedNames);
            mPendingPresentation.mSetLayerVisibility = edit.mSetVisibility;
            mPendingPresentation.mLayerVisible = edit.mVisible;
            mPendingPresentation.mSetLayerOpacity = edit.mSetOpacity;
            mPendingPresentation.mLayerOpacity = edit.mOpacity;
            mPendingPresentation.mSetLayerTransform = edit.mSetTransform;
            mPendingPresentation.mLayerTransform = edit.mTransform;
            mPendingPresentation.mSetPaintRepeat = edit.mSetPaintRepeat;
            mPendingPresentation.mPaintRepeat = edit.mPaintRepeat;
            mPendingPresentation.mLayer = layer;
            return;
        }

        if (batch.mDrawingDeletions.size() == 1 && batch.mFrameMappings.size() <= 1 &&
            batch.mLayerPropertyEdits.empty() && batch.mAnimationKeyEdits.empty() &&
            batch.mInitialSpawnAreaEdits.empty() &&
            batch.mSpawnAreaEdits.empty() &&
            batch.mLayerCreations.empty() &&
            batch.mLayerDeletions.empty() &&
            batch.mLayerReparents.empty() &&
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
            bool referenced = paint != nullptr && paint->GetNumFrames() != 0 && frames == nullptr;
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
                iSetCommitState(status, AuthoringCommitState::Prepared);
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
            batch.mLayerDeletions.empty() &&
            batch.mLayerReparents.empty() &&
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
                iSetCommitState(status, AuthoringCommitState::Prepared);
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

        const bool hasCreationFrameSequence = batch.mFrameSequences.size() == 1 &&
            batch.mDrawingCreations.size() == 1 && batch.mFrameMappings.empty() &&
            batch.mFrameSequences[0].mLayerId == batch.mDrawingCreations[0].mLayerId;
        const bool hasCreationFrameMapping = batch.mFrameMappings.size() == 1 &&
            batch.mDrawingCreations.size() == 1 &&
            batch.mFrameMappings[0].mLayerId == batch.mDrawingCreations[0].mLayerId &&
            batch.mFrameMappings[0].mDrawingId == batch.mDrawingCreations[0].mDrawingId;
        if (!batch.mLayerPropertyEdits.empty() || !batch.mAnimationKeyEdits.empty() ||
            !batch.mInitialSpawnAreaEdits.empty() ||
            !batch.mSpawnAreaEdits.empty() ||
            !batch.mLayerCreations.empty() ||
            !batch.mLayerDeletions.empty() ||
            !batch.mLayerReparents.empty() ||
            !batch.mDrawingDeletions.empty() ||
            (!batch.mFrameMappings.empty() && !hasCreationFrameMapping) ||
            (!batch.mFrameSequences.empty() && !hasCreationFrameSequence) ||
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

        std::unique_ptr<LayerPaintStatic::PreparedFrameTable> creationFrameTable;
        if (hasCreationFrameSequence)
        {
            Layer * sequenceLayer = nullptr;
            const int32_t result = iPrepareFrameSequence(batch.mFrameSequences[0], creation, 0,
                creationFrameTable, &sequenceLayer);
            if (result != 0 || sequenceLayer != layer)
            {
                iRejectCommit(batch.mRevision, result != 0 ? result : -6, 0, edit.mDrawingId);
                return;
            }
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
        mPendingPresentation.mHasFrameSequence = hasCreationFrameSequence;
        mPendingPresentation.mPreparedFrameTable = std::move(creationFrameTable);
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
        const uint64_t commitStartedNs = iAuthoringClockNs();
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
            mOpenLayerDeletions.empty() &&
            mOpenLayerReparents.empty() &&
            (mOpenFrameMappings.empty() || creationTargetsMapping);
        const bool hasFrameMapping = mOpenFrameMappings.size() == 1 &&
            mOpenGeometryEdits.empty() && mOpenDrawingCreations.empty() &&
            mOpenDrawingDeletions.size() <= 1 && mOpenLayerPropertyEdits.empty() &&
            mOpenAnimationKeyEdits.empty() && mOpenInitialSpawnAreaEdits.empty() &&
            mOpenSpawnAreaEdits.empty() && mOpenLayerCreations.empty() &&
            mOpenLayerDeletions.empty() && mOpenLayerReparents.empty();
        const bool hasDrawingDeletion = mOpenDrawingDeletions.size() == 1 &&
            mOpenFrameMappings.empty() && mOpenGeometryEdits.empty() &&
            mOpenDrawingCreations.empty() && mOpenLayerPropertyEdits.empty() &&
            mOpenAnimationKeyEdits.empty() && mOpenInitialSpawnAreaEdits.empty() &&
            mOpenSpawnAreaEdits.empty() && mOpenLayerCreations.empty() &&
            mOpenLayerDeletions.empty() && mOpenLayerReparents.empty();
        const bool hasLayerPropertyEdit = mOpenLayerPropertyEdits.size() == 1 &&
            mOpenDrawingDeletions.empty() && mOpenFrameMappings.empty() &&
            mOpenGeometryEdits.empty() && mOpenDrawingCreations.empty() &&
            mOpenAnimationKeyEdits.empty() && mOpenInitialSpawnAreaEdits.empty() &&
            mOpenSpawnAreaEdits.empty() && mOpenLayerCreations.empty() &&
            mOpenLayerDeletions.empty() && mOpenLayerReparents.empty();
        const bool hasAnimationKeyEdit = mOpenAnimationKeyEdits.size() == 1 &&
            mOpenLayerPropertyEdits.empty() && mOpenDrawingDeletions.empty() &&
            mOpenFrameMappings.empty() && mOpenGeometryEdits.empty() &&
            mOpenDrawingCreations.empty() && mOpenInitialSpawnAreaEdits.empty() &&
            mOpenSpawnAreaEdits.empty() && mOpenLayerCreations.empty() &&
            mOpenLayerDeletions.empty() && mOpenLayerReparents.empty();
        const bool hasInitialSpawnAreaEdit = mOpenInitialSpawnAreaEdits.size() == 1 &&
            mOpenAnimationKeyEdits.empty() && mOpenLayerPropertyEdits.empty() &&
            mOpenDrawingDeletions.empty() && mOpenFrameMappings.empty() &&
            mOpenGeometryEdits.empty() && mOpenDrawingCreations.empty() &&
            mOpenSpawnAreaEdits.empty() && mOpenLayerCreations.empty() &&
            mOpenLayerDeletions.empty() && mOpenLayerReparents.empty();
        const bool hasSpawnAreaEdit = mOpenSpawnAreaEdits.size() == 1 &&
            mOpenInitialSpawnAreaEdits.empty() && mOpenAnimationKeyEdits.empty() &&
            mOpenLayerPropertyEdits.empty() && mOpenDrawingDeletions.empty() &&
            mOpenFrameMappings.empty() && mOpenGeometryEdits.empty() &&
            mOpenDrawingCreations.empty() && mOpenLayerCreations.empty() &&
            mOpenLayerDeletions.empty() && mOpenLayerReparents.empty();
        const bool hasLayerCreation = mOpenLayerCreations.size() == 1 &&
            mOpenSpawnAreaEdits.empty() && mOpenInitialSpawnAreaEdits.empty() &&
            mOpenAnimationKeyEdits.empty() && mOpenLayerPropertyEdits.empty() &&
            mOpenDrawingDeletions.empty() && mOpenFrameMappings.empty() &&
            mOpenGeometryEdits.empty() && mOpenDrawingCreations.empty() &&
            mOpenLayerDeletions.empty() && mOpenLayerReparents.empty();
        const bool hasLayerDeletion = mOpenLayerDeletions.size() == 1 &&
            mOpenLayerCreations.empty() && mOpenSpawnAreaEdits.empty() &&
            mOpenInitialSpawnAreaEdits.empty() && mOpenAnimationKeyEdits.empty() &&
            mOpenLayerPropertyEdits.empty() && mOpenDrawingDeletions.empty() &&
            mOpenFrameMappings.empty() && mOpenGeometryEdits.empty() &&
            mOpenDrawingCreations.empty() && mOpenLayerReparents.empty();
        const bool hasLayerReparent = mOpenLayerReparents.size() == 1 &&
            mOpenLayerCreations.empty() && mOpenLayerDeletions.empty() &&
            mOpenSpawnAreaEdits.empty() && mOpenInitialSpawnAreaEdits.empty() &&
            mOpenAnimationKeyEdits.empty() && mOpenLayerPropertyEdits.empty() &&
            mOpenDrawingDeletions.empty() && mOpenFrameMappings.empty() &&
            mOpenGeometryEdits.empty() && mOpenDrawingCreations.empty();
        const bool frameSequenceShape = mOpenFrameSequences.size() == 1 && mOpenFrameMappings.empty() &&
            mOpenLayerPropertyEdits.empty() && mOpenAnimationKeyEdits.empty() &&
            mOpenInitialSpawnAreaEdits.empty() && mOpenSpawnAreaEdits.empty() &&
            mOpenLayerCreations.empty() && mOpenLayerDeletions.empty() && mOpenLayerReparents.empty() &&
            ((mOpenDrawingCreations.empty() && mOpenGeometryEdits.empty() && mOpenDrawingDeletions.size() <= 1) ||
                (creationTargetsGeometry && mOpenDrawingDeletions.empty() &&
                    mOpenFrameSequences[0].mLayerId == mOpenDrawingCreations[0].mLayerId));
        const bool legacyShape = hasGeometryEdit || hasFrameMapping || hasDrawingDeletion ||
            hasLayerPropertyEdit || hasAnimationKeyEdit || hasInitialSpawnAreaEdit || hasSpawnAreaEdit ||
            hasLayerCreation || hasLayerDeletion || hasLayerReparent;
        const bool supportedShape = mOpenFrameSequences.empty() ? legacyShape : frameSequenceShape;
        if (!mEditing || !supportedShape ||
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
        batch.mFrameSequences.swap(mOpenFrameSequences);
        batch.mDrawingDeletions.swap(mOpenDrawingDeletions);
        batch.mLayerPropertyEdits.swap(mOpenLayerPropertyEdits);
        batch.mAnimationKeyEdits.swap(mOpenAnimationKeyEdits);
        batch.mInitialSpawnAreaEdits.swap(mOpenInitialSpawnAreaEdits);
        batch.mSpawnAreaEdits.swap(mOpenSpawnAreaEdits);
        batch.mLayerCreations.swap(mOpenLayerCreations);
        batch.mLayerDeletions.swap(mOpenLayerDeletions);
        batch.mLayerReparents.swap(mOpenLayerReparents);
        mSealedBatches.push_back(std::move(batch));
        mCommitStatuses.push_back(AuthoringCommitStatus{
            mRequestedRevision, AuthoringCommitState::Queued, 0, 0, 0, commitStartedNs });
        if (mCommitStatuses.size() > 64)
            mCommitStatuses.erase(mCommitStatuses.begin());
        return mRequestedRevision;
    }

    void Document::iDiscardPendingPresentation(LayerRendererPaint * layerPaintRender,
        piRenderer * renderer, piLog * log)
    {
        if (mPendingPresentation.mRevision == 0)
            return;
        if (mPendingPresentation.mHasPaintCreationRendererToken && layerPaintRender != nullptr)
            layerPaintRender->CancelDrawingReplacement(renderer, mPendingPresentation.mRendererToken, log);
        if (layerPaintRender != nullptr && (!mPendingPresentation.mHasFrameSequence || mPendingPresentation.mIsCreation) &&
            !mPendingPresentation.mIsFrameMapping &&
            !mPendingPresentation.mIsDeletion && !mPendingPresentation.mIsLayerPropertyEdit &&
            !mPendingPresentation.mIsAnimationKeyEdit &&
            !mPendingPresentation.mIsInitialSpawnAreaEdit &&
            !mPendingPresentation.mIsSpawnAreaEdit &&
            !mPendingPresentation.mIsLayerCreation &&
            !mPendingPresentation.mIsLayerDeletion &&
            !mPendingPresentation.mIsLayerReparent)
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
        if (IsLoadingAsync(&mSequence))
        {
            StopLoadingAsync(&mSequence);
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
            log->Printf(LT_ERROR, L"Document synchronous unload did not complete for id=%d state=%d", mID, static_cast<int>(mState.mLoadingState.load()));
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
                if (IsStoppedLoading(&mSequence)) return false;

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
            log->Printf(LT_ERROR, L"Error unloading in GPU [%d]...", mID);
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
        StopLoadingAsync(&mSequence);
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
