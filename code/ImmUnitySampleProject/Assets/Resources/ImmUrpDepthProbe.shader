Shader "IMM/URPDepthProbe"
{
    Properties { _ZWrite ("Depth write", Float) = 1 }
    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" "RenderType"="Opaque" "Queue"="Geometry" }
        Pass
        {
            Tags { "LightMode"="UniversalForward" }
            Cull Off
            ZWrite [_ZWrite]
            ZTest LEqual
            Blend One Zero
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment Frag
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
            float4 Vert(float3 positionOS : POSITION) : SV_POSITION
            {
                return TransformObjectToHClip(positionOS);
            }
            half4 Frag() : SV_Target { return half4(0, 0, 0, 1); }
            ENDHLSL
        }
        Pass
        {
            Tags { "LightMode"="DepthOnly" }
            Cull Off
            ZWrite On
            ColorMask 0
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment Frag
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
            float4 Vert(float3 positionOS : POSITION) : SV_POSITION
            {
                return TransformObjectToHClip(positionOS);
            }
            half4 Frag() : SV_Target { return 0; }
            ENDHLSL
        }
    }
}
