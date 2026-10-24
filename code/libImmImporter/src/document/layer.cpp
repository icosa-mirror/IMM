#include <stdlib.h>
#include <algorithm>
#include <new>

#include "layer.h"
#include "layerInstance.h"
#include "layerModel3d.h"
#include "layerPaint.h"
#include "layerPaintStatic.h"
#include "layerPicture.h"
#include "layerSound.h"
#include "layerSpawnArea.h"
#include "libImmCore/src/libBasics/piString.h"
#include "libImmCore/src/libBasics/piVecTypes.h"
#include "libImmCore/src/libBasics/piDebug.h"
#include "libImmCore/src/libBasics/piFile.h"
#include "libImmCore/src/libBasics/piStr.h"
#include "sequence.h"
using namespace ImmCore;

namespace ImmImporter
{
	static bool iIsBBoxReasonable(const bound3d& b)
	{
		if (isEmpty(b))
			return false;

		const double limit = 1.0e6;
		if (b.mMinX < -limit || b.mMinX > limit) return false;
		if (b.mMaxX < -limit || b.mMaxX > limit) return false;
		if (b.mMinY < -limit || b.mMinY > limit) return false;
		if (b.mMaxY < -limit || b.mMaxY > limit) return false;
		if (b.mMinZ < -limit || b.mMinZ > limit) return false;
		if (b.mMaxZ < -limit || b.mMaxZ > limit) return false;
		return true;
	}

    bool Layer::EvaluationContext::Contains(const Layer * layer) const
    {
        if (mBoundary == nullptr || mBoundary->GetType() != Type::Group || !mBoundary->GetIsTimeline())
            return false;
        for (const Layer * current = layer; current != nullptr; current = current->GetParent())
            if (current == mBoundary) return true;
        return false;
    }

    bool Layer::EvaluationContext::ConsumeAction(const Layer * layer, piTick time)
    {
        if (!mActionsEnabled || !Contains(layer) || layer == mBoundary) return false;
        for (const auto & consumed : mConsumedActions)
            if (consumed.layerId == layer->GetID() && consumed.time == time) return false;
        mConsumedActions.push_back(ConsumedAction{layer->GetID(), time});
        return true;
    }

	Layer::Layer(Sequence* sq, Layer* parent, uint32_t id)
	{
		mSequence = sq;
		mParent = parent;
		mID = id;
	}

	Layer::~Layer() {}

	bool Layer::Init(Type type, const wchar_t* name, bool visible, const trans3d & transform, const trans3d& pivot, float opacity, bool isTimeLine, piTick duration, uint32_t maxRepeatCount, uint32_t assetID, piLog *log, bool publishToParent)
	{
        if (mInitialized)
            return false;
		mType = type;
        mLoaded = false;
        mStartTime = piTick(0);
        mStopTime = piTick(0);
        mIsPlaying = false;
        mHasStopTime = false;
        mStoppedByAction = false;
	mVisible = visible;
	mCanonicalVisible = visible;
    mVisibilityOverrideEnabled = false;
    mVisibilityOverrideValue = visible;
    mTransformOverrideEnabled = false;
    mTransformOverrideValue = transform;
	mPotentiallyVisible = visible;
		mImplementation = nullptr;
		mTransform = transform;
		mCanonicalTransform = transform;
        mPivotTransform = pivot;
		mOpacity = opacity;
		mCanonicalOpacity = opacity;
		mOpacityOverrideEnabled = false;
		mOpacityOverrideValue = opacity;
		mIsTimeline = isTimeLine;
		mDuration = duration;
		mMaxRepeatCount = maxRepeatCount;
        mEvaluatedMaxRepeatCount = maxRepeatCount;
        mStartOffset = 0;
		mAssetID = assetID;
		mDrawInTime = 1.0;
        mCanonicalDrawInTime = 1.0;
		mAnimParams[0] = 0.0;
		mAnimParams[1] = 0.7;
		mAnimParams[2] = 0.3;
		mAnimParams[3] = 0.7;
        mLastActiveSpanStart = piTick(0);
        mLastActiveSpanClockOrigin = piTick(0);
        mHasActiveSpanClockOrigin = false;

        if (!mName.InitCopyW(name))
        {
            mName.End();
            return false;
        }

        if (!iCalcFullName())
        {
            mName.End();
            return false;
        }

        if (type == Type::Group)
        {
            if (!mChildren.Init(16, true))
            {
                mChildren.End();
                mFullName.End();
                mName.End();
                return false;
            }
            mImplementation = nullptr;
        }

        // Publish only after the complete layer has initialized successfully.
        if (publishToParent && mParent != nullptr && !mParent->mChildren.Append(this, true))
        {
            if (type == Type::Group)
                mChildren.End();
            mFullName.End();
            mName.End();
            return false;
        }

        mInitialized = true;
		return true;
	}


	void Layer::Deinit(piLog *log)
	{
        if (!mInitialized)
            return;
        mInitialized = false;

		//log->Printf(LT_MESSAGE, L"  Layer::Deinit 1 : name : %s,   type: %d,    imp = %x", this->mName.GetS(), static_cast<int>(mType), (mImplementation ==nullptr)?0:uint64_t(mImplementation));

		if (mType == Type::Group)
		{
			uint64_t num = mChildren.GetLength();
			for (uint64_t i = 0; i < num; i++)
			{
				Layer* la = mChildren.Get(i);
				la->Deinit(log);
			}
		}

        const int maxProps = static_cast<int>(AnimProperty::MAX);
        for (int i = 0; i < maxProps; i++)
            std::vector<AnimKey>().swap(mAnimKeys[i]);

        mName.End();
        mFullName.End();

        if (mType == Type::Group)
        {
            mChildren.End();
        }
        
        // don't deinit aync loaded layers not loaded (were not inited)
        if (!mLoaded) return;

		if (mType == Type::Paint)
		{
			LayerPaint *li = (LayerPaint*)mImplementation;
			li->Deinit();
			delete li;
		}
		else if (mType == Type::Model)
		{
			LayerModel *li = (LayerModel*)mImplementation;
			li->Deinit();
			delete li;
		}
		else if (mType == Type::Picture)
		{
			LayerPicture *li = (LayerPicture*)mImplementation;
			li->Deinit();
			delete li;
		}
		else if (mType == Type::Sound)
		{
			LayerSound *li = (LayerSound*)mImplementation;
			li->Deinit();
			delete li;
		}
		else if (mType == Type::SpawnArea)
		{
			LayerSpawnArea *li = (LayerSpawnArea*)mImplementation;
			li->Deinit();
			delete li;
		}		
	}

trans3d Layer::GetTransformToWorld(const EvaluationContext * context) const
{
    if (context != nullptr && (!context->Contains(this) || this == context->mBoundary))
        return trans3d::identity();
    trans3d mat = GetTransform();
    Layer * ancestor = mParent;
    while (ancestor != nullptr && (context == nullptr || ancestor != context->mBoundary))
    {
        mat = ancestor->GetTransform() * mat;
        ancestor = ancestor->mParent;
    }
    return mat;
}

