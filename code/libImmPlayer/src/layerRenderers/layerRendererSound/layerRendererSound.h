#pragma once


#include "libImmCore/src/libBasics/piArray.h"
#include "libImmCore/src/libBasics/piLog.h"
#include "libImmCore/src/libBasics/piVecTypes.h"
#include "libImmCore/src/libRender/piRenderer.h"
#include "../renderLayer.h"
#include "libImmImporter/src/document/layer.h"
#include "../../renderMode.h"

namespace ImmPlayer
{

	class LayerRendererSound : public LayerRenderer
	{
	public:
		LayerRendererSound();
		~LayerRendererSound();

		bool Init(ImmCore::piRenderer* renderer, ImmCore::piLog* log, ImmImporter::Drawing::ColorSpace colorSpace, bool frontIsCCW) override;
		void Deinit(ImmCore::piRenderer* renderer, ImmCore::piLog* log) override;

		bool LoadInCPU(ImmCore::piLog* log, ImmImporter::Layer* la) override;
		void UnloadInCPU(ImmCore::piLog* log, ImmImporter::Layer* la) override;

		bool LoadInGPU(ImmCore::piRenderer* renderer, ImmCore::piSoundEngine* sound, ImmCore::piLog* log, ImmImporter::Layer* la) override;
		bool UnloadInGPU(ImmCore::piRenderer* renderer, ImmCore::piSoundEngine* sound, ImmCore::piLog* log, ImmImporter::Layer* la) override;

		bool LoadInSPU(ImmCore::piSoundEngine* sound, ImmCore::piLog* log, ImmImporter::Layer* la) ;
		bool UnloadInSPU(ImmCore::piSoundEngine* sound, ImmImporter::Layer *la) ;

        // Borrowed source bytes must outlive the voice. These voices own no mSounds slot.
        struct PreviewVoice;
        PreviewVoice* CreatePreviewVoice(ImmCore::piSoundEngine* sound, ImmCore::piLog* log, ImmImporter::Layer* layer);
        void DestroyPreviewVoice(ImmCore::piSoundEngine* sound, PreviewVoice* voice);
        // Success means requests were accepted, not that asynchronous playback has completed.
        bool UpdatePreviewVoice(ImmCore::piSoundEngine* sound, ImmCore::piLog* log, ImmImporter::Layer* layer,
                                PreviewVoice* voice, float masterVolume, bool audible, bool restart,
                                const ImmCore::trans3d* layerToWorld = nullptr);
        // The caller must suppress normal GlobalWork while holding this pause barrier.
        bool PauseMainVoice(ImmCore::piSoundEngine* sound, ImmImporter::Layer* layer);

		void GlobalWork(ImmCore::piRenderer* renderer, ImmCore::piSoundEngine* sound, ImmCore::piLog* log, ImmImporter::Layer* la, float masterVolume) override;
		void PrepareForDisplay(StereoMode stereoMode) override;
		void DisplayPreRender(ImmCore::piRenderer* renderer, ImmCore::piSoundEngine* sound, ImmCore::piLog* log, ImmImporter::Layer* la, const ImmCore::frustum3& frus, const ImmCore::trans3d & layerToViewer, float opacity) override;
		void DisplayRender(ImmCore::piRenderer* renderer, ImmCore::piLog* log, ImmCore::piBuffer layerStateShaderConstans, int capDelta) override;

		const DrawCallInfo & GetDrawCallInfo() override { return mDrawCallInfo; };

	private:
		ImmCore::piArray mSounds;
		DrawCallInfo mDrawCallInfo {};

	};

}
