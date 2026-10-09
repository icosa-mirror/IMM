Shader "IMM/URPSkyboxProbe"
{
    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" "Queue"="Background" "RenderType"="Background" "PreviewType"="Skybox" }
        Cull Off ZWrite Off ZTest LEqual
        Pass
        {
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment Frag
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
            float4 Vert(float3 positionOS : POSITION) : SV_POSITION
            {
                float4 positionCS = TransformObjectToHClip(positionOS);
                positionCS.z = UNITY_RAW_FAR_CLIP_VALUE * positionCS.w;
                return positionCS;
            }
            half4 Frag() : SV_Target { return half4(0, 0, 1, 1); }
            ENDHLSL
        }
    }
}