    bool Layer::iCalcFullName(void)
    {
        Layer * stack[64];
        stack[0] = this;
        int num = 0;

        Layer * ancestor = mParent;
        while (ancestor != nullptr)
        {
            if (num + 1 >= 64)
                return false;
            stack[++num] = ancestor;
            ancestor = ancestor->mParent;
        }

        if (!mFullName.InitCopy(&stack[num]->GetName()))
        {
            mFullName.End();
            return false;
        }

        for (int i = num - 1; i >= 0; i--)
        {
            if (!mFullName.AppendWC(L'/') || !mFullName.AppendS(&stack[i]->GetName()))
            {
                mFullName.End();
                return false;
            }
        }
        return true;
    }

	const piString* Layer::GetFullName(void) const { return &mFullName; }

	KeepAlive * Layer::GetKeepAlive(void)
	{
		return &mKeepAlive;
	}

	LayerImplementation Layer::GetImplementation(void) const { return (LayerImplementation)mImplementation; }

	void Layer::SetImplementation(LayerImplementation implementation)
	{
		mImplementation = implementation;
	}

	bool* Layer::GetVisibleRef(void) { return &mCanonicalVisible; }

	float * Layer::GetOpacityRef(void)
	{
		return &mCanonicalOpacity;
	}

	bool Layer::GetVisible(void) const { return mVisibilityOverrideEnabled ? mVisibilityOverrideValue : mVisible; }
	bool Layer::GetCanonicalVisible(void) const { return mCanonicalVisible; }
	bool Layer::GetPotentiallyVisible(void) const { return mPotentiallyVisible; }
    bool Layer::GetWorldVisible(const EvaluationContext * context) const
    {
        if (context != nullptr && !context->Contains(this)) return false;
        for (const Layer * layer = this; layer != nullptr; layer = layer->mParent)
        {
            if (context != nullptr && layer == context->mBoundary) break;
            if (!layer->GetVisible()) return false;
        }
        return true;
    }
bool Layer::GetVisibilityOverrideEnabled(void) const { return mVisibilityOverrideEnabled; }
bool Layer::GetVisibilityOverrideValue(void) const { return mVisibilityOverrideValue; }
bool Layer::GetTransformOverrideEnabled(void) const { return mTransformOverrideEnabled; }
const trans3d &Layer::GetTransformOverrideValue(void) const { return mTransformOverrideValue; }


	void Layer::SetVisible(bool visible)
	{
		mCanonicalVisible = visible;
		mVisible = visible;
        if (visible) mPotentiallyVisible = true;
	}
	void Layer::SetCanonicalVisible(bool visible)
	{
		mCanonicalVisible = visible;
        if (visible) mPotentiallyVisible = true;
		if (GetNumAnimKeys(AnimProperty::Visibility) == 0)
			mVisible = visible;
	}
    void Layer::SetVisibilityOverride(bool enabled, bool visible)
    {
        mVisibilityOverrideEnabled = enabled;
        mVisibilityOverrideValue = visible;
        if (enabled && visible) mPotentiallyVisible = true;
    }
	void Layer::SetPotentiallyVisible(bool visible) { mPotentiallyVisible = visible; }

	const Layer::Type Layer::GetType(void) const { return mType; }


	const piString& Layer::GetName(void) const { return mName; }

	float Layer::GetOpacity(void) const { return mOpacityOverrideEnabled ? mOpacityOverrideValue : mOpacity; }
	float Layer::GetCanonicalOpacity(void) const { return mCanonicalOpacity; }
	bool Layer::GetOpacityOverrideEnabled(void) const { return mOpacityOverrideEnabled; }
	float Layer::GetOpacityOverrideValue(void) const { return mOpacityOverrideValue; }
    float Layer::GetWorldOpacity(const EvaluationContext * context) const
    {
        if (context != nullptr && !context->Contains(this)) return 0.0f;
        float opacity = 1.0f;
        for (const Layer * layer = this; layer != nullptr; layer = layer->mParent)
        {
            if (context != nullptr && layer == context->mBoundary) break;
            opacity *= layer->GetOpacity();
        }
        return opacity;
    }

	void Layer::SetOpacity(float opacity)
	{
		mCanonicalOpacity = opacity;
		mOpacity = opacity;
	}
	void Layer::SetCanonicalOpacity(float opacity)
	{
		mCanonicalOpacity = opacity;
		if (GetNumAnimKeys(AnimProperty::Opacity) == 0)
			mOpacity = opacity;
	}
	void Layer::SetOpacityOverride(bool enabled, float opacity)
	{
		mOpacityOverrideEnabled = enabled;
		mOpacityOverrideValue = opacity;
	}

trans3d Layer::GetTransform(void) const { return mTransformOverrideEnabled ? mTransformOverrideValue : mTransform; }
trans3d Layer::GetCanonicalTransform(void) const { return mCanonicalTransform; }
trans3d Layer::GetPivot(void) const { return mPivotTransform; }

void Layer::SetTransform(const trans3d & mat)
{
    mCanonicalTransform = mat;
    mTransform = mat;
}
void Layer::SetCanonicalTransform(const trans3d & mat)
{
    mCanonicalTransform = mat;
    if (GetNumAnimKeys(AnimProperty::Transform) == 0)
        mTransform = mat;
}
void Layer::SetTransformOverride(bool enabled, const trans3d & mat)
{
    mTransformOverrideEnabled = enabled;
    mTransformOverrideValue = mat;
}


	void Layer::SetDrawInTime(double v) { mCanonicalDrawInTime = v; mDrawInTime = v; }
	double Layer::GetDrawInTime(void) const { return mDrawInTime; }



	uint32_t Layer::GetID(void) const
	{
		return mID;
	}

