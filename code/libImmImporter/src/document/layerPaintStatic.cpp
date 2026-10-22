#include <new>
#include "libImmCore/src/libBasics/piDebug.h"
#include "libImmCore/src/libBasics/piFile.h"
#include "libImmCore/src/libBasics/piStr.h"

#include "layerPaintStatic.h"

namespace ImmImporter
{

	bool LayerPaintStatic::Init(int numDrawings, int numFrames, int maxRepeatCount, uint32_t frameRate, unsigned int version)
	{
        if (numDrawings < 0 || numFrames < 0)
            return false;
		// When adding properties here, make sure to add them to Copy and De/Serialize
		mIsPlaying = false;		
		mTime = 0;
		mFrameRate = frameRate;
		mCurrentFrame = 0;
        mOffset = 0;
		mMaxRepeatCount = maxRepeatCount;
        mEvaluatedMaxRepeatCount = maxRepeatCount;
        mVersion = version;

		mDrawings.clear();
		mDrawings.reserve(static_cast<size_t>(numDrawings));
		for (int i = 0; i < numDrawings; i++)
		{
			std::unique_ptr<DrawingStatic> drawing(new (std::nothrow) DrawingStatic());
			if (!drawing)
				return false;
			mDrawings.push_back(std::move(drawing));
		}

        try
        {
            mFrames.assign(static_cast<size_t>(numFrames), 0);
        }
        catch (const std::bad_alloc &)
        {
            return false;
        }
		return true;
	}

	void LayerPaintStatic::Deinit(void)
	{
		for (uint64_t i = 0, max = mDrawings.size(); i < max; i++)
		{
			Drawing* drawing = mDrawings[i].get();
			drawing->Deinit();
		}
		mDrawings.clear();
        std::vector<uint32_t>().swap(mFrames);
	}

	const bound3& LayerPaintStatic::GetBBox(void) const
	{
        const Drawing * drawing = GetCurrentDrawing();
        static const bound3 empty(1e20f);
        return drawing != nullptr ? drawing->GetBBox() : empty;
	}

	const bool LayerPaintStatic::HasBBox(void) const
	{
        const Drawing * drawing = GetCurrentDrawing();
        return drawing != nullptr && drawing->GetNumStrokes() > 0;
	}

	unsigned int LayerPaintStatic::GetNumDrawings(void) const
	{
		return static_cast<unsigned int>(mDrawings.size());
	}

	unsigned int LayerPaintStatic::GetNumFrames(void) const
	{
		return static_cast<unsigned int>(mFrames.size());
	}

    bool LayerPaintStatic::GetPlaying(void) const
    {
        return mIsPlaying;
    }

	void LayerPaintStatic::SetTime(piTick time)
	{
		if (!mIsPlaying) return;
        SelectFrameAtTime(time);
    }

    void LayerPaintStatic::SelectFrameAtTime(piTick time) noexcept
    {
        mTime = time;
        iSelectFrame();
    }

    void LayerPaintStatic::iSelectFrame(void) noexcept
    {
        const uint64_t numFrames = mFrames.size();
        if (numFrames <= 1)
        {
            mCurrentFrame = 0;
            return;
        }
        const uint64_t framesSinceStart = piTick::ToFramesFloor(mTime, mFrameRate) + mOffset;
        const uint64_t framesSinceStartWrapped = framesSinceStart % numFrames;
        const uint64_t numRepeatsSinceStart = framesSinceStart / numFrames;

        if (mEvaluatedMaxRepeatCount != 0 && numRepeatsSinceStart >= mEvaluatedMaxRepeatCount)
        {
            mCurrentFrame = static_cast<unsigned int>(numFrames - 1);
        }
        else
        {
            mCurrentFrame = static_cast<uint32_t>(framesSinceStartWrapped);
        }
	}

    void LayerPaintStatic::SetOffset(uint32_t offsetFrames)
    {
        mOffset = offsetFrames;
    }

	void LayerPaintStatic::SetPlaying(bool playing)
	{
		mIsPlaying = playing;
	}

    void LayerPaintStatic::SetMaxRepeatCount(uint32_t maxCount)
    {
        mMaxRepeatCount = maxCount;
        mEvaluatedMaxRepeatCount = maxCount;
    }

