#include "libImmCore/src/libBasics/piLog.h"
#include "libImmCore/src/libBasics/piStr.h"
#include "libImmCore/src/libMesh/piMeshSerialized.h"

#include "../document/layer.h"
#include "../document/layerModel3d.h"
#include "fromImmersiveLayerModel.h"
using namespace ImmCore;

namespace ImmImporter
{

    namespace fiLayerModel
    {

        LayerImplementation ReadData(piIStream *fp, piLog* log)
        {
            uint32_t metadata[3];
            if (fp->Read(metadata, sizeof(metadata)) != sizeof(metadata) ||
                metadata[0] != 1 || metadata[1] > 1 || metadata[2] > 1) return nullptr;
            LayerModel *me = new LayerModel();
            if (!me) return nullptr;
            if (!me->Init(metadata[2] != 0, static_cast<LayerModel::ShadingModel>(metadata[1]))) {
                delete me;
                return nullptr;
            }

            return me;
        }



        bool ReadAsset(LayerImplementation implementation, piIStream *fp, piLog* log)
        {
            auto* model = static_cast<LayerModel*>(implementation);
            uint64_t size = 0;
            if (!model || fp->Read(&size, sizeof(size)) != sizeof(size) ||
                size == 0 || size > 256ull * 1024 * 1024) return false;
            piTArray<uint8_t> data;
            if (!data.Init(size, false)) return false;
            bool valid = fp->Read(data.GetAddress(0), size) == size &&
                piMeshValidateSerialized(data.GetAddress(0), size);
            if (valid) {
                auto* mesh = model->GetMesh();
                // Zero unused streams before the existing reader allocates. Its
                // failure cleanup can then safely visit every declared stream.
                mesh->DeInit();
                mesh->mVertexData = {};
                mesh->mFaceData = {};
                valid = mesh->ReadFromMemory(&data);
                if (!valid) model->Deinit();
            }
            data.End();
            return valid;
        }


    }
}