	bool Layer::GetLayerUsesDrawin(void) const
	{
		return !mAnimKeys[static_cast<int>(AnimProperty::DrawInTime)].empty();
	}


	double Layer::GetAnimParam(int id) const
	{
		return mAnimParams[id];
	}



	bool Layer::HasBBox(void) const
	{
		if (mType == Type::Group)
		{
			const uint64_t numChildren = mChildren.GetLength();
			for (uint64_t i = 0; i < numChildren; i++)
			{
				Layer* ch = mChildren.Get(i);
				if (ch != nullptr && ch->HasBBox())
					return true;
			}
			return false;
		}
		if (mType == Type::Paint)
		{
			const LayerPaint* lp = (LayerPaint*)mImplementation;
			if (!lp->HasBBox()) return false;
			return iIsBBoxReasonable(f2d(lp->GetBBox()));
		}
		if (mType == Type::Picture)
		{
			const LayerPicture* lp = (LayerPicture*)mImplementation;
			if (!lp->HasBBox()) return false;
			return iIsBBoxReasonable(f2d(lp->GetBBox()));
		}
		if (mType == Type::Model)
		{
			const LayerModel* lp = (LayerModel*)mImplementation;
			if (!lp->HasBBox()) return false;
			return iIsBBoxReasonable(f2d(lp->GetBBox()));
		}

		return false;
	}

	const bound3d Layer::GetBBox(void)
	{
		if (mType == Type::Paint)   return f2d(((LayerPaint  *)mImplementation)->GetBBox());
		if (mType == Type::Picture) return f2d(((LayerPicture*)mImplementation)->GetBBox());
		if (mType == Type::Model)   return f2d(((LayerModel  *)mImplementation)->GetBBox());
		if (mType == Type::Group)
		{
			bound3d bbox = bound3d(1e30);    // this whole thing can probably be cached as long as no children or subchildren has changed position, oritnetation, scale or content bounds
			const uint64_t numChildren = mChildren.GetLength();
			for (uint64_t i = 0; i < numChildren; i++)
			{
				Layer* ch = mChildren.Get(i);
				if (ch->HasBBox())
				{
					bound3d tmp = ch->GetBBox();
					if (!iIsBBoxReasonable(tmp))
						continue;
					tmp = btransform(tmp, ch->GetTransform());
					if (!iIsBBoxReasonable(tmp))
						continue;
					bbox = include(bbox, tmp);
				}
			}

			if (!iIsBBoxReasonable(bbox))
				return bound3d(1e30);

			return bbox;
		}

		piAssert(false); // we should never arrive here
		return bound3d(1e30);
	}

	uint32_t Layer::GetNumChildren(void) const
    {
        return mChildren.IsInitialized() ? static_cast<uint32_t>(mChildren.GetLength()) : 0;
    }

	Layer* Layer::GetChild(uint32_t id) { return mChildren.Get(id); }

	bool Layer::PrepareChildPublication(void)
	{
		return mType == Type::Group &&
			mChildren.SetMaxLengthNoShrink(mChildren.GetLength() + 1);
	}

	bool Layer::PublishPreparedChild(Layer * child)
	{
		return mType == Type::Group && child != nullptr && child->mParent == this &&
			mChildren.Append(child, false);
	}

	bool Layer::InsertPreparedChild(Layer * child, uint32_t childIndex)
	{
		return mType == Type::Group && child != nullptr && child->mParent == this &&
			childIndex <= mChildren.GetLength() &&
			mChildren.InsertAndShift(&child, childIndex, false) != nullptr;
	}

	bool Layer::RemovePublishedChild(Layer * child)
	{
		if (mType != Type::Group || child == nullptr || child->mParent != this)
			return false;
		for (uint64_t index = 0; index < mChildren.GetLength(); index++)
		{
			if (mChildren.Get(index) == child)
			{
				mChildren.RemoveAndShift(index);
				return true;
			}
		}
		return false;
	}

	bool Layer::ReorderPublishedChild(Layer * child, uint32_t childIndex)
	{
		if (mType != Type::Group || child == nullptr || child->mParent != this ||
			childIndex >= mChildren.GetLength())
			return false;
		for (uint64_t currentIndex = 0; currentIndex < mChildren.GetLength(); currentIndex++)
		{
			if (mChildren.Get(currentIndex) != child)
				continue;
			if (currentIndex == childIndex)
				return true;
			mChildren.RemoveAndShift(currentIndex);
			return mChildren.InsertAndShift(&child, childIndex, false) != nullptr;
		}
		return false;
	}

	void Layer::SetPublishedParent(Layer * parent) { mParent = parent; }

	void Layer::PublishPreparedFullName(piString * fullName)
	{
		piAssert(fullName != nullptr);
		mFullName = std::move(*fullName);
	}

    void Layer::PublishPreparedName(piString * name)
    {
        piAssert(name != nullptr);
        mName = std::move(*name);
    }

	Layer* Layer::GetParent() const { return mParent; }

	bool Layer::Recurse(VisitorF func, int level, unsigned int child, bool instance, bool doNotRecurseCollapsedGroups, bool doNotRecurseHiddenGroups, bool doNotRecurseLockedGroups, bool doPostExec)
	{
		if (!doPostExec)
		{
			if (!func(this, level, child, instance))
				return false;
		}

		if (mType == Type::Group)
		{
			if (!(doNotRecurseHiddenGroups && !this->mVisible))
			{
				const uint64_t numChildren = mChildren.GetLength();
				for (uint64_t i = 0; i < numChildren; i++)
				{
					Layer* ch = mChildren.Get(i);
					if (!ch->Recurse(func, level + 1, static_cast<unsigned int>(i), instance, doNotRecurseCollapsedGroups, doNotRecurseHiddenGroups, doNotRecurseLockedGroups, doPostExec))
						return false;
				}
			}
		}

		if (doPostExec)
		{
			if (!func(this, level, child, instance))
				return false;
		}


		return true;
	}

	bool Layer::GetPlaying() const
	{
		return mIsPlaying;
	}

	void Layer::SetPlaying(bool playing)
	{
		mIsPlaying = playing;
	}

	bool Layer::GetIsTimeline(void) const
	{
		return mIsTimeline;
	}

