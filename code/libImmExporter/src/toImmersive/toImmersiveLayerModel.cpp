#include "libImmCore/src/libBasics/piLog.h"
#include "libImmCore/src/libBasics/piStreamO.h"
#include "libImmCore/src/libMesh/piMeshSerialized.h"

#include "../document/layer.h"
#include "../document/layerModel3d.h"

using namespace ImmCore;
namespace ImmExporter
{

	namespace tiLayerModel
	{
		bool ExportData(piOStream *fp, LayerImplementation imp)
		{
            auto* model = static_cast<LayerModel*>(imp);
            if (!model || !model->GetMesh()) return false;
            const auto shading = static_cast<uint32_t>(model->GetShadingModel());
            if (shading > 1) return false;
            fp->WriteUInt32(1); // model metadata version
            fp->WriteUInt32(shading);
            fp->WriteUInt32(model->GetRenderWireframe() ? 1 : 0);
            return true;
		}

		bool ExportAsset(piOStream *fp, LayerImplementation imp)
		{
            auto* model = static_cast<LayerModel*>(imp);
            if (!model || !model->GetMesh()) return false;
            piTArray<uint8_t> data;
            if (!data.Init(0, false)) return false;
            const bool valid = model->GetMesh()->WriteToMemory(&data) &&
                data.GetLength() <= 256ull * 1024 * 1024 &&
                piMeshValidateSerialized(data.GetAddress(0), data.GetLength());
            if (valid) {
                fp->WriteUInt64(data.GetLength());
                fp->Write(data.GetAddress(0), data.GetLength());
            }
            data.End();
            return valid;
		}
	}

}
