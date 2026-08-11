// Fullscreen composite of IMM's per-eye offscreen RTs, drawn INSIDE Unity's
// normal camera pass (queue Overlay) instead of a CommandBuffer.Blit. The blit
// broke Unity's render pass per eye (measured ~9ms/frame on Quest 3); this quad
// keeps the composite in-pass and tile-friendly.
//
// Eye selection happens in the GPU eye pass. OnPreCull is not eye-authoritative
// in Unity Multi Pass and can report Left for both passes, so both eye textures
// stay bound and the shader selects with Unity's stereo eye index. V orientation:
// _FlipY is a runtime material knob so parity can be corrected live from the flag
// file - never again a dead toggle.
Shader "Imm/VulkanCompositeQuad"
{
    Properties
    {
        _EyeTex0 ("Left Eye", 2D) = "black" {}
        _EyeTex1 ("Right Eye", 2D) = "black" {}
        _FlipY ("Flip V", Float) = 0
    }
    SubShader
    {
        Tags { "Queue"="Overlay" "RenderType"="Overlay" "IgnoreProjector"="True" }
        Lighting Off
        Cull Off
        ZWrite Off
        ZTest Always
        Blend SrcAlpha OneMinusSrcAlpha

        Pass
        {
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #include "UnityCG.cginc"

            sampler2D _EyeTex0;
            sampler2D _EyeTex1;
            float _FlipY;

            struct appdata
            {
                float4 vertex : POSITION;
                float2 uv : TEXCOORD0;
                UNITY_VERTEX_INPUT_INSTANCE_ID
            };

            struct v2f
            {
                float4 pos : SV_POSITION;
                float2 uv : TEXCOORD0;
                UNITY_VERTEX_OUTPUT_STEREO
            };

            v2f vert (appdata v)
            {
                v2f o;
                UNITY_SETUP_INSTANCE_ID(v);
                UNITY_INITIALIZE_OUTPUT(v2f, o);
                UNITY_INITIALIZE_VERTEX_OUTPUT_STEREO(o);
                // Quad mesh corners are (+-0.5, +-0.5); map straight to full
                // clip space, ignoring all transforms.
                o.pos = float4(v.vertex.x * 2.0, v.vertex.y * 2.0, 0.0, 1.0);
                o.uv = v.uv;
                if (_FlipY > 0.5)
                    o.uv.y = 1.0 - o.uv.y;
                return o;
            }

            fixed4 frag (v2f i) : SV_Target
            {
                UNITY_SETUP_STEREO_EYE_INDEX_POST_VERTEX(i);
                return unity_StereoEyeIndex == 0
                    ? tex2D(_EyeTex0, i.uv)
                    : tex2D(_EyeTex1, i.uv);
            }
            ENDCG
        }
    }
    Fallback Off
}
