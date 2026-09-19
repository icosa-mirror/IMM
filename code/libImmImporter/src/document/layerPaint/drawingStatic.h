#pragma once

#include "libImmCore/src/libBasics/piString.h"

#include "element.h"
#include "drawing.h"

// 0: 36 bytes (PC/Windows)
// 1: 28 bytes (Quest/Android)
#if defined(ANDROID)
#define ST_VERTEX_FORMAT 1
#else
#define ST_VERTEX_FORMAT 0 // set 1 to debug Android issues on Windows
#endif

using namespace ImmCore;

namespace ImmImporter
{
	class DrawingStatic : public Drawing
	{
	public:
		DrawingStatic() = default;
		~DrawingStatic() override = default;

		bool Init(uint32_t numElements) override;
		void Deinit(void) override;

		bool  StartAdding(float biggestStroke) override; // for the deserializer
		bool  Add(const Element * ele, ColorSpace colorSpace, bool flipped) override;
		void  StopAdding(void) override;

		// Live editing: rebuild this drawing's vertices, indices and chunks from new element
		// data. The previous geometry is dropped and the new elements go through the same
		// builder the importer uses, so the result is what an import of the same points would
		// have produced. The caller keeps ownership of the elements.
		bool  ReplaceGeometry(const Element * elements, int numElements, ColorSpace colorSpace, bool flipped, float biggestStroke) override;
		bool  SwapGeometry(Drawing * replacement) override;

		void     SetGpuId(int id) override;
		int      GetGpuId(void) const override;

        void  SetLoaded(bool loaded) override;
        bool  GetLoaded(void) const override;

        void  SetFileOffset(uint64_t offset) override;
        uint64_t GetFileOffset(void) const override;

        // GPU vertex format
#if ST_VERTEX_FORMAT == 1
        // 28 (27 really, 1 unused)
		typedef struct
		{
			vec3      mPos;    // 12  ----> can be 6 bytes is we use "half[3]". chunk data then should have bbox corner
			uint32_t  mWidInfo;//  2 bytes width, 1 byte info, 1 byte unused
			uint8_t   mCol[3]; //  3
			uint8_t   mAlp;    //  1
			uint32_t  mAxU;    //  4
			uint32_t  mAxV;    //  4
		}MyVertexFormat;
#else
		// 36 (34 really, 2 unused)
		typedef struct
		{
			vec3     mPos;    // 12  ----> can be 6 bytes is we use "half[3]". chunk data then should have bbox corner
			uint32_t mWid;    //  4  ----> 2 bytes are unused!
			uint8_t  mCol[3]; //  3
			uint8_t  mAlp;    //  1
			uint8_t  mDir[3]; //  3
			uint8_t  mInfo;   //  1
			uint32_t mAxU;    //  4
			uint32_t mAxV;    //  4
			float    mTim;    //  4   ----> not needed, we can recover time from the chunk data and vertex id!
		}MyVertexFormat;
#endif
		struct Geometry
		{
			struct Chunk
			{
				uint32_t      mNumVertices;
				uint32_t      mNumIndices;
				uint32_t      mNumPolygons;
                bound3        mBBox;
				uint32_t      mType;
				uint32_t      mVertexOffset;
				uint32_t      mPointsOffset;
				uint32_t      mIndexOffset;
				uint64_t      mChunkIndex;
                float         mBiggestStroke;
			};

			struct Data
			{
				piTArray<MyVertexFormat> mPoints;
				piTArray<uint16_t>       mIndices;
				piTArray<Chunk>          mChunks;
			}mBuffers[5];

			uint32_t mNumStrokes;
            float    mBiggestStroke;
		};


		const bound3& GetBBox(void) const override;
		const uint32_t GetNumStrokes(void) const override;
		const uint32_t GetNumGeometryChunks(uint32_t type) const override;
		const Geometry::Chunk * GetGeometryChunk(uint32_t id, uint32_t type) const;
		const Geometry *GetGeometry(void) const;

	private:
		void iAppendChunk(const uint32_t vbase, const uint32_t ibase, const uint32_t pbase, const uint32_t numPoints, uint32_t numIndices, uint32_t numPolygons, const ImmCore::bound3 & bbox, const uint32_t type, const float biggestStroke) override;

	private:
        bound3   mBBox;
		int      mGpuId;
		Geometry mGeometry;

	private:
		struct Builder
		{
            uint32_t mPointsBase;
            uint32_t mVerticesBase;
            uint32_t mIndexBase;

            uint32_t mNumPoints;
            uint32_t mNumVertices;
			uint32_t mNumIndices;
			uint32_t mNumPolygons;
            ImmCore::bound3 lbox;
		}mBuilders[5];

        uint64_t mFileOffset;
        bool mLoaded;
	};
}
