#include "libImmCore/src/libBasics/piFile.h"
#include "libImmCore/src/libMesh/piMeshSerialized.h"

#include "layerModel3d.h"

using namespace ImmCore;


namespace ImmExporter
{

    LayerModel::LayerModel() {}

    LayerModel::~LayerModel() {}

    bool LayerModel::Init(uint32_t version)
    {
        mMesh.Init();
        mVersion = version;
        mRenderWireFrame = false;
        mShadingModel = ShadingModel::Unlit;
        mHasAsset = false;
        return true;
    }

    void LayerModel::Deinit()
    {
        if(mHasAsset)
            mMesh.DeInit();
        mMesh.Init();
        mHasAsset = false;
    }

    bool LayerModel::AssignAsset(const piMesh *asset, bool move)
    {
        if (!asset) return false;
        if (asset == &mMesh) return mHasAsset;
        Deinit();
        if (move)
            mMesh.InitMove(asset);
        else
        {
            // The legacy generic Clone copies only one stream. Model documents
            // must preserve split colour streams, all index arrays and bounds.
            piTArray<uint8_t> bytes;
            if (!bytes.Init(0, false)) return false;
            bool valid = asset->WriteToMemory(&bytes) &&
                bytes.GetLength() <= 256ull * 1024 * 1024 &&
                piMeshValidateSerialized(bytes.GetAddress(0), bytes.GetLength());
            if (valid) {
                bytes.SetLength(0);
                mMesh.mVertexData = {};
                mMesh.mFaceData = {};
                valid = mMesh.ReadFromMemory(&bytes);
                if (!valid) { mMesh.DeInit(); mMesh.Init(); }
            }
            bytes.End();
            if (!valid) return false;
        }
        mHasAsset = true;
        return true;
    }


    const LayerModel::ShadingModel LayerModel::GetShadingModel(void) const
    {
        return mShadingModel;
    }

    void LayerModel::SetShadingModel(const ShadingModel shadingModel)
    {
        mShadingModel = shadingModel;
    }

	const bool LayerModel::GetRenderWireframe(void) const
	{
		return mRenderWireFrame;
	}

    void LayerModel::SetRenderWireframe(bool renderWireFrame)
    {
        mRenderWireFrame = renderWireFrame;
    }

	const bool LayerModel::HasBBox(void) const
	{
		return true;
	}
	const bound3 & LayerModel::GetBBox(void) const
	{
		return mMesh.mBBox;
	}

	piMesh *LayerModel::GetMesh(void)
	{
        if (!mHasAsset)
            return nullptr;
		return &mMesh;
	}
}