    Layer * Layer::GetTimeline(const EvaluationContext * context) const
    {
        if (context != nullptr)
        {
            if (!context->Contains(this)) return nullptr;
            if (this == context->mBoundary) return context->mBoundary;
        }
        Layer * layer = mParent;
        while (layer != nullptr && !layer->GetIsTimeline())
        {
            if (context != nullptr && layer == context->mBoundary) return layer;
            layer = layer->GetParent();
        }
        return layer != nullptr ? layer : mSequence->GetRoot();
    }

	uint32_t Layer::GetMaxRepeatCount() const
	{
		return mMaxRepeatCount;
	}

    uint32_t Layer::GetEvaluatedMaxRepeatCount() const
    {
        return mEvaluatedMaxRepeatCount;
    }

	void Layer::SetStartTime(piTick time)
	{
		mStartTime = time;
	}

	piTick Layer::GetStartTime() const
	{
		return mStartTime;
	}

	void Layer::SetStopTime(piTick time, bool hasStopTime, bool stoppedByAction)
	{
		mStopTime = time;
        mHasStopTime = hasStopTime;
        mStoppedByAction = hasStopTime && stoppedByAction;
	}

	piTick Layer::GetStopTime() const
	{
		return mStopTime;
	}


    bool Layer::HasStopTime() const
    {
        return mHasStopTime;
    }

    bool Layer::IsStoppedByAction() const
    {
        return mStoppedByAction;
    }

    piTick Layer::GetTimelineElapsed(piTick now, const EvaluationContext * context) const
    {
        if (context != nullptr)
        {
            if (!context->Contains(this)) return piTick(0);
            if (this == context->mBoundary) return context->mBoundaryElapsed;
        }
        const Layer * root = context != nullptr ? context->mBoundary : mSequence->GetRoot();
        const bool rootHeld = context != nullptr ? (context->mHeld || context->mActionHeld) : root->HasStopTime();
        if (!mHasStopTime && !rootHeld) return now - mStartTime;
        piTick clock = mHasStopTime ? mStopTime : now;
        if (this != root && rootHeld)
        {
            Layer * timeline = GetTimeline(context);
            const piTick timelineStart = context != nullptr && timeline == root ?
                context->mBoundaryStartTime : timeline->GetStartTime();
            const piTick parentClock = timelineStart + timeline->GetTimelineElapsed(now, context);
            if (clock > parentClock) clock = parentClock;
        }
        const piTick elapsed = clock - mStartTime;
        return elapsed < piTick(0) ? piTick(0) : elapsed;
    }

	piTick Layer::GetDuration() const
	{
		return mDuration;
	}

	void Layer::SetPosition(const vec3d & pos)
	{
		mTransform.mTranslation = pos;
	}

	void Layer::SetRotation(const quatd & rot)
	{
		mTransform.mRotation = rot;
	}

	void Layer::SetScale(double scale)
	{
		mTransform.mScale = scale;
	}

    void Layer::SetMaxRepeatCount(uint32_t count)
    {
        mMaxRepeatCount = count;
        mEvaluatedMaxRepeatCount = count;
    }

		void Layer::SetIsTimeline(bool timeline)
		{
				mIsTimeline = timeline;
		}

    void Layer::SetDuration(piTick duration)
    {
        mDuration = duration;
    }

	uint32_t Layer::GetNumAnimKeys(AnimProperty property) const
	{
		return static_cast<uint32_t>(mAnimKeys[static_cast<int>(property)].size());
	}

	const Layer::AnimKey* Layer::GetAnimKey(AnimProperty property, unsigned int index) const
	{
		if (index >= mAnimKeys[static_cast<int>(property)].size())
			return nullptr;

		return &mAnimKeys[static_cast<int>(property)][index];
	}

    bool Layer::GetLoaded(void) const
    {
        return mLoaded;
    }


    const Layer::AnimKey * Layer::GetAnimKeyAt(AnimProperty property, piTick time) const
    {
        const int propIndex = static_cast<int>(property);
        const size_t numKeys = mAnimKeys[propIndex].size();
        for (size_t i = 0; i < numKeys; i++)
        {
            const AnimKey * key = &mAnimKeys[propIndex][i];
            if (key->mTime == time)
                return key;
        }
        return nullptr;
    }

	bool Layer::AddKey(piTick time, AnimProperty property, const AnimValue& value, InterpolationType interpolation)
	{
        const bool added = SetAnimKey(mAnimKeys[static_cast<int>(property)], time, value, interpolation);
        if (added && property == AnimProperty::Visibility && value.mBool) mPotentiallyVisible = true;
        return added;
	}

	bool Layer::SetAnimKey(std::vector<AnimKey> & keys, piTick time,
		const AnimValue & value, InterpolationType interpolation)
	{
		auto key = std::lower_bound(keys.begin(), keys.end(), time,
			[](const AnimKey & candidate, piTick target)
			{
				return candidate.mTime < target;
			});
		try
		{
			if (key == keys.end() || key->mTime != time)
				key = keys.insert(key, AnimKey{});
		}
		catch (const std::bad_alloc &)
		{
			return false;
		}
		key->mTime = time;
		key->mValue = value;
		key->mInterpolation = interpolation;
		return true;
	}

	bool Layer::RemoveAnimKey(std::vector<AnimKey> & keys, piTick time)
	{
		auto key = std::lower_bound(keys.begin(), keys.end(), time,
			[](const AnimKey & candidate, piTick target)
			{
				return candidate.mTime < target;
			});
		if (key == keys.end() || key->mTime != time)
			return false;
		keys.erase(key);
		return true;
	}

	bool Layer::CopyAnimKeys(AnimProperty property, std::vector<AnimKey> & keysOut) const
	{
		try
		{
			keysOut = mAnimKeys[static_cast<int>(property)];
			return true;
		}
		catch (const std::bad_alloc &)
		{
			return false;
		}
	}

    void Layer::ReconcileActionPlayback(std::vector<AnimKey> & prepared, piTick localTime) const noexcept
    {
        const auto & live = mAnimKeys[static_cast<int>(AnimProperty::Action)];
        for (AnimKey & key : prepared)
        {
            const auto current = std::lower_bound(live.begin(), live.end(), key.mTime,
                [](const AnimKey & candidate, piTick time) { return candidate.mTime < time; });
            const bool unchanged = current != live.end() && current->mTime == key.mTime &&
                current->mValue.mInt == key.mValue.mInt;
            key.mValue.mBool = unchanged ? current->mValue.mBool : key.mTime < localTime;
        }
    }
	void Layer::ReplaceAnimKeys(AnimProperty property, std::vector<AnimKey> && keys)
	{
		mAnimKeys[static_cast<int>(property)].swap(keys);
        if (property == AnimProperty::Visibility)
            for (const auto & key : mAnimKeys[static_cast<int>(property)])
                if (key.mValue.mBool) { mPotentiallyVisible = true; break; }
	}

