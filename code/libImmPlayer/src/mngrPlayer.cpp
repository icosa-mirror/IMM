#include <cstdlib>
#include "libImmImporter/src/document/layer.h"
#include "libImmImporter/src/document/sequence.h"
#include "libImmImporter/src/document/layerSound.h"
#include "libImmImporter/src/document/layerSpawnArea.h"

#include "mngrPlayer.h"
using namespace ImmCore;
using namespace ImmImporter;

namespace ImmPlayer
{
    MngrPlayer::MngrPlayer()
    {
    }

    MngrPlayer::~MngrPlayer()
    {
    }

    bool MngrPlayer::Init(piLog * log)
    {
        mIsPlaying = false;
        mIsWaitingForInput = false;
        mIsPaused = false;
        mDocument = nullptr;
        mLog = log;
        mIsAtEnd = false;
        mCurrentSpawnAreaId = -1;
        mNumPlayMarkers = 0;
        mNumStopMarkers = 0;
        mCurrentChapter = 0;
        mNumChapters = 0;
        mHasLoopKey = false;
        mLoopKeyTime = piTick::FromSeconds(60.0*60.0*1000.0);
        if (!mSpawnAreas.Init(6, true))
            return false;

        return true;
    }


    void MngrPlayer::Deinit()
    {
        mSpawnAreas.End();
    }


    void MngrPlayer::Enter(Sequence * document, const piTick now)
    {
        mDocument = document;
        mCurrentSpawnAreaId = -1;
#ifdef _DEBUG
        mLog->Printf(LT_DEBUG, L"Enter!");
#endif
        mIsPlaying = true;
        mIsAtEnd = false;

        // Parse and set initial spawn area
        auto saveSpawnArea = [this](Layer* layer, int level, int child, bool instance) -> bool
        {
            if (layer->GetType() == Layer::Type::SpawnArea)
            {
                LayerSpawnArea * lv = (LayerSpawnArea*)layer->GetImplementation();
                mSpawnAreas.Append(layer, true);
            }
            return true;
        };
        mSpawnAreas.Reset();
        mDocument->Recurse(saveSpawnArea, false, false, false, false);

        const Layer *layer = mDocument->GetInitialSpawnArea();

        if( layer!=nullptr )
        {
            int initialSpawnAreaId = FindSpawnAreaIdByLayer(layer);
            if (initialSpawnAreaId == -1)
            {
                mLog->Printf(LT_ERROR, L"Could not find default spawn area, setting to 0.");
                initialSpawnAreaId = 0;
            }

            SetSpawnArea(initialSpawnAreaId);
        }

        // Count root plays and stops
        mCurrentChapter = 0;
        iRefreshRootActionCounts();

        mLog->Printf(LT_MESSAGE, L"Has %d chapters", mNumChapters);

        Restart(now);
    }

    void MngrPlayer::iRefreshRootActionCounts()
    {
        mNumPlayMarkers = 0;
        mNumStopMarkers = 0;
        mNumChapters = 0;
        mHasLoopKey = false;
        mLoopKeyTime = piTick::FromSeconds(60.0 * 60.0 * 1000.0);
        const Layer* root = mDocument != nullptr ? mDocument->GetRoot() : nullptr;
        if (root == nullptr) return;
        const unsigned int numActionKeys = root->GetNumAnimKeys(Layer::AnimProperty::Action);
        for (unsigned int i = 0; i < numActionKeys; i++)
        {
            const Layer::AnimKey* key = root->GetAnimKey(Layer::AnimProperty::Action, i);
            Layer::AnimAction action = static_cast<Layer::AnimAction>(key->mValue.mInt);
            if (action == Layer::AnimAction::Stop)
                mNumStopMarkers++;
            else if (action == Layer::AnimAction::Play)
                mNumPlayMarkers++;
            else if (action == Layer::AnimAction::Loop)
            {
                mHasLoopKey = true;
                mLoopKeyTime = key->mTime;
            }
        }

        mNumChapters = mNumPlayMarkers > 0 ? (mNumPlayMarkers + 1) : (mNumStopMarkers + 1);

    }

