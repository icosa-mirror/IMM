#pragma pack_matrix(row_major)

cbuffer FrameState : register(b0)
{
	struct
	{
		float       mTime;
		int         mFrame;
		int         dummy1;
		int         dummy2;
	}frame;
};


cbuffer LayersState : register(b3)
{
	struct
	{
		float4x4    mMatrix; // layer to viewer
		float       mScale;
		float       mOpacity;
		float       mUnused;
		float       mDrawInTime;
		float4      mDrawInParams;
		float4      mKeepAlive[2];        // note, can't pack in an array of float[8] due to granularity of GLSL array types
		uint        mID;
	}layer;
}

cbuffer DisplayState : register(b4)
{
	struct
	{
		struct
		{
			//float4x4      mMatrix_Prj;
			//float4x4      mMatrix_Cam;
			float4x4      mMatrix_CamPrj;
			//float4x4      mInvMatrix_Prj;
			//float4x4      mInvMatrix_Cam;
			//float4x4      mInvMatrix_CamPrj;
		}mEye[2];
		float2        mResolution;
	}display;
}

cbuffer PassState : register(b5)
{
	struct
	{
		int mID;
		int kk1;
		int kk2;
		int kk3;
	}eyepass;
}

struct vertex_format_t
{
	float   mPosX;
	float   mPosY;
	float   mPosZ;
	uint    mColAlp;
	uint    mDirInf;
	float   mTim;
};

//struct chunk_data_t
//{
//	uint vertexOffset;
//};
StructuredBuffer<vertex_format_t> data : register(t8);
//StructuredBuffer<chunk_data_t> cdata : register(t9);

cbuffer ChunkData : register(b9)
{
	struct
	{
		uint vertexOffset;
	}chunk_data[128];
}

float4 unpack4(uint d)
{
	return float4(uint4(d, d >> 8u, d >> 16u, d >> 24u) & 255u) / 255.0;
}

float3 unpack3(uint d)
{
	return float3(uint3(d, d >> 8u, d >> 16u) & 255u) / 255.0;
}


#if STEREOMODE==0
#define iid 0
#endif
#if STEREOMODE==1
// see Player::RenderStereoMultiPass::eyeID
//#define iid eyepass.mID
#define iid 0
#endif
#if STEREOMODE==2
#define iid instanceID
#endif


void main(uint vertexID : SV_VertexID,
          #if STEREOMODE==2	
	      uint instanceID : SV_InstanceID,
	      out uint oSlice : SV_RenderTargetArrayIndex,
          #endif
	      out float4 oColor    : V2P_COLOR,
	      out float4 oPosition : SV_Position, 
	      out uint   oInfo     : V2P_INFO )
{

	// Indexed draws already include the chunk base vertex. Positions are tessellated on the CPU.
	uint bid = vertexID;

	vertex_format_t vertex = data[bid];


	float3 inVertex = float3(vertex.mPosX, vertex.mPosY, vertex.mPosZ);
	uint   inInfo = vertex.mDirInf >> 24u;
	float  inTime = vertex.mTim;
	float4 inColAlpha = unpack4(vertex.mColAlp);
	float3 inOri = normalize(-1.0 + 2.0*unpack3(vertex.mDirInf));


	float3 pos = inVertex;
	float4 color = inColAlpha;




	// wiggle effect
	#if WIGGLE==1  
	{
		pos += layer.mKeepAlive[0].z*sin(layer.mKeepAlive[0].x*pos.yzx + layer.mKeepAlive[0].y*frame.mTime);
	}
	#endif


	float3 cpos = (mul(layer.mMatrix, float4(pos, 1.0))).xyz;

	oInfo = (inColAlpha.w>0.996) ? layer.mID : inInfo;

	// directional stroke
	float f = 1.0;
	if (((inInfo >> 7) & 1u) == 0u)
	{
		float3 wori = normalize(mul(layer.mMatrix,float4(inOri, 0.0)).xyz);
		//f = clamp( -wori.z, 0.0, 1.0 );
		f = clamp(dot(wori, normalize(cpos)), 0.0, 1.0);
		f = f*f;
	}

	#if COLOR_COMPRESSED==0 // linear. Colors are linear. But they are encoded in a sqrt() curve for more precission. Here we undo it
	oColor = float4(color.xyz*color.xyz, color.w*f*layer.mOpacity);
	#elif COLOR_COMPRESSED==1 // gamma. Colors are already in gamma comnverted in the CPU before upload to the GPU. Nothing to do
	oColor = float4(color.xyz, color.w*f*layer.mOpacity);
	#endif

	#if DRAWIN==1 
	float drawingT = 2.0*layer.mDrawInTime-inTime;						
	oColor.w *= smoothstep(layer.mDrawInParams.z, layer.mDrawInParams.z + layer.mDrawInParams.w, drawingT);
	#endif

	float3 bWPos = cpos;

	float4x4 mat = display.mEye[iid].mMatrix_CamPrj;

	// GL to DX conversion. If we remove this, then enable dx2gl() in player.cpp::iDisplayPreRenderLayer::289 and also enable GL.GetGPUProjectionMatrix() in C#
	//mat[1][1] = -mat[1][1];
	//mat[2][2] = -0.5*(1.0+mat[2][2]);
	//mat[2][3] = -0.5*mat[2][3];

	oPosition = mul(mat, float4(bWPos, 1.0));


	#if STEREOMODE==2
	oSlice = instanceID; // Each eye renders at full width into its array slice.
	#endif
}
