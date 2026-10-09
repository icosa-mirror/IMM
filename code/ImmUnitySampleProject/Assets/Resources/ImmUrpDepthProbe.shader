Shader "IMM/URPDepthProbe"
{
    Properties { _ZWrite ("Depth write", Float) = 1 }
    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" "RenderType"="Opaque" "Queue"="Geometry" }
        HLSLINCLUDE
        #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
        struct Attributes
        {
            float3 positionOS : POSITION;
            UNITY_VERTEX_INPUT_INSTANCE_ID
        };
        struct Varyings
        {
            float4 positionCS : SV_POSITION;
            UNITY_VERTEX_OUTPUT_STEREO
        };
        Varyings Vert(Attributes input)
        {
            Varyings output = (Varyings)0;
            UNITY_SETUP_INSTANCE_ID(input);
            UNITY_INITIALIZE_VERTEX_OUTPUT_STEREO(output);
            output.positionCS = TransformObjectToHClip(input.positionOS);
            return output;
        }
        half4 Frag() : SV_Target { return half4(0, 0, 0, 1); }
        ENDHLSL
        Pass
        {
            Tags { "LightMode"="UniversalForward" }
            Cull Off
            ZWrite [_ZWrite]
            ZTest LEqual
            Blend One Zero
            HLSLPROGRAM
            #pragma target 3.5
            #pragma multi_compile_instancing
            #pragma vertex Vert
            #pragma fragment Frag
            ENDHLSL
        }
        Pass
        {
            Tags { "LightMode"="DepthOnly" }
            Cull Off
            ZWrite On
            ColorMask 0
            HLSLPROGRAM
            #pragma target 3.5
            #pragma multi_compile_instancing
            #pragma vertex Vert
            #pragma fragment Frag
            ENDHLSL
        }
    }
}