    void MngrPlayer::iUpdateCurrentChapter(piTick now)
    {
        mCurrentChapter = 0;
        const Layer* root = mDocument != nullptr ? mDocument->GetRoot() : nullptr;
        if (root == nullptr || mNumChapters == 0) return;
        // Use the evaluated pose clock. Public GetTime adds one tick at stops for
        // reporting; chapter boundaries deliberately use the held pose instead.
        const piTick rootTime = root->GetTimelineElapsed(now);
        const unsigned int count = root->GetNumAnimKeys(Layer::AnimProperty::Action);
        for (unsigned int i = 0; i < count; ++i)
        {
            const Layer::AnimKey* key = root->GetAnimKey(Layer::AnimProperty::Action, i);
            if (key->mTime > rootTime || (mNumPlayMarkers == 0 && key->mTime == rootTime)) break;
            const Layer::AnimAction action = static_cast<Layer::AnimAction>(key->mValue.mInt);
            if ((mNumPlayMarkers > 0 && action == Layer::AnimAction::Play) ||
                (mNumPlayMarkers == 0 && action == Layer::AnimAction::Stop))
                ++mCurrentChapter;
        }
    }

    void MngrPlayer::RefreshRootActionMetadata(piTick now)
    {
        iRefreshRootActionCounts();
        iUpdateCurrentChapter(now);
    }

	bool MngrPlayer::GetHasPlays(void) const
	{
		return mNumPlayMarkers > 0;
	}

    void MngrPlayer::Restart(const piTick now)
    {
        mIsWaitingForInput = false;
        mIsAtEnd = false;
        mIsPaused = false;

        // Start playback of root
#ifdef _DEBUG
        mLog->Printf(LT_DEBUG, L"Restart");
#endif

        iJumpAndReset(now, piTick(0));
    }

    void MngrPlayer::Exit()
    {
#ifdef _DEBUG
        mLog->Printf(LT_DEBUG, L"Exit");
#endif
        Layer * root = mDocument != nullptr ? mDocument->GetRoot() : nullptr;
        mIsPlaying = false;
        if (root != nullptr)
            root->SetPlaying(false);
        mSpawnAreas.Reset();
        mCurrentSpawnAreaId = -1;
    }

    void MngrPlayer::Pause(const piTick now)
    {
        if (mIsPaused) return;

#ifdef _DEBUG
        mLog->Printf(LT_DEBUG, L"Pause");
#endif
        Layer* root = mDocument->GetRoot();

        // Update all layer states
        auto update = [this, &now, &root](Layer* layer, int level, int child, bool instance) -> bool
        {
            switch (layer->GetType())
            {
            case Layer::Type::Sound:
            {
                LayerSound * ls = (LayerSound*)layer->GetImplementation();
                if (ls->GetPlaying())
                {
                    //ls->SetPlaying(false);
					ls->SetPaused(true);
                    //layer->SetVisible(false);
                }
                break;
            }

            case Layer::Type::Paint:
            {
                LayerPaint * lp = (LayerPaint*)layer->GetImplementation();
                lp->SetPlaying(false);
                break;
            }
            case Layer::Type::Group:
                // Authored root holds already freeze all nested timeline clocks.
                if (mIsWaitingForInput)
                    break;
                // stop timelines
                if ((layer == root || layer->GetIsTimeline()) && layer->GetPlaying())
                {
                    layer->SetPlaying(false);
                    layer->SetStopTime(layer->GetStartTime() + layer->GetTimelineElapsed(now));
                }
                break;

            default:
                break;
            }
            return true;
        };

        mDocument->Recurse(update, false, false, false, false);

        mIsPaused = true;
    }

    void MngrPlayer::Resume(const piTick now)
    {
        if (!mIsPaused) return;

#ifdef _DEBUG
        mLog->Printf(LT_DEBUG, L"Resume");
#endif
        Layer* root = mDocument->GetRoot();

        auto update = [this,&now, &root](Layer* layer, int level, int child, bool instance) -> bool
        {
            switch (layer->GetType())
            {
            case Layer::Type::Sound:

			{
				LayerSound * ls = (LayerSound*)layer->GetImplementation();
				ls->SetPaused(false);
                break;
			}

            case Layer::Type::Paint:
            {
                LayerPaint * lp = (LayerPaint*)layer->GetImplementation();
                lp->SetPlaying(true);
                break;
            }
            case Layer::Type::Group:
                // Keep nested clocks aligned with an authored root hold.
                if (mIsWaitingForInput)
                    break;
                // restart timelines
                if (layer->GetWorldVisible() && (layer == root || layer->GetIsTimeline()) && !layer->GetPlaying())
                {
                    layer->SetPlaying(true);
                    const piTick currentPosition = layer->GetTimelineElapsed(now);
                    layer->SetStartTime(now - currentPosition );
                    layer->SetStopTime(now - currentPosition - 1, false);
                }
                break;

            default:
                break;
            }
            return true;
        };
        mDocument->Recurse(update, false, false, false, false);

        mIsPaused = false;
    }

