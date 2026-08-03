Shader "IMM/VulkanDepthComposite"
{
    Properties
    {
        _MainTex ("Unity Color", 2D) = "black" {}
        _ImmColorTex ("IMM Color", 2D) = "black" {}
        _ImmDepthTex ("IMM Depth", 2D) = "black" {}
    }

    SubShader
    {
        Cull Off
        ZWrite Off
        ZTest Always

        Pass
        {
            CGPROGRAM
            #pragma vertex vert_img
            #pragma fragment frag
            #include "UnityCG.cginc"

            sampler2D _MainTex;
            sampler2D _ImmColorTex;
            UNITY_DECLARE_DEPTH_TEXTURE(_CameraDepthTexture);
            UNITY_DECLARE_DEPTH_TEXTURE(_ImmDepthTex);

            fixed4 frag(v2f_img input) : SV_Target
            {
                float2 uv = input.uv;
                fixed4 unityColor = tex2D(_MainTex, uv);
                fixed4 immColor = tex2D(_ImmColorTex, uv);
                float unityDepth = SAMPLE_DEPTH_TEXTURE(_CameraDepthTexture, uv);
                float immDepth = SAMPLE_DEPTH_TEXTURE(_ImmDepthTex, uv);

                #if defined(UNITY_REVERSED_Z)
                    bool unityIsNearer = unityDepth > immDepth + 1.0e-6;
                #else
                    bool unityIsNearer = unityDepth < immDepth - 1.0e-6;
                #endif

                if (unityIsNearer)
                    return unityColor;
                return lerp(unityColor, immColor, immColor.a);
            }
            ENDCG
        }

        // Validation diagnostic: R=Unity depth, G=IMM depth, B=vertically
        // flipped IMM depth. Reverse-Z values are amplified so the physical
        // device capture can distinguish clear, range, and orientation faults.
        Pass
        {
            CGPROGRAM
            #pragma vertex vert_img
            #pragma fragment fragDepthDebug
            #include "UnityCG.cginc"

            UNITY_DECLARE_DEPTH_TEXTURE(_CameraDepthTexture);
            UNITY_DECLARE_DEPTH_TEXTURE(_ImmDepthTex);

            fixed4 fragDepthDebug(v2f_img input) : SV_Target
            {
                float unityDepth = SAMPLE_DEPTH_TEXTURE(_CameraDepthTexture, input.uv);
                float immDepth = SAMPLE_DEPTH_TEXTURE(_ImmDepthTex, input.uv);
                float flippedImmDepth = SAMPLE_DEPTH_TEXTURE(
                    _ImmDepthTex,
                    float2(input.uv.x, 1.0 - input.uv.y));
                return fixed4(
                    saturate(unityDepth * 8.0),
                    saturate(immDepth * 8.0),
                    saturate(flippedImmDepth * 8.0),
                    1.0);
            }
            ENDCG
        }
    }
    Fallback Off
}
