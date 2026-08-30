#include <math.h>
#include "libImmCore/src/libBasics/piVecTypes.h"
#include "libImmCore/src/libCompression/basic/piQuantize.h"
#include "element.h"
#include "paintGeometry.h"
using namespace ImmCore;

namespace ImmImporter
{

Element::Element() {}

Element::~Element() {}

// static
const int Element::kSectionsLUT[] = { 2, 2, 7, 7, 4 };

void Element::Make( int num, BrushSectionType bid, VisibilityType mode)
{
	mBrush = bid;
	mVisibleMode = mode;

	mNumPoints = num;

	mBBox = bound3(1e20f);
}

float Element::GetWidth(int vertex, float biggestStroke) const
{
    return piQuantize::ibits15(mPoints[vertex].mWid) * (1.7f*biggestStroke);
}

void Element::Compute(float biggestStroke)
{
    const uint64_t nump = mNumPoints;
	if (nump < 1)
	{
		mBBox = bound3(1e20f);
		return;
	}

    Point* p = mPoints;

    const float wid0 = GetWidth(0, biggestStroke);
    mBBox = bound3(p[0].mPos - vec3(wid0),
		           p[0].mPos + vec3(wid0));
    for (uint64_t i = 1; i < nump; i++)
    {
        const float widi = GetWidth(static_cast<int>(i), biggestStroke);
        mBBox = include(mBBox, p[i].mPos - vec3(widi));
		mBBox = include(mBBox, p[i].mPos + vec3(widi));
	}


	float s = 0.0f;
	float numpf = float(nump);
	for (uint64_t i = 0; i < nump; i++)
	{
		p[i].mLen = s;
		if (i < (nump - 1))
			s += length(p[i].mPos - p[i + 1].mPos);

		//p[i].mTim = timeOffset + float(i) / 90.0f; // per curve time should come from binary file // IQIQ

		// HACK: make sure that the timing across a curve is normalized to be between 0 and 1.  timeOffset
		// is provided with the range [0., 1.]
		float timeOffset = 0.0f;
		p[i].mTim = clamp01( 0.5f * (timeOffset + (float(i) / numpf)));

		// reduce unintentional dithering due to old UI problems with opacity control
		//if (p[i].mTra > 0.95f) p[i].mTra = 1.0f;
	}

	mLength = s;

}


//====================================================================================================================================================

vec3 Element::ComputeTangent(int i) const
{
	return PaintGeometry::ComputeTangent(mPoints, mNumPoints, static_cast<uint32_t>(i));
}

void Element::ComputeBasis(int i, vec3 * resTan, vec3 *resU, vec3 * resV) const
{
    PaintGeometry::ComputeBasis(mPoints, mNumPoints, static_cast<uint32_t>(i), resTan, resU, resV);
}


const int Element::GetNumPolygons() const
{
    static const uint32_t kSegments[] = {1, 1, 7, 7, 4};
    const uint32_t numSegments = kSegments[static_cast<uint32_t>(mBrush)];
    return numSegments * (mNumPoints - 1); // GetNumPolygons() * 2 for num triangles
}

}