    int MngrPlayer::GetSpawnAreaCount() const
    {
        return (int)mSpawnAreas.GetLength();
    }

    int MngrPlayer::GetSpawnArea()
    {
        return mCurrentSpawnAreaId >= 0 && mCurrentSpawnAreaId < GetSpawnAreaCount() ?
            mCurrentSpawnAreaId : -1;
    }

    int MngrPlayer::GetInitialSpawnArea()
    {
        if (!mIsPlaying || mDocument == nullptr)
            return -1;
        const Layer *initialSpawnArea = mDocument->GetInitialSpawnArea();
        return FindSpawnAreaIdByLayer(initialSpawnArea);
    }

    void MngrPlayer::SetSpawnArea(int spawnAreaId)
    {
        if (spawnAreaId < 0 || spawnAreaId >= GetSpawnAreaCount())
            return;
        if (mCurrentSpawnAreaId != spawnAreaId)
        {
            mCurrentSpawnAreaId = spawnAreaId;
        }
    }

    Layer * MngrPlayer::GetSpawnAreaLayer( int spawnAreaId)
    {
        if (spawnAreaId < 0 || spawnAreaId >= GetSpawnAreaCount())
            return nullptr;
        return mSpawnAreas.Get(spawnAreaId);
    }


    // continue playback from a stop
    bool MngrPlayer::PrepareSpawnAreaPublication(uint32_t count)
    {
        if (std::getenv("IMM_LIVE_EDIT_FAIL_SPAWN_CACHE") != nullptr)
            return false;
        return mSpawnAreas.SetMaxLengthNoShrink(count);
    }

    void MngrPlayer::PublishPreparedSpawnAreas(Layer * const * layers, uint32_t count,
        const Layer * activeLayer)
    {
        // Capacity was reserved before publication; only pointer copies remain.
        mSpawnAreas.SetLength(count);
        for (uint32_t index = 0; index < count; index++)
            mSpawnAreas.Set(layers[index], index);
        mCurrentSpawnAreaId = activeLayer != nullptr ? FindSpawnAreaIdByLayer(activeLayer) : -1;
    }

    void MngrPlayer::iSetSoundsPaused(bool paused)
    {
        if (mDocument == nullptr) return;
        mDocument->Recurse([paused](Layer* layer, int, int, bool) -> bool
        {
            if (layer->GetType() == Layer::Type::Sound)
            {
                LayerSound* sound = static_cast<LayerSound*>(layer->GetImplementation());
                if (!paused || sound->GetPlaying()) sound->SetPaused(paused);
            }
            return true;
        }, false, false, false, false);
    }

    void MngrPlayer::Continue(const ImmCore::piTick now)
    {
        if (mIsWaitingForInput)
        {
#ifdef _DEBUG
            mLog->Printf(LT_DEBUG, L"Continue");
#endif
            // Resume releases the ordinary user-pause media policy. While waiting,
            // its group branch deliberately leaves the authored hold clock alone.
            if (mIsPaused) Resume(now);
            else iSetSoundsPaused(false);
            Layer* root = mDocument->GetRoot();
            const piTick rootTime = root->GetTimelineElapsed(now);
            const piTick shift = now - rootTime - root->GetStartTime();
            // Continue moves the root clock past its hold. Keep active nested
            // timelines at the same local position instead of adding the wait.
            mDocument->Recurse([&](Layer* layer, int, int, bool) -> bool
            {
                if (layer != root && layer->GetType() == Layer::Type::Group && layer->GetIsTimeline() &&
                    (layer->GetPlaying() || layer->HasStopTime()))
                {
                    layer->SetStartTime(layer->GetStartTime() + shift);
                    layer->SetStopTime(layer->GetStopTime() + shift, layer->HasStopTime(), layer->IsStoppedByAction());
                }
                return true;
            }, false, false, false, false);
            root->SetStartTime(now - rootTime);
            root->SetStopTime(now - rootTime -1, false);
            root->SetPlaying(true);
            mIsWaitingForInput = false;
        }
    }