    Layer::PlaybackState Layer::CapturePlaybackState() const
    {
        PlaybackState state;
        state.mVisible = mVisible;
        state.mPotentiallyVisible = mPotentiallyVisible;
        state.mOpacity = mOpacity;
        state.mTransform = mTransform;
        state.mDrawInTime = mDrawInTime;
        for (int index = 0; index < 4; ++index) state.mAnimParams[index] = mAnimParams[index];
        state.mVisibilityOverrideEnabled = mVisibilityOverrideEnabled;
        state.mVisibilityOverrideValue = mVisibilityOverrideValue;
        state.mOpacityOverrideEnabled = mOpacityOverrideEnabled;
        state.mOpacityOverrideValue = mOpacityOverrideValue;
        state.mTransformOverrideEnabled = mTransformOverrideEnabled;
        state.mTransformOverrideValue = mTransformOverrideValue;
        state.mEvaluatedMaxRepeatCount = mEvaluatedMaxRepeatCount;
        state.mIsPlaying = mIsPlaying;
        state.mHasStopTime = mHasStopTime;
        state.mStoppedByAction = mStoppedByAction;
        state.mStartTime = mStartTime;
        state.mStopTime = mStopTime;
        state.mLastActiveSpanStart = mLastActiveSpanStart;
        state.mLastActiveSpanClockOrigin = mLastActiveSpanClockOrigin;
        state.mHasActiveSpanClockOrigin = mHasActiveSpanClockOrigin;
        state.mStartOffset = mStartOffset;
        return state;
    }

    void Layer::RestorePlaybackState(const PlaybackState & state, piTick wallClockShift)
    {
        mVisible = state.mVisible;
        mPotentiallyVisible = state.mPotentiallyVisible;
        mOpacity = state.mOpacity;
        mTransform = state.mTransform;
        mDrawInTime = state.mDrawInTime;
        for (int index = 0; index < 4; ++index) mAnimParams[index] = state.mAnimParams[index];
        mVisibilityOverrideEnabled = state.mVisibilityOverrideEnabled;
        mVisibilityOverrideValue = state.mVisibilityOverrideValue;
        mOpacityOverrideEnabled = state.mOpacityOverrideEnabled;
        mOpacityOverrideValue = state.mOpacityOverrideValue;
        mTransformOverrideEnabled = state.mTransformOverrideEnabled;
        mTransformOverrideValue = state.mTransformOverrideValue;
        mEvaluatedMaxRepeatCount = state.mEvaluatedMaxRepeatCount;
        mIsPlaying = state.mIsPlaying;
        mHasStopTime = state.mHasStopTime;
        mStoppedByAction = state.mStoppedByAction;
        const piTick groupShift = mType == Type::Group ? wallClockShift : piTick(0);
        mStartTime = state.mStartTime + groupShift;
        mStopTime = state.mStopTime + groupShift;
        mLastActiveSpanStart = state.mLastActiveSpanStart;
        mLastActiveSpanClockOrigin = state.mLastActiveSpanClockOrigin +
            (state.mHasActiveSpanClockOrigin ? wallClockShift : piTick(0));
        mHasActiveSpanClockOrigin = state.mHasActiveSpanClockOrigin;
        mStartOffset = state.mStartOffset;
    }

    void Layer::ResetPlaybackStateForEvaluation()
    {
        mVisible = mCanonicalVisible;
        mOpacity = mCanonicalOpacity;
        mTransform = mCanonicalTransform;
        mDrawInTime = mCanonicalDrawInTime;
        mAnimParams[0] = 0.0; mAnimParams[1] = 0.7;
        mAnimParams[2] = 0.3; mAnimParams[3] = 0.7;
        mEvaluatedMaxRepeatCount = mMaxRepeatCount;
        mIsPlaying = false;
        mHasStopTime = false;
        mStoppedByAction = false;
        mStartTime = piTick(0);
        mStopTime = piTick(0);
        mLastActiveSpanStart = piTick(0);
        mLastActiveSpanClockOrigin = piTick(0);
        mHasActiveSpanClockOrigin = false;
        mStartOffset = piTick(0);
    }

    bool Layer::CanEvaluateInContext(const EvaluationContext * context) const
    {
        return context == nullptr || (context->Contains(this) && this != context->mBoundary &&
            (mType != Type::Sound || context->mAudioEnabled));
    }

    void Layer::SetStateAt(piTick now, piLog * log, EvaluationContext * context)
    {
        SetStateAtImpl(now, log, false, AnimProperty::MAX, context);
        if (CanEvaluateInContext(context) && GetVisible()) mPotentiallyVisible = true;
    }

    void Layer::ReevaluatePaintPlaybackAt(piTick now, piLog * log, EvaluationContext * context)
    {
        if (CanEvaluateInContext(context) && mType == Type::Paint &&
            dynamic_cast<LayerPaintStatic *>((LayerPaint *)GetImplementation()) != nullptr)
        {
            SetStateAtImpl(now, log, true, AnimProperty::MAX, context);
            if (GetVisible()) mPotentiallyVisible = true;
        }
    }

    void Layer::ReevaluateTransformAt(piTick now, piLog * log, EvaluationContext * context)
    {
        if (!CanEvaluateInContext(context)) return;
        if (GetNumAnimKeys(AnimProperty::Transform) == 0)
        {
            mTransform = mCanonicalTransform;
            return;
        }
        SetStateAtImpl(now, log, false, AnimProperty::Transform, context);
    }

    void Layer::ReevaluateVisibilityAt(piTick now, piLog * log, EvaluationContext * context)
    {
        if (!CanEvaluateInContext(context)) return;
        if (GetNumAnimKeys(AnimProperty::Visibility) == 0) mVisible = mCanonicalVisible;
        else SetStateAtImpl(now, log, false, AnimProperty::Visibility, context);
        if (GetVisible()) mPotentiallyVisible = true;
    }