    void LayerPaintStatic::SetEvaluatedMaxRepeatCount(uint32_t count)
    {
        mEvaluatedMaxRepeatCount = count;
    }


	const Drawing * LayerPaintStatic::GetCurrentDrawing(void) const
	{
        if (mFrames.empty() || mCurrentFrame >= mFrames.size())
            return nullptr;
        const uint32_t drawing = mFrames[mCurrentFrame];
        return drawing < mDrawings.size() ? mDrawings[drawing].get() : nullptr;
	}

    Drawing * LayerPaintStatic::NewDrawing(void)
    {
        return nullptr; // unused
    }

    Drawing * LayerPaintStatic::AddDrawing(void)
    {
        // Existing drawings keep their indices: the frame buffer stores indices, and the GPU
        // pool is addressed by each drawing's own gpu id, so appending invalidates neither.
		std::unique_ptr<DrawingStatic> drawing(new (std::nothrow) DrawingStatic());
		if (!drawing || !drawing->Init(1))
		{
			return nullptr;
		}

		drawing->SetGpuId(-1);
		drawing->SetLoaded(false);
		DrawingStatic * result = drawing.get();
        try
        {
            mDrawings.push_back(std::move(drawing));
        }
        catch (const std::bad_alloc &)
        {
            drawing->Deinit();
            return nullptr;
        }
        return result;
	}

	bool LayerPaintStatic::RemoveLastDrawing(Drawing * expected)
	{
		if (mDrawings.empty() || mDrawings.back().get() != expected)
			return false;
		mDrawings.back()->Deinit();
		mDrawings.pop_back();
		return true;
	}

    void LayerPaintStatic::ReleaseDrawingsForRetirement(void) noexcept
    {
        for (auto & drawing : mDrawings)
            drawing.release();
        mDrawings.clear();
    }

    std::unique_ptr<LayerPaintStatic::PreparedFrameTable> LayerPaintStatic::PrepareFrameTable(
        std::vector<uint32_t> indices, uint32_t resultingDrawingCount)
    {
        if (indices.size() > UINT32_MAX)
            return nullptr;
        for (const uint32_t index : indices)
            if (index >= resultingDrawingCount)
                return nullptr;
        std::unique_ptr<PreparedFrameTable> prepared(new (std::nothrow) PreparedFrameTable());
        if (prepared != nullptr)
            prepared->mFrames.swap(indices);
        return prepared;
    }

    bool LayerPaintStatic::PublishFrameTable(PreparedFrameTable & table) noexcept
    {
        if (table.mFrames.size() > UINT32_MAX)
            return false;
        for (const uint32_t index : table.mFrames)
            if (index >= mDrawings.size())
                return false;
        mFrames.swap(table.mFrames);
        // Reselect even while stopped: the retained local time defines the held
        // drawing, and the old frame index may exceed the replacement length.
        iSelectFrame();
        return true;
    }

	Drawing * LayerPaintStatic::ExtractDrawing(uint32_t drawingIndex)
	{
		if (drawingIndex >= mDrawings.size())
			return nullptr;
		uint32_t * frames = GetFrameBuffer();
		if (GetNumFrames() > 0 && frames == nullptr)
			return nullptr;
		for (uint32_t frameIndex = 0; frameIndex < GetNumFrames(); frameIndex++)
		{
			if (frames[frameIndex] == drawingIndex)
				return nullptr;
		}

		DrawingStatic * removed = mDrawings[drawingIndex].release();
		mDrawings.erase(mDrawings.begin() + drawingIndex);
		for (uint32_t frameIndex = 0; frameIndex < GetNumFrames(); frameIndex++)
		{
			if (frames[frameIndex] > drawingIndex)
				frames[frameIndex]--;
		}
		return removed;
	}

	Drawing * LayerPaintStatic::GetDrawing(int drawing) const
	{
		return mDrawings[drawing].get();
	}

	uint32_t * LayerPaintStatic::GetFrameBuffer(void)
	{
		return mFrames.empty() ? nullptr : mFrames.data();
	}

    unsigned int LayerPaintStatic::GetVersion(void) const
    {
        return mVersion;
    }

    unsigned int LayerPaintStatic::GetMaxRepeatCount(void) const
    {
        return mMaxRepeatCount;
    }

    unsigned int LayerPaintStatic::GetFrameRate(void) const
    {
        return mFrameRate;
    }

}