    // go to next play marker (if any play markers) or next stop marker.
    void MngrPlayer::SkipForward(const piTick now)
    {
        if (mNumChapters>0 && mCurrentChapter < mNumChapters - 1)
        {
#ifdef _DEBUG
            mLog->Printf(LT_DEBUG, L"Skip Forward");
#endif
            if (mIsPaused)
                mIsPaused = false;
            Layer* root = mDocument->GetRoot();
            piTick rootTime = root->GetPlaying() ? now - root->GetStartTime() : root->GetStopTime() - root->GetStartTime();
            const int numKeys = root->GetNumAnimKeys(Layer::AnimProperty::Action);
            for (int i = 0; i < numKeys; i++)
            {
                const Layer::AnimKey* key = root->GetAnimKey(Layer::AnimProperty::Action, i);
                const Layer::AnimAction action = static_cast<Layer::AnimAction>(key->mValue.mInt);
                if (key->mTime > rootTime)
                {
                    if (mNumPlayMarkers > 0)
                    {
                        if (action == Layer::AnimAction::Play)
                        {
                            mLog->Printf(LT_MESSAGE, L"Jumping forward to Play index %d, goto time %f  root time %f", i, piTick::ToSeconds(key->mTime),piTick::ToSeconds(rootTime));
                            iJumpAndReset(now, key->mTime);
                            return;
                        }
                    }
                    else //if (mNumStopMarkers > 0)
                    {
                        if (action == Layer::AnimAction::Stop)
                        {
                            iJumpAndReset(now, key->mTime+1);
                            return;
                        }
                    }
                }
            }
        }
    }

    // go to previous play marker (if any play markers) or previous stop marker.
    void MngrPlayer::SkipBack(const piTick now)
    {
        if (mNumChapters > 0 && mCurrentChapter > 0)
        {
#ifdef _DEBUG
            mLog->Printf(LT_DEBUG, L"Skip Back");
#endif
            if (mIsPaused)
                mIsPaused = false;
            Layer* root = mDocument->GetRoot();
            piTick rootTime = root->GetPlaying() ? now - root->GetStartTime() : root->GetStopTime() - root->GetStartTime();
            const int numKeys = root->GetNumAnimKeys(Layer::AnimProperty::Action);
            int numSkip = 0;
            for (int i = 0; i < numKeys; i++)
            {
                const Layer::AnimKey* key = root->GetAnimKey(Layer::AnimProperty::Action, numKeys -i -1);
                const Layer::AnimAction action = static_cast<Layer::AnimAction>(key->mValue.mInt);
                if (key->mTime < rootTime)
                {
                    if (mNumPlayMarkers > 0)
                    {
                        if (action == Layer::AnimAction::Play)
                        {
                            numSkip++;
                            if (numSkip == 2)
                            {
                                mLog->Printf(LT_MESSAGE, L"Jumping back to Play index %d, goto time %f  root time %f", numKeys - i - 1, piTick::ToSeconds(key->mTime), piTick::ToSeconds(rootTime));
                                iJumpAndReset(now, key->mTime);
                                return;
                            }
                        }
                    }
                    else //if (mNumStopMarkers > 0)
                    {
                        if (action == Layer::AnimAction::Stop)
                        {
                            numSkip++;
                            if (numSkip == 2)
                            {
                                iJumpAndReset(now, key->mTime);
                                return;
                            }
                        }
                    }
                }
            }
            if (rootTime > 0)
            {
                mLog->Printf(LT_MESSAGE, L"Jumping back to 0 root time %f", piTick::ToSeconds(rootTime));
                iJumpAndReset(now, piTick(0));
            }
        }
    }

