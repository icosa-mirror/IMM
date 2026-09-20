#pragma once

#include "../renderLayer.h"
#include "libImmImporter/src/document/layerPaint/element.h"

namespace ImmPlayer
{
	class LayerRendererPaint : public LayerRenderer
	{
	public:
		LayerRendererPaint() = default;

		virtual ~LayerRendererPaint() = default;

		virtual bool Init(ImmCore::piRenderer *renderer, ImmCore::piLog *log, ImmImporter::Drawing::ColorSpace colorSpace, bool frontIsCCW) override = 0;

		virtual void Deinit(ImmCore::piRenderer *renderer, ImmCore::piLog *log) override = 0;

		virtual bool LoadInCPU(ImmCore::piLog *log, ImmImporter::Layer *la) override = 0;

		virtual void UnloadInCPU(ImmCore::piLog *log, ImmImporter::Layer *la) override = 0;

		virtual bool
		LoadInGPU(ImmCore::piRenderer *renderer, ImmCore::piSoundEngine *sound, ImmCore::piLog *log, ImmImporter::Layer *la) override = 0;

        virtual bool IsLoadedInGPU(ImmImporter::Layer* la) = 0;

		virtual bool
		UnloadInGPU(ImmCore::piRenderer *renderer, ImmCore::piSoundEngine *sound, ImmCore::piLog *log, ImmImporter::Layer *la) override = 0;

        virtual bool UnloadInGPU(ImmCore::piRenderer *renderer, ImmCore::piSoundEngine *sound, ImmCore::piLog *log, ImmImporter::Layer *la, unsigned int drawingID) = 0;

		virtual void GlobalWork(ImmCore::piRenderer *renderer, ImmCore::piSoundEngine *sound, ImmCore::piLog *log, ImmImporter::Layer *la,
								float masterVolume) override = 0;

		// Live editing replacement lifecycle. CPU and GPU preparation allocate a separate
		// renderer slot. PresentDrawingReplacement swaps geometry and slot identity together;
		// on success the renderer owns replacement, which then contains the retired geometry.
		virtual bool PrepareDrawingReplacementInCPU(ImmImporter::Drawing * replacement,
			uint64_t * tokenOut, ImmCore::piLog * log)
		{
			(void)replacement; (void)tokenOut; (void)log;
			return false;
		}

		virtual bool PrepareDrawingReplacementInGPU(ImmCore::piRenderer * renderer,
			uint64_t token, ImmCore::piLog * log)
		{
			(void)renderer; (void)token; (void)log;
			return false;
		}

		virtual bool PresentDrawingReplacement(ImmImporter::Drawing * active,
			ImmImporter::Drawing * replacement, uint64_t token, uint64_t revision,
			ImmCore::piLog * log)
		{
			(void)active; (void)replacement; (void)token; (void)revision; (void)log;
			return false;
		}

		virtual bool PresentDrawingCreation(ImmImporter::Drawing * created,
			ImmImporter::Drawing * prepared, uint64_t token, uint64_t revision,
			ImmCore::piLog * log)
		{
			(void)created; (void)prepared; (void)token; (void)revision; (void)log;
			return false;
		}

		virtual void CancelDrawingReplacement(ImmCore::piRenderer * renderer,
			uint64_t token, ImmCore::piLog * log)
		{
			(void)renderer; (void)token; (void)log;
		}

		virtual void AdvanceDrawingRetirement(ImmCore::piRenderer * renderer, ImmCore::piLog * log)
		{
			(void)renderer; (void)log;
		}

		virtual void PrepareForDisplay(StereoMode stereoMode) override = 0;

		virtual void
		DisplayPreRender(ImmCore::piRenderer *renderer, ImmCore::piSoundEngine *sound, ImmCore::piLog *log, ImmImporter::Layer *la,
						 const ImmCore::frustum3 &frus, const ImmCore::trans3d &layerToViewer,
						 float opacity) override = 0;

		virtual void
		DisplayRender(ImmCore::piRenderer *renderer, ImmCore::piLog *log, ImmCore::piBuffer layerStateShaderConstans,
					  int capDelta) override = 0;

		virtual const DrawCallInfo &GetDrawCallInfo () override = 0;

		// for loading data
		static constexpr int kNumChunkTypes = static_cast<int>(ImmImporter::Element::BrushSectionType::Count);
	};
}
