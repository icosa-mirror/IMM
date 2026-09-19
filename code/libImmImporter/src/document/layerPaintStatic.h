#pragma once

#include "libImmCore/src/libBasics/piVecTypes.h"
#include "libImmCore/src/libBasics/piTick.h"
#include "libImmCore/src/libBasics/piArray.h"
#include "libImmCore/src/libBasics/piString.h"
#include "libImmCore/src/libBasics/piLog.h"

#include "layerPaint/element.h"
#include "layerPaint/drawing.h"

#include "layerPaint.h"

#include "layerPaint/drawingStatic.h"

#include <vector>

namespace ImmImporter
{

	class LayerPaintStatic : public LayerPaint
	{
	public:
		LayerPaintStatic() = default;
		~LayerPaintStatic() override = default;

		bool Init(int numDrawings, int numFrames, int maxRepeatCount, uint32_t frameRate, unsigned int version) override;
		void Deinit(void) override;

		// rendering controlls. Return proper data based on mCurrentFrame
		const ImmCore::bound3& GetBBox(void) const override;
		const bool HasBBox(void) const override;
		const Drawing * GetCurrentDrawing(void) const override;
        bool GetPlaying(void) const override;

		// playback controlls
		void SetTime(ImmCore::piTick time) override; // sets current time during playback
		void SetPlaying(bool playing) override;
        void SetOffset(uint32_t offsetFrames) override;
        void SetMaxRepeatCount(uint32_t count) override;


		// for deserializing... (ugly!)
		unsigned int GetNumDrawings(void) const override;
		unsigned int GetNumFrames(void) const override;
		uint32_t *   GetFrameBuffer(void) override;
        unsigned int GetVersion(void) const override;
        unsigned int GetMaxRepeatCount(void) const override;
        unsigned int GetFrameRate(void) const override;

        // for deserializing... (ugly!)
        Drawing * NewDrawing(void) override;
		// for gpu unloading... (ugly!)
		Drawing * GetDrawing(int drawing) const override;

		// Live editing: append a drawing. Existing drawings keep their indices, so frame
		// mappings pointing at them stay valid.
		Drawing * AddDrawing(void) override;


	private:
		// static data
		std::vector<DrawingStatic> mDrawings;
		ImmCore::piArray mFrames;
		uint32_t mFrameRate;	 // in FPS
		uint32_t mMaxRepeatCount; // 0 repeats forever
        uint32_t mVersion;

		// playback state
		bool mIsPlaying;		
        ImmCore::piTick mTime;	
        uint32_t mOffset;        // start offset in frames
		uint32_t mCurrentFrame; // derived from mCurrentTime, and always in synch

	};

}