    bool MngrPlayer::SetChapter(const piTick now, size_t chapterIndex)
    {
        if (mNumChapters == 0)
            return false;
        if (chapterIndex >= mNumChapters)
            return false;

        if (mIsPaused)
            mIsPaused = false;

        piTick rootTime = piTick(0);
        if (!iGetChapterStartTime(chapterIndex, &rootTime))
            return false;

        iJumpAndReset(now, rootTime);
        mCurrentChapter = chapterIndex;
        return true;
    }

    void MngrPlayer::SetTime(const piTick now, piTick timeSinceStart, piTick timeSinceStop)
    {
        Layer* root = mDocument->GetRoot();
        const bool isPlaying = timeSinceStop == 0;
#ifdef _DEBUG
        mLog->Printf(LT_DEBUG, L"now: %lld, SetTime %lld %lld", now, timeSinceStart,timeSinceStop);
#endif // _DEBUG
        // Reset nested timelines and resource playback before evaluating a seek.
        // The paused state is preserved and reapplied by Update when needed.
        iJumpAndReset(now, timeSinceStart);

        if (isPlaying)
        {
            root->SetStopTime(now - timeSinceStart-1, false);
            root->SetStartTime(now - timeSinceStart);
            root->SetPlaying(true);
        }
        else
        {
            root->SetStartTime(now - timeSinceStart);
            root->SetStopTime(now - timeSinceStop);
            // we set to true so children keys get visited,
            // since we're at a stop, the stop action will fire immediately
            root->SetPlaying(true);
        }
    }

    void MngrPlayer::GetTime(const piTick now, piTick * timeSinceStart, piTick * timeSinceStop)
    {
        if (timeSinceStart != nullptr) *timeSinceStart = piTick(0);
        if (timeSinceStop != nullptr) *timeSinceStop = piTick(0);
        if (timeSinceStart == nullptr || timeSinceStop == nullptr || mDocument == nullptr)
            return;
        Layer* root = mDocument->GetRoot();
        if (root == nullptr)
            return;

        const bool isStopped = !root->GetPlaying();
        *timeSinceStart = isStopped ? (root->GetStopTime() - root->GetStartTime() + 1) : (now - root->GetStartTime());				   // playing
        *timeSinceStop = isStopped ? (now - root->GetStopTime()) : piTick(0);
#ifdef _DEBUG
        mLog->Printf(LT_DEBUG, L"GetTime: %lld %lld", *timeSinceStart, *timeSinceStart);
#endif // DEBUG

    }

    bool MngrPlayer::GetIsWaiting(void) const
    {
        return mIsWaitingForInput;
    }

    bool MngrPlayer::GetIsFinished(void) const
    {
        if (mNumStopMarkers > 0)
        {
            if (mIsWaitingForInput)
            {
                Layer* root = mDocument->GetRoot();
				//  when we set stop time we do "SetStopTime(GetStartTime() + lastKey->mTime + piTick(-1));"

                piTick rootTime = root->GetStopTime() - root->GetStartTime() + piTick(1);
                const int numKeys = root->GetNumAnimKeys(Layer::AnimProperty::Action);
                for (int i = 0; i < numKeys; i++)
                {
                    const Layer::AnimKey* key = root->GetAnimKey(Layer::AnimProperty::Action, numKeys-i-1);
                    const Layer::AnimAction action = static_cast<Layer::AnimAction>(key->mValue.mInt);
                    if (action == Layer::AnimAction::Stop)
                    {
                        if (rootTime >= key->mTime)
                        {
                            return true; // at or past last stop
                        }
                        else
                        {
                            return false;
                        }
                    }
                }
            }
        }
        return false;
    }

    size_t MngrPlayer::GetCurrentChapter(void) const
    {
        return mCurrentChapter;
    }

    size_t MngrPlayer::GetChapterCount(void) const
    {
        return mNumChapters;
    }