    void Layer::ReevaluateOpacityAt(piTick now, piLog * log, EvaluationContext * context)
    {
        if (!CanEvaluateInContext(context)) return;
        if (GetNumAnimKeys(AnimProperty::Opacity) == 0) mOpacity = mCanonicalOpacity;
        else SetStateAtImpl(now, log, false, AnimProperty::Opacity, context);
        if (mType == Type::Sound)
            static_cast<LayerSound *>(GetImplementation())->SetVolume(GetWorldOpacity(context));
    }

    void Layer::ReevaluateDrawInTimeAt(piTick now, piLog * log, EvaluationContext * context)
    {
        if (!CanEvaluateInContext(context)) return;
        if (GetNumAnimKeys(AnimProperty::DrawInTime) == 0) mDrawInTime = mCanonicalDrawInTime;
        else SetStateAtImpl(now, log, false, AnimProperty::DrawInTime, context);
    }

    void Layer::SetStateAtImpl(piTick now, piLog * log, bool reevaluatePaint,
        AnimProperty onlyProperty, EvaluationContext * context)
    {
        if (!CanEvaluateInContext(context)) return;
        const bool propertyOnly = onlyProperty != AnimProperty::MAX;
        Layer* root = context != nullptr ? context->mBoundary : mSequence->GetRoot();
        const bool isRoot = this == root;
        const bool isGroup = mType == Type::Group;
        const bool isTimeline = isGroup && mIsTimeline;
        const bool isPaintLayer = mType == Type::Paint;
        const bool isSpawnArea = mType == Type::SpawnArea;
        LayerPaint* lp = isPaintLayer ? (LayerPaint*)GetImplementation() : nullptr;
        const bool isSoundLayer = mType == Type::Sound;
        LayerSound * ls = isSoundLayer ? (LayerSound*)GetImplementation() : nullptr;

        Layer* timeline = GetTimeline(context);
        const auto durationFor = [&](const Layer * layer) {
            return context != nullptr && layer == root ? context->mBoundaryDuration : layer->GetDuration();
        };
        const auto repeatFor = [&](const Layer * layer) {
            return context != nullptr && layer == root ? context->mBoundaryRepeatCount : layer->GetEvaluatedMaxRepeatCount();
        };
        const auto startFor = [&](const Layer * layer) {
            return context != nullptr && layer == root ? context->mBoundaryStartTime : layer->GetStartTime();
        };
        const auto playingFor = [&](const Layer * layer) {
            return context != nullptr && layer == root ? context->mPlaying : layer->GetPlaying();
        };
        const piTick timelineDuration = durationFor(timeline);

        if (!reevaluatePaint && !propertyOnly && mParent && !mParent->GetWorldVisible(context))
        {
            if (isSoundLayer) ls->SetPlaying(false);
            return;
        }
            

        // handle stop frames to correct for stop time
        piTick timeFromStart = timeline->GetTimelineElapsed(now, context);

        const auto resolveTimelineRepeat = [&](piTick keyTime) {
            mEvaluatedMaxRepeatCount = mMaxRepeatCount;
            const int keyCount = GetNumAnimKeys(AnimProperty::Loop);
            for (int index = 0; index < keyCount; ++index)
            {
                const AnimKey * key = GetAnimKey(AnimProperty::Loop, index);
                if (key->mTime > keyTime)
                    break;
                mEvaluatedMaxRepeatCount = key->mValue.mBool ? 0 : 1;
            }
        };
        // The root's Loop control uses its unwrapped clock, avoiding a lookup
        // whose own wrap/clamp decision changes which Loop key is selected.
        if (isRoot && !propertyOnly)
            resolveTimelineRepeat(timeFromStart);

        const piTick timelineWrapped = (timelineDuration != 0) ? timeFromStart % timelineDuration : timeFromStart;

        // handle looping clips
        const int64_t numPlays = (timelineDuration != 0) ? piTick::CastInt(timeFromStart) / piTick::CastInt(timelineDuration) : 0;
        const piTick timelineTime = (repeatFor(timeline) == 0 || numPlays < repeatFor(timeline)) ?
            timelineWrapped : timelineDuration;

        // Other timeline controls belong to their parent's effective clock.
        // Resolve before descendants read this timeline's repeat behavior.
        if (!isRoot && !isPaintLayer && !propertyOnly)
            resolveTimelineRepeat(timelineTime);

        // Resolve step playback keys before Visibility computes the clip's local
        // time. Missing keys (including a seek before the first key) restore the
        // canonical repeat and zero tick offset on every evaluation.
        if (!propertyOnly) mStartOffset = piTick(0);
        if (isPaintLayer && !propertyOnly)
            lp->SetEvaluatedMaxRepeatCount(lp->GetMaxRepeatCount());
        for (const AnimProperty prop : { AnimProperty::Loop, AnimProperty::Offset })
        {
            if (propertyOnly) break;
            if (prop == AnimProperty::Loop && !isPaintLayer)
                continue;
            const AnimKey * activeKey = nullptr;
            const int numKeys = GetNumAnimKeys(prop);
            for (int index = 0; index < numKeys; ++index)
            {
                const AnimKey * key = GetAnimKey(prop, index);
                if (key->mTime > timelineTime)
                    break;
                activeKey = key;
            }
            if (activeKey == nullptr)
                continue;
            if (prop == AnimProperty::Loop)
                lp->SetEvaluatedMaxRepeatCount(activeKey->mValue.mBool ? 0 : 1);
            else
                mStartOffset = piTick(int64_t(activeKey->mValue.mInt));
        }

        piTick timeSinceStart = piTick(0);

        const int maxProps = static_cast<int>(AnimProperty::MAX);
        for (int i = 0; i < maxProps; i++)
        {
            const AnimProperty prop = static_cast<AnimProperty>(i);
            if (propertyOnly && prop != onlyProperty) continue;
            if (prop == AnimProperty::Offset || prop == AnimProperty::Loop)
                continue;
            if (reevaluatePaint && prop != AnimProperty::Visibility)
                continue;

            const int numKeys = GetNumAnimKeys(prop);
            if (numKeys == 0) continue; // skip property

            // Find the last key, assuming keys are sorted
            AnimKey* lastKey = nullptr;
            int lastKeyIndex = -1;
            for (int j = 0; j < numKeys; j++)
            {
                const AnimKey* key = GetAnimKey(prop, j);
                // we only start a key if we're past it.
                // this allows stopping at the exact time where keys start without starting them
                if (key->mTime > timelineTime)
                    break;
                lastKeyIndex = j;
                lastKey = (AnimKey*)key;
            }

            // Time is before the first key, do nothing
            if (lastKeyIndex == -1)
            {
                if (prop == AnimProperty::Visibility)
                {
                    mVisible = false;
                    if (isSoundLayer) ls->SetPlaying(false);
                    continue;
                }
                else if (prop == AnimProperty::Action)
                {
                    if (isSpawnArea && context == nullptr) ResetActionStatesAt(piTick(0));
                    continue;
                }
                lastKey = (AnimKey*)GetAnimKey(prop, 0);
                lastKeyIndex = numKeys - 1;
            }


            // Find the next key is there is one
            const bool isLastKey = lastKeyIndex == numKeys - 1;
            const AnimKey* nextKey = isLastKey ? nullptr : GetAnimKey(prop, lastKeyIndex + 1);

            // interpolation param between last and next keys
            const piTick totalTime = isLastKey ? piTick(0) : nextKey->mTime - lastKey->mTime;
            double t = isLastKey ? 0.0 : piTick::CastDouble(timelineTime - lastKey->mTime) / piTick::CastDouble(totalTime);

            // apply easing curve if needed
            switch (lastKey->mInterpolation)
            {
            case InterpolationType::EaseIn:
                t = t * t*t;
                break;
            case InterpolationType::EaseOut:
            {
                const double t1 = 1.0 - t;
                t = 1.0 - (t1*t1*t1);
                break;
            }
            case InterpolationType::Smoothstep:
                t = t * t*(3.0 - 2.0*t);
                break;
            default:
                break;
            }

            // set the state from the keys
            switch (prop)
            {
            case AnimProperty::Action:
            {
                AnimAction action = static_cast<AnimAction>(lastKey->mValue.mInt);
                if (context != nullptr ? context->ConsumeAction(this, lastKey->mTime) : !lastKey->mValue.mBool)
                {
                    if (context == nullptr) lastKey->mValue.mBool = true; // flag to only run actions once

                    if (action == AnimAction::Stop)
                    {
                        // Descendant action keys use the parent timeline clock,
                        // while this layer's start already includes its visibility
                        // start and content offset. Resolve the action in that
                        // parent occurrence before storing an absolute hold.
                        const piTick actionTime = isRoot ? GetStartTime() + lastKey->mTime :
                            startFor(timeline) + timeFromStart - timelineTime + lastKey->mTime;
                        SetStopTime(actionTime + piTick(-1), true, true);
#ifdef _DEBUG
                        log->Printf(LT_DEBUG, L"[LayerSetState] Stop action %s", mName.GetS());
#endif
                    }
                    if (action == AnimAction::MakeDefault && isSpawnArea)
                    {
                        if (context != nullptr)
                            context->mDefaultSpawnAreaLayerId = GetID();
                        else
                        {
                            mSequence->SetInitialSpawnArea(this);
                            mSequence->SetSpawnAreaNeedsUpdate(true);
                        }
                        log->Printf(LT_DEBUG, L"[LayerSetState] SpawnArea action %s", mName.GetS());
                    }
                }
                break;
            }
            case AnimProperty::Visibility:
            {
                if (propertyOnly)
                {
                    mVisible = lastKey->mValue.mBool;
                    break;
                }
                // Visibility controls if some layers are alive or dead for playback

                 // span change since last evaluation retriggers sounds
                const bool isNewSpan = (context != nullptr && !mHasActiveSpanClockOrigin) || lastKey->mTime != mLastActiveSpanStart;
                if (isNewSpan) mLastActiveSpanStart = lastKey->mTime;

                if (lastKey->mValue.mBool)
                {
                    timeSinceStart = (timelineTime - lastKey->mTime) + mStartOffset;
                    const piTick startTime = startFor(timeline) + timeFromStart - timeSinceStart;

                    if (reevaluatePaint)
                    {
                        // Select a held paint frame without starting playback,
                        // touching action flags or retiming any timeline.
                        mVisible = true;
                        break;
                    }

                    if (!playingFor(timeline))
                        break;
                    
                    // A finite repeated timeline also starts a new child span at
                    // each wrap. Include the root; it can be the looping ancestor.
                    bool isParentSequenceLooping = false;
                    Layer* ti = timeline;
                    // Once this timeline is clamped at its finite end, an
                    // advancing wall clock is not another wrap.
                    const bool timelineCanWrap = repeatFor(timeline) == 0 || numPlays < repeatFor(timeline);
                    while (timelineCanWrap && ti != nullptr)
                    {
                        if ((ti == root || ti->GetIsTimeline()) && repeatFor(ti) != 1 && durationFor(ti) != 0)
                        {
                            const piTick elapsed = ti->GetTimelineElapsed(now, context);
                            const int64_t plays = piTick::CastInt(elapsed) / piTick::CastInt(durationFor(ti));
                            if (repeatFor(ti) == 0 || plays < repeatFor(ti))
                            {
                                isParentSequenceLooping = true;
                                break;
                            }
                        }
                        if (ti == root) break;
                        ti = ti->GetParent();
                    }

                    // Detect the visibility span's clock advance, excluding its
                    // content offset. Decreasing Offset retimes content without
                    // creating another parent-loop iteration. Clock advances
                    // from a real loop or resume retain the existing behavior.
                    const piTick spanClockOrigin = startFor(timeline) + timeFromStart -
                        (timelineTime - lastKey->mTime);
                    const bool hasRestarted = mHasActiveSpanClockOrigin ?
                        spanClockOrigin > mLastActiveSpanClockOrigin : startTime > mStartTime;
                    mLastActiveSpanClockOrigin = spanClockOrigin;
                    mHasActiveSpanClockOrigin = true;
                    // The root wraps its own clock without retiming its start;
                    // resetting that start would make a finite root loop forever.
                    const bool clipRestartedAndParentLooping = !isRoot && isParentSequenceLooping && hasRestarted;
                   
                    if (isTimeline && ( (!mIsPlaying && !isRoot) || clipRestartedAndParentLooping))
                    {
                        SetPlaying(true);
                        SetStartTime(startTime);
                        SetStopTime(startTime - 1, false);
#ifdef _DEBUG
                        log->Printf(LT_DEBUG, L"[LayerSetState] Starting timeline %s", mName.GetS());
#endif
                    }
                    else if (isPaintLayer && !lp->GetPlaying())
                    {
                        lp->SetTime(timeSinceStart);
                        lp->SetPlaying(true);
#ifdef _DEBUG
                        log->Printf(LT_DEBUG, L"[LayerSetState] Starting anim clip %s", mName.GetS());
#endif
                    }
                    else if (isSoundLayer && ((!mVisible && !ls->GetPlaying()) || isNewSpan || clipRestartedAndParentLooping))
                    {
                        mStartTime = startTime;
                        ls->SetPlaying(true, piTick::ToMicroSeconds(timeSinceStart));
                        if (context != nullptr) ls->SetForceRestart(true);
#ifdef _DEBUG
                        log->Printf(LT_DEBUG, L"[LayerSetState] Starting sound %s", mName.GetS());
#endif
                    }
                }
                else if (!lastKey->mValue.mBool)
                {
                    if (reevaluatePaint)
                    {
                        mVisible = false;
                        break;
                    }
                    const piTick timeSinceStop = (timelineTime - lastKey->mTime) + mStartOffset;
                    const piTick stopTime = context != nullptr ?
                        startFor(timeline) + timeFromStart - timeSinceStop : now - timeSinceStop;

                    if (isTimeline && mIsPlaying)
                    {
                        SetPlaying(false);
                        SetStopTime(stopTime);
#ifdef _DEBUG
                        log->Printf(LT_DEBUG, L"[LayerSetState] Stopping timeline %s", mName.GetS());
#endif
                    }
                    else if (isPaintLayer && lp->GetPlaying())
                    {
                        lp->SetPlaying(false);
#ifdef _DEBUG
                        log->Printf(LT_DEBUG, L"[LayerSetState] Stopping animclip %s", mName.GetS());
#endif
                    }
                    else if (isSoundLayer && ls->GetPlaying())
                    {
                        ls->SetPlaying(false);
                        mStopTime = stopTime;
#ifdef _DEBUG
                        log->Printf(LT_DEBUG, L"[LayerSetState] Stopping sound %s", mName.GetS());
#endif
                    }
                }
                // never interpolate visiblity
                mVisible = lastKey->mValue.mBool;
                break;
            }
            case AnimProperty::Opacity:
                // opacity means volume for sound layers
                if (lastKey->mInterpolation == InterpolationType::None || isLastKey)
                    mOpacity = lastKey->mValue.mFloat;
                else
                    mOpacity = lastKey->mValue.mFloat + (nextKey->mValue.mFloat - lastKey->mValue.mFloat)*float(t);
                break;

            case AnimProperty::Transform:
                if (lastKey->mInterpolation == InterpolationType::None || isLastKey)
                    mTransform = lastKey->mValue.mTransform;
                else
                {
                    mTransform = mix(lastKey->mValue.mTransform * mPivotTransform, nextKey->mValue.mTransform * mPivotTransform, t) * invert(mPivotTransform);
                }
                break;
           
            case AnimProperty::DrawInTime:
                if (lastKey->mInterpolation == InterpolationType::None || isLastKey)
                    mDrawInTime = lastKey->mValue.mDouble;
                else
                    mDrawInTime = lastKey->mValue.mDouble + (nextKey->mValue.mDouble - lastKey->mValue.mDouble)*t;
                break;
            case AnimProperty::Loop:
                break;
            default:
                break;

            } // switch
        } // props loop


        // A published paint key must also refresh latent selection while hidden,
        // so later unhide does not expose the previous key's frame. Visibility
        // spans still define local time; inactive/keyless spans retain local0.
        if (propertyOnly) return;
        if (reevaluatePaint)
        {
            static_cast<LayerPaintStatic *>(lp)->SelectFrameAtTime(timeSinceStart);
            return;
        }
        // Handle layerpaint animations
        if (GetWorldVisible(context))
        {
            if (isPaintLayer && lp->GetPlaying())
            {
                lp->SetTime(timeSinceStart);
            }
            if (isSoundLayer && ls->GetPlaying())
            {
                ls->SetVolume(GetWorldOpacity(context));
            }
        }
    }

