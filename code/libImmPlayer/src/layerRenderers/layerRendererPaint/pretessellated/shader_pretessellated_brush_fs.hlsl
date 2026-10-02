#define CUSTOM_ALPHA_TO_COVERAGE 1
#define MSAASampleCount GetRenderTargetSampleCount()

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

float4 main(
			#if STEREOMODE==2	
	        uint slice : SV_RenderTargetArrayIndex,
            #endif
	        float4 color : V2P_COLOR,
	        float4 fragCoord : SV_POSITION, 
			#if CUSTOM_ALPHA_TO_COVERAGE==1
	        uint primitiveID : V2P_INFO,
	        out uint coverage : SV_COVERAGE
			#else
	        uint primitiveID : V2P_INFO
            #endif
           ) : SV_TARGET
{

	float2 q = fragCoord.xy + float2(0,float((frame.mFrame) & 7)*11.0);

	float ran = frac( 52.9829189*frac(dot(q,float2(0.06711056,0.00583715))));

	float al = clamp(color.a + 0.99*(ran - 0.5) / float(MSAASampleCount), 0.0, 1.0); // 0.99 is to make sure the dithering never makes the alpha leak to the previour or the next bucket

	#if CUSTOM_ALPHA_TO_COVERAGE==1
	color.a = 1.0;
	uint fullMask = (1u << MSAASampleCount) - 1u;
    uint mask = ((fullMask << MSAASampleCount) >> uint(al*float(MSAASampleCount) + 0.5)) & fullMask;
	uint shift = (uint(ran*float(MSAASampleCount - 1u)) + primitiveID) & (MSAASampleCount - 1u); // rotate coverage within the attachment sample count
	coverage = (((mask << MSAASampleCount) | mask) >> shift) & fullMask;
	#else
	color.a = al;
	#endif
/*
#if STEREOMODE==0	
	color.xyz *= float3(1.0, 0.0, 0.0);
#endif
#if STEREOMODE==1
	color.xyz *= float3(0.0, 1.0, 0.0);
#endif
#if STEREOMODE==2	
	color.xyz *= float3(0.0, 0.0, 1.0);
#endif
*/
	return color;
}