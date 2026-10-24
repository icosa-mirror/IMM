#pragma once
#include <atomic>
#include <cstddef>
#include <vector>

#include <functional>

#include "libImmCore/src/libVR/piVR.h"

#include "layer.h"
#include "layerPaint.h"

namespace ImmImporter
{

class Sequence
{
public:
    struct ImportState
    {
        std::atomic<bool> continueLoading{true};
        std::atomic<unsigned int> activeWorkers{0};
    };
    ImportState & GetImportState() { return mImportState; }
    const ImportState & GetImportState() const { return mImportState; }


	typedef enum { NoComicFeatures = 0, ComicFeatures = 1 } FileVersion;

	enum class Type : int
	{
		Still = 0,
		Animated = 1,
		Comic = 2,
		COUNT = 3
	};

	struct Asset
	{
		uint64_t mFileOffset;
		uint64_t mSize;
	};

    Sequence();
    ~Sequence();

    bool Init( const Type & type, uint16_t caps, const ImmCore::vec3 & backgroundColor );
    void Deinit(ImmCore::piLog *log);

	Type     GetType(void) const;
	uint16_t GetCaps(void) const;
	Layer *  GetRoot(void) const;
    ImmCore::vec3 GetBackgroundColor(void) const;

	// pure memory manager. Does not handle hierarchy or resource allocations
    Layer* CreateLayer(Layer* parent);
    bool PrepareLayerPublication(void);
    bool PublishPreparedLayer(Layer * layer);
    bool RollbackPreparedLayer(Layer * layer);
    bool RemovePublishedLayer(Layer * layer);
    bool RemovePublishedLayers(Layer * const * layers, uint32_t count);
    uint32_t GetLayerStorageCount(void) const;
    Layer* FindLayerByFullName(const wchar_t* name);
	bool   AddAsset(uint64_t offset, uint64_t size);
	const Asset* GetAsset(uint32_t assetID) const;

    bool GetAnimateOnStart(void) const;
	void SetAnimateOnStart(bool b);
    // Explicit choice supersedes prior MakeDefault executions. The getter below
    // remains the effective choice used by navigation.
	void SetInitialSpawnArea(const Layer* vp);
	const Layer* GetInitialSpawnArea(void) const;
    const Layer* GetInitialSpawnAreaBaseline() const { return mInitialSpawnAreaBaseline; }
    bool PrepareDefaultActionCapacity(size_t additionalIdentities);
    bool RecordDefaultActionExecution(const Layer* layer, ImmCore::piTick time) noexcept;
    void ReconcileDefaultActionKeys(const Layer* layer, const std::vector<Layer::AnimKey>& keys) noexcept;
    void ReconcileDefaultActionKeys() noexcept;
    // Must run before any removed target Layer is freed.
    void ReconcileDefaultActionTargets(Layer* const* survivors, size_t count) noexcept;
    void ResetDefaultActionHistory() noexcept;
    void SetSpawnAreaNeedsUpdate(bool state);
    bool GetSpawnAreaNeedsUpdate() const;

    typedef std::function<bool(Layer* layer, int level, int child, bool instance)> VisitorF;
    bool Recurse(VisitorF v, bool doNotRecurseCollapsedGroups, bool doNotRecurseHiddenGroups, bool doNotRecurseLockedGroups, bool doPostExec);

    // only needed for exporter
    void  GetInfo(Type *resType, bool *resHasSound, int*umSpawnAreas) const;
    uint32_t GetFrameRate() const;
    void SetFrameRate(uint32_t fps); // TODO: this is only temporarily needed before we serialize global framerate at sequence level

private:
    struct DefaultActionExecution
    {
        const Layer* target;
        uint32_t layerId;
        ImmCore::piTick time;
        bool active;
    };
    void RecomputeInitialSpawnArea() noexcept;
    const Layer* mInitialSpawnAreaBaseline = nullptr;
    std::vector<DefaultActionExecution> mDefaultActionExecutions;
    ImportState mImportState;
    ImmCore::piTArray<Layer*> mLayers; // just a flat array. Hierarchy is captured through the child pointers in LayerGroup
	ImmCore::piTArray<Asset>  mAssetTable;
    ImmCore::vec3             mBackgroundColor;
    Layer *          mRoot = nullptr;
	Type             mType;
	uint16_t         mCaps;  // bit: can be grabbed

	bool             mAnimateOnStart;
    bool             mSpawnAreaNeedsUpdate;
    const Layer*     mInitialSpawnArea = nullptr;
    uint32_t         mFrameRate;

};

}