    void MngrPlayer::Update(const piTick now, bool evaluateWhilePaused)
    {
        if (!mIsPaused || evaluateWhilePaused)
        {
            // Update all layer states
            auto update = [this, &now](Layer* layer, int level, int child, bool instance) -> bool
            {
                layer->SetStateAt(now, mLog);
                return true;
            };
            mDocument->Recurse(update, false, false, false, false);

            Layer* root = mDocument->GetRoot();

            if (root->GetPlaying())
            {
                // If hit loop key, rewind and play.
                piTick rootTime = now - root->GetStartTime();
                if (mHasLoopKey && rootTime >= mLoopKeyTime)
                {
                    Restart(now);
                }

                // A paused seek also holds the clock, without requesting chapter input.
                if (root->HasStopTime())
                {
    #ifdef _DEBUG
                    mLog->Printf(LT_DEBUG, L"[IMM_ROOT_STOP] Clock held; waiting for input=%d", root->IsStoppedByAction());
    #endif
                    root->SetPlaying(false);
                    mIsWaitingForInput = root->IsStoppedByAction();
                    // Sound voices advance autonomously; a held layer clock alone
                    // cannot pause their cursor. Renderer ownership reconciles this
                    // intent without a Play, Stop, or seek of an existing voice.
                    if (mIsWaitingForInput) iSetSoundsPaused(true);
                }
            }


            iUpdateCurrentChapter(now);
            if (evaluateWhilePaused && mIsPaused)
            {
                // A seek can activate previously hidden sound/paint/timeline layers.
                // Reapply the normal pause policy to those layers before returning.
                mIsPaused = false;
                Pause(now);
            }
        }
    }

    void MngrPlayer::iJumpAndReset(const ImmCore::piTick now, const ImmCore::piTick rootTime)
    {
        auto resetLayer = [this, &now](Layer* layer, int level, int child, bool instance) -> bool
        {
            switch (layer->GetType())
            {
            case Layer::Type::Sound:
            {
                LayerSound *ls = (LayerSound*)layer->GetImplementation();
                ls->SetPlaying(false);
                ls->SetPaused(mIsPaused);
                ls->SetForceRestart(true);
                break;
            }
            case Layer::Type::Paint:
            {
                LayerPaint * lp = (LayerPaint*)layer->GetImplementation();
                lp->SetPlaying(false);
                break;
            }
            case Layer::Type::Group:
                layer->SetPlaying(false);
                layer->SetStopTime(piTick(0), false);
                break;

            default:
                break;
            }
            layer->SetDefaultPlaybackVisibility();
            return true; // recurse
        };

        mDocument->Recurse(resetLayer, false, false, false, false);

        // Start playback of root
        Layer* root = mDocument->GetRoot();
        root->ResetActionStatesAt(rootTime);
        root->SetVisible(true);
        root->SetPlaying(true);
        root->SetStopTime(now - rootTime - 1, false);
        root->SetStartTime(now - rootTime);
        mIsWaitingForInput = false;

    }

    bool MngrPlayer::iGetChapterStartTime(size_t chapterIndex, ImmCore::piTick* rootTime) const
    {
        if (rootTime == nullptr)
            return false;

        *rootTime = piTick(0);

        if (mDocument == nullptr || mNumChapters == 0 || chapterIndex >= mNumChapters)
            return false;
        if (chapterIndex == 0)
            return true;

        Layer* root = mDocument->GetRoot();
        if (root == nullptr)
            return false;

        const int numKeys = root->GetNumAnimKeys(Layer::AnimProperty::Action);
        size_t markerCount = 0;
        for (int i = 0; i < numKeys; i++)
        {
            const Layer::AnimKey* key = root->GetAnimKey(Layer::AnimProperty::Action, i);
            const Layer::AnimAction action = static_cast<Layer::AnimAction>(key->mValue.mInt);
            if (mNumPlayMarkers > 0)
            {
                if (action != Layer::AnimAction::Play)
                    continue;
                markerCount++;
                if (markerCount == chapterIndex)
                {
                    *rootTime = key->mTime;
                    return true;
                }
            }
            else
            {
                if (action != Layer::AnimAction::Stop)
                    continue;
                markerCount++;
                if (markerCount == chapterIndex)
                {
                    *rootTime = key->mTime + 1;
                    return true;
                }
            }
        }

        return false;
    }

    int MngrPlayer::FindSpawnAreaIdByLayer(const Layer* spawnAreaLayer) const
    {
        for (int i = 0; i < GetSpawnAreaCount(); ++i)
        {
            if (mSpawnAreas.Get(i) == spawnAreaLayer)
            {
                return i;
            }
        }
        return -1;
    }

}