    void Layer::SetPivot(const trans3d& p)
    {
        mPivotTransform = p;
    }

    void Layer::SetLoaded(bool loaded)
    {
        mLoaded = loaded;
    }

    void Layer::SetDefaultPlaybackVisibility()
    {
        mVisible = false;
        mHasActiveSpanClockOrigin = false;
    }

    void Layer::ResetActionStatesAt(piTick time)
    {
        // reset action states
        const int numActionKeys = GetNumAnimKeys(Layer::AnimProperty::Action);
        if (numActionKeys > 0)
        {
            for (int i = 0; i < numActionKeys; i++)
            {
                Layer::AnimKey* key = (Layer::AnimKey*) GetAnimKey(Layer::AnimProperty::Action, i);
                key->mValue.mBool = key->mTime < time;
            }
        }
    }

	const uint32_t Layer::GetAssetID(void) const
	{
		return mAssetID;
	}


    void Layer::GetLocalTimeOffsetAndVisiblity(piTick rootTime, piTick* offset, bool* visible)
    {
        // Make a stack of parents
        int numParents = 0;
        Layer* parentStack[64];
        Layer* tmp = mParent;
        while (tmp)
        {
            parentStack[numParents++] = tmp;
            tmp = tmp->GetParent();
        }

        piTick rootToLocal = piTick(0);
        piTick localTime = rootTime;

        // Walk from root
        for (int64_t i = numParents - 1; i >= 0; i--)
        {
            const int numVisKeys = parentStack[i]->GetNumAnimKeys(Layer::AnimProperty::Visibility);
            bool foundKey = false;

            for (int j = numVisKeys - 1; j >= 0; j--)
            {
                const Layer::AnimKey* key = parentStack[i]->GetAnimKey(Layer::AnimProperty::Visibility, j);
                if (key->mTime <= localTime)
                {
                    // we're at the first key before time
                    if (key->mValue.mBool == true)
                    {
                        // we're in a span
                        if (parentStack[i]->GetIsTimeline())
                        {
                            rootToLocal += key->mTime;
                            localTime -= key->mTime;
                        }
                        foundKey = true;
                        break;
                    }
                    else
                    {
                        foundKey = false;
                        break;
                    }
                }
            }
            // we're not world visible at this point
            if (foundKey == false)
            {
                *visible = false;
                *offset = rootToLocal;
                return;
            }
        }

        // we're world visible, return the offset to root
        *visible = true;
        *offset = rootToLocal;
        return;
    }


}
