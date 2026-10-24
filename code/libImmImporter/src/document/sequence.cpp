#include "libImmCore/src/libBasics/piVecTypes.h"
#include "libImmCore/src/libBasics/piDebug.h"
#include "libImmCore/src/libBasics/piStr.h"

#include "sequence.h"
using namespace ImmCore;

namespace ImmImporter
{


Sequence::Sequence() {}

Sequence::~Sequence() {}

bool Sequence::Init(const Type & type, uint16_t caps, const vec3 & backgroundColor)
{
    mInitialSpawnArea = nullptr;
    mInitialSpawnAreaBaseline = nullptr;
    mDefaultActionExecutions.clear();
    mBackgroundColor = backgroundColor;
	mType = type;
	mCaps = caps;
    mSpawnAreaNeedsUpdate = false;
    mAnimateOnStart = false;
    mFrameRate = 0;  // TODO --- pass this as argument, and DESTROY the SetFramerate() function

	if (!mLayers.Init(256, true)) // this array grows as needed
        return false;

	if (!mAssetTable.Init(1024, false))
		return false;

	mRoot = this->CreateLayer(nullptr);
	if (!mRoot)
		return false;

    return true;
}

void Sequence::Deinit(piLog *log) 
{ 
    std::vector<DefaultActionExecution>().swap(mDefaultActionExecutions);
    mInitialSpawnAreaBaseline = nullptr;
    mInitialSpawnArea = nullptr;
    // Hierarchy teardown releases implementations first. The flat registry owns
    // the Layer allocations, including unpublished layers whose Init failed.
    if (mRoot != nullptr)
        mRoot->Deinit(log);
    if (mLayers.IsInitialized())
    {
        for (uint64_t i = 0; i < mLayers.GetLength(); ++i)
            mLayers.Get(i)->Deinit(log);
        for (uint64_t i = 0; i < mLayers.GetLength(); ++i)
            delete mLayers.Get(i);
        mLayers.End();
    }
    mRoot = nullptr;
    mInitialSpawnArea = nullptr;
    if (mAssetTable.IsInitialized()) mAssetTable.End();
}

bool Sequence::AddAsset(uint64_t offset, uint64_t size)
{
	Asset *as = mAssetTable.Alloc(1, true);
	if (!as) return false;
	as->mFileOffset = offset;
	as->mSize = size;
	return true;
}

const Sequence::Asset* Sequence::GetAsset(uint32_t assetID) const
{
	return mAssetTable.GetAddress(static_cast<uint64_t>(assetID));
}

const Layer* Sequence::GetInitialSpawnArea(void) const
{
	return mInitialSpawnArea;
}

bool Sequence::GetSpawnAreaNeedsUpdate() const 
{
    return mSpawnAreaNeedsUpdate;
}

void Sequence::SetSpawnAreaNeedsUpdate(bool state)
{
    mSpawnAreaNeedsUpdate = state;
}


void Sequence::SetInitialSpawnArea(const Layer* la)
{
    mInitialSpawnAreaBaseline = la;
    mDefaultActionExecutions.clear();
    mInitialSpawnArea = la;
}

bool Sequence::PrepareDefaultActionCapacity(size_t additionalIdentities)
{
    if (additionalIdentities > mDefaultActionExecutions.max_size() - mDefaultActionExecutions.size())
        return false;
    try
    {
        mDefaultActionExecutions.reserve(mDefaultActionExecutions.size() + additionalIdentities);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void Sequence::RecomputeInitialSpawnArea() noexcept
{
    const Layer* effective = mInitialSpawnAreaBaseline;
    for (const auto& execution : mDefaultActionExecutions)
        if (execution.active) effective = execution.target;
    if (effective != mInitialSpawnArea)
    {
        mInitialSpawnArea = effective;
        mSpawnAreaNeedsUpdate = true;
    }
}

bool Sequence::RecordDefaultActionExecution(const Layer* layer, piTick time) noexcept
{
    if (layer == nullptr || layer->GetType() != Layer::Type::SpawnArea) return false;
    const uint32_t id = layer->GetID();
    for (size_t i = 0; i < mDefaultActionExecutions.size(); ++i)
    {
        const auto& old = mDefaultActionExecutions[i];
        if (old.target != layer || old.layerId != id || old.time != time) continue;
        // Re-execution changes execution order, not the identity's authored time.
        const DefaultActionExecution execution{ layer, id, time, true };
        for (size_t j = i + 1; j < mDefaultActionExecutions.size(); ++j)
            mDefaultActionExecutions[j - 1] = mDefaultActionExecutions[j];
        mDefaultActionExecutions.back() = execution;
        RecomputeInitialSpawnArea();
        return true;
    }
    if (mDefaultActionExecutions.size() == mDefaultActionExecutions.capacity()) return false;
    mDefaultActionExecutions.push_back({ layer, id, time, true });
    RecomputeInitialSpawnArea();
    return true;
}

void Sequence::ReconcileDefaultActionKeys(const Layer* layer, const std::vector<Layer::AnimKey>& keys) noexcept
{
    if (layer == nullptr) return;
    for (auto& execution : mDefaultActionExecutions)
    {
        if (execution.target != layer || execution.layerId != layer->GetID()) continue;
        execution.active = false;
        for (const auto& key : keys)
            if (key.mTime == execution.time && key.mValue.mInt == static_cast<uint32_t>(Layer::AnimAction::MakeDefault))
            {
                execution.active = true;
                break;
            }
    }
    RecomputeInitialSpawnArea();
}

void Sequence::ReconcileDefaultActionTargets(Layer* const* survivors, size_t count) noexcept
{
    const auto survives = [survivors, count](const Layer* target) {
        if (target == nullptr) return true;
        for (size_t i = 0; survivors != nullptr && i < count; ++i)
            if (survivors[i] == target) return true;
        return false;
    };
    if (!survives(mInitialSpawnAreaBaseline)) mInitialSpawnAreaBaseline = nullptr;
    size_t retained = 0;
    for (size_t i = 0; i < mDefaultActionExecutions.size(); ++i)
        if (survives(mDefaultActionExecutions[i].target))
            mDefaultActionExecutions[retained++] = mDefaultActionExecutions[i];
    mDefaultActionExecutions.resize(retained);
    RecomputeInitialSpawnArea();
}

void Sequence::ReconcileDefaultActionKeys() noexcept
{
    // Deleted targets must already have been pruned before their Layer is freed.
    // Reconcile the whole live scene before selecting a winner, including when
    // canonical keys changed while main playback was frozen by a preview.
    for (auto& execution : mDefaultActionExecutions)
    {
        execution.active = false;
        const Layer* layer = execution.target;
        if (layer->GetID() != execution.layerId) continue;
        const uint32_t count = layer->GetNumAnimKeys(Layer::AnimProperty::Action);
        for (uint32_t i = 0; i < count; ++i)
        {
            const Layer::AnimKey* key = layer->GetAnimKey(Layer::AnimProperty::Action, i);
            if (key->mTime == execution.time && key->mValue.mInt == static_cast<uint32_t>(Layer::AnimAction::MakeDefault))
            {
                execution.active = true;
                break;
            }
        }
    }
    RecomputeInitialSpawnArea();
}

void Sequence::ResetDefaultActionHistory() noexcept
{
    mInitialSpawnAreaBaseline = mInitialSpawnArea;
    mDefaultActionExecutions.clear();
}


Layer* Sequence::GetRoot(void) const { return mRoot; }

vec3 Sequence::GetBackgroundColor(void) const { return mBackgroundColor; }

Layer* Sequence::CreateLayer(Layer* parent)
{
	const int id = static_cast<int>(mLayers.GetLength());
	
	Layer* la = new Layer(this, parent, id);

    if (!mLayers.Append(la, true))
    {
        delete la;
        return nullptr;
    }


    return la;
}

bool Sequence::PrepareLayerPublication(void)
{
    return mLayers.SetMaxLengthNoShrink(mLayers.GetLength() + 1);
}

bool Sequence::PublishPreparedLayer(Layer * layer)
{
    return layer != nullptr && mLayers.Append(layer, false);
}

bool Sequence::RollbackPreparedLayer(Layer * layer)
{
    const uint64_t length = mLayers.GetLength();
    if (layer == nullptr || length == 0 || mLayers.Get(length - 1) != layer)
        return false;
    mLayers.RemoveAndShift(length - 1);
    return true;
}

bool Sequence::RemovePublishedLayer(Layer * layer)
{
    if (layer == nullptr || layer == mRoot)
        return false;
    for (uint64_t index = 0; index < mLayers.GetLength(); index++)
    {
        if (mLayers.Get(index) == layer)
        {
            mLayers.RemoveAndShift(index);
            return true;
        }
    }
    return false;
}

bool Sequence::RemovePublishedLayers(Layer * const * layers, uint32_t count)
{
    if (layers == nullptr || count == 0)
        return false;
    for (uint32_t candidateIndex = 0; candidateIndex < count; candidateIndex++)
    {
        Layer * candidate = layers[candidateIndex];
        if (candidate == nullptr || candidate == mRoot)
            return false;
        bool found = false;
        for (uint64_t layerIndex = 0; layerIndex < mLayers.GetLength(); layerIndex++)
        {
            if (mLayers.Get(layerIndex) == candidate)
            {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }

    uint64_t writeIndex = 0;
    for (uint64_t layerIndex = 0; layerIndex < mLayers.GetLength(); layerIndex++)
    {
        Layer * current = mLayers.Get(layerIndex);
        bool remove = false;
        for (uint32_t candidateIndex = 0; candidateIndex < count; candidateIndex++)
        {
            if (layers[candidateIndex] == current)
            {
                remove = true;
                break;
            }
        }
        if (!remove)
        {
            mLayers.Set(current, writeIndex);
            writeIndex++;
        }
    }
    mLayers.SetLength(writeIndex);
    return true;
}

uint32_t Sequence::GetLayerStorageCount(void) const
{
    return mLayers.IsInitialized() ? static_cast<uint32_t>(mLayers.GetLength()) : 0;
}


bool Sequence::Recurse(VisitorF v, bool doNotRecurseCollapsedGroups, bool doNotRecurseHiddenGroups, bool doNotRecurseLockedGroups, bool doPostExec)
{
    return mRoot != nullptr && mRoot->Recurse(v, 0, 0, false, doNotRecurseCollapsedGroups, doNotRecurseHiddenGroups, doNotRecurseLockedGroups, doPostExec);
}


Layer* Sequence::FindLayerByFullName(const wchar_t* name)
{
    if (!mLayers.IsInitialized()) return nullptr;
    const uint64_t num = mLayers.GetLength();
    for (uint64_t i = 0; i < num; i++)
    {
        Layer* la = mLayers.Get(i);
        const piString* fullName = la->GetFullName();
        if (piwstrcmp(fullName->GetS(), name) == 0)
            return la;
    }
    return nullptr;
}


bool Sequence::GetAnimateOnStart(void) const
{
    return mAnimateOnStart;
}

void Sequence::SetAnimateOnStart(bool b)
{
	mAnimateOnStart = b;
}

Sequence::Type Sequence::GetType(void) const
{
	return mType;
}
uint16_t Sequence::GetCaps(void) const
{
	return mCaps;
}

// --- everything below is only needed for the exporter ---
uint32_t Sequence::GetFrameRate() const
{
    return mFrameRate;
};

void Sequence::SetFrameRate(uint32_t fps)
{
    mFrameRate = fps;
}

void Sequence::GetInfo(Type *resType, bool *resHasSound, int* numSpawnAreas) const
{
    Type oType = Type::Still;
    bool oHasSound = false;

    bool hasAnimation = false;
    int numSA = 0;
    auto visitor = [&hasAnimation, &oHasSound, &numSA](Layer* layer, int level, unsigned int child, bool instance) -> bool
    {
        Layer::Type type = layer->GetType();

        // Keyframe animation
        if (type != Layer::Type::SpawnArea && (layer->GetNumAnimKeys(Layer::AnimProperty::Visibility) > 1
            || layer->GetNumAnimKeys(Layer::AnimProperty::Opacity) > 1
            || layer->GetNumAnimKeys(Layer::AnimProperty::Transform) > 1))
        {
            hasAnimation = true;
        }

        if (type == Layer::Type::Paint)
        {
            // Frame by frame animation
            const LayerPaint*lp = (LayerPaint*)layer->GetImplementation();
            if (lp->GetNumDrawings() > 1)
                hasAnimation = true;
        }
        else if (type == Layer::Type::Sound)
        {
            oHasSound = true;
        }
        else if (type == Layer::Type::SpawnArea)
        {
            numSA++;
        }
        return true;
    };

    mRoot->Recurse(visitor, 0, 0, false, false, false, false, false);

    oType = hasAnimation ? Type::Animated : Type::Still;

    // Detect if the timeline has STOPs, and if so, we flag the piece as
    // a COMIC, to signal the user in Spaces' UI what they are about to watch.    
    if (mRoot->GetNumAnimKeys(Layer::AnimProperty::Action) > 0)
    {
        oType = Type::Comic;
    }

    *resType = oType;
    *resHasSound = oHasSound;
    *numSpawnAreas = numSA;
}



}
