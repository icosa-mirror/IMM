#pragma once

#include "layerPaint/element.h"
#include "layerPaint/drawing.h"
#include "libImmCore/src/libBasics/piTick.h"
namespace ImmImporter
{

	class LayerPaint
	{
	public:
		LayerPaint() = default;
		virtual ~LayerPaint() = default;

		virtual bool Init(int numDrawings, int numFrames, int maxRepeatCount, uint32_t frameRate, unsigned int version) = 0;
		virtual void Deinit(void) = 0;

		// rendering controlls. Return proper data based on mCurrentFrame
		virtual const ImmCore::bound3& GetBBox(void) const = 0;
		virtual const bool HasBBox(void) const = 0;
		virtual const Drawing * GetCurrentDrawing(void) const = 0;
        virtual bool GetPlaying(void) const = 0;

		// playback controlls
		virtual void SetTime(ImmCore::piTick time) = 0; // sets current time during playback
		virtual void SetPlaying(bool playing) = 0;
        virtual void SetOffset(uint32_t offsetFrames) = 0;
        virtual void SetMaxRepeatCount(uint32_t count) = 0;

		// for deserializing... (ugly!)
        virtual unsigned int GetMaxRepeatCount(void) const = 0;
		virtual unsigned int GetNumDrawings(void) const = 0;
		virtual unsigned int GetNumFrames(void) const = 0;
        virtual unsigned int GetFrameRate(void) const = 0;
		virtual uint32_t *   GetFrameBuffer(void) = 0;
        virtual unsigned int GetVersion(void) const = 0;

        // for deserializing... (ugly!)
        virtual Drawing * NewDrawing(void) = 0;

		// Live editing: append a drawing with no geometry and no GPU resources yet. The caller
		// fills it through the authoring presentation boundary. Implementations must keep the
		// addresses of existing drawings stable when appending.
        virtual Drawing * AddDrawing(void) { return nullptr; }
		virtual bool RemoveLastDrawing(Drawing * expected) { (void)expected; return false; }
		// Removes an unreferenced drawing and transfers ownership to the caller. Implementations
		// compact their private frame indices while preserving every remaining drawing address.
		virtual Drawing * ExtractDrawing(uint32_t drawingIndex) { (void)drawingIndex; return nullptr; }

		// for gpu unloading... (ugly!)
		virtual Drawing * GetDrawing(int drawing) const = 0;
	};
}
