#pragma once
#include <vector>

#include "libImmCore/src/libBasics/piPool.h"

#include "../layerRendererPaint.h"

namespace ImmPlayer
{
	class LayerRendererPaintStatic : public LayerRendererPaint
	{
	public:
		LayerRendererPaintStatic();
		~LayerRendererPaintStatic();

		bool Init(ImmCore::piRenderer* renderer, ImmCore::piLog* log, ImmImporter::Drawing::ColorSpace colorSpace, bool frontIsCCW) override;
		void Deinit(ImmCore::piRenderer* renderer, ImmCore::piLog* log) override;

		bool LoadInCPU(ImmCore::piLog* log, ImmImporter::Layer* la) override;
		void UnloadInCPU(ImmCore::piLog* log, ImmImporter::Layer* la) override;
        bool IsLoadedInGPU(ImmImporter::Layer* la) override;
		bool LoadInGPU(ImmCore::piRenderer* renderer, ImmCore::piSoundEngine* sound, ImmCore::piLog* log, ImmImporter::Layer* la) override;
		bool UnloadInGPU(ImmCore::piRenderer* renderer, ImmCore::piSoundEngine* sound, ImmCore::piLog* log, ImmImporter::Layer* la) override;
        bool UnloadInGPU(ImmCore::piRenderer *renderer, ImmCore::piSoundEngine *sound, ImmCore::piLog *log, ImmImporter::Layer *la, unsigned int drawingID) override;
		void GlobalWork(ImmCore::piRenderer* renderer, ImmCore::piSoundEngine* sound, ImmCore::piLog* log, ImmImporter::Layer* la, float masterVolume) override;

		bool PrepareDrawingReplacementInCPU(ImmImporter::Drawing * replacement,
			uint64_t * tokenOut, ImmCore::piLog * log) override;
		bool PrepareDrawingReplacementInGPU(ImmCore::piRenderer * renderer,
			uint64_t token, ImmCore::piLog * log) override;
		bool PresentDrawingReplacement(ImmImporter::Drawing * active,
			ImmImporter::Drawing * replacement, uint64_t token, uint64_t revision,
			ImmCore::piLog * log) override;
		void CancelDrawingReplacement(ImmCore::piRenderer * renderer,
			uint64_t token, ImmCore::piLog * log) override;
		void AdvanceDrawingRetirement(ImmCore::piRenderer * renderer, ImmCore::piLog * log) override;

		void PrepareForDisplay(StereoMode stereoMode) override;
		void DisplayPreRender(ImmCore::piRenderer* renderer, ImmCore::piSoundEngine* sound, ImmCore::piLog* log, ImmImporter::Layer* la, const ImmCore::frustum3& frus, const ImmCore::trans3d & layerToViewer, float opacity) override;
		void DisplayRender(ImmCore::piRenderer* renderer, ImmCore::piLog* log, ImmCore::piBuffer layerStateShaderConstans, int capDelta) override;

		const DrawCallInfo & GetDrawCallInfo() override { return mDrawCallInfo; }

		// for loading data
        static constexpr int kNumChunkTypes = static_cast<int>(ImmImporter::Element::BrushSectionType::Count);

	private:
        ImmCore::piPool      mLayerInfo;   // per camera pass, partially
		StereoMode  mStereoMode;  // per camera pass
		ImmCore::piArray     mVisibleLayerInfos;

        ImmImporter::Drawing::ColorSpace mColorSpace;

#if defined(ANDROID)
		static const int kNumShaders = 5 * 3 * 2; // 5 brushes, 3 stereo modes, 2 wiggle = 30
#else
		static const int kNumShaders = 5 * 3 * 2 * 2; // 5 brushes, 3 stereo modes, 2 wiggle, 2 drawin = 60
#endif
		ImmCore::piShader mShader[kNumShaders];
		ImmCore::piBuffer mChunkData;
		ImmCore::piTexture mBlueNoise;
		ImmCore::piRasterState mRasterState[4];
        uint64_t mCapLayersToRender;

		DrawCallInfo mDrawCallInfo {};

		struct RetiredDrawing
		{
			ImmImporter::Drawing * mDrawing = nullptr;
			uint64_t mToken = 0;
			uint64_t mRetireAfterFrame = 0;
		};
		uint64_t mRetirementFrame = 0;
		uint64_t mPresentationSample = 0;
		bool mTracePresentationFrames = false;
		std::vector<RetiredDrawing> mRetiredDrawings;
		static constexpr uint64_t kRetirementFramesInFlight = 3;
	};

}
