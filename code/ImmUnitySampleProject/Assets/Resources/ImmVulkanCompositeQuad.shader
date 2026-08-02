// Fullscreen composite of IMM's per-eye offscreen RTs, drawn INSIDE Unity's
// normal camera pass (queue Overlay) instead of a CommandBuffer.Blit. The blit
// broke Unity's render pass per eye (measured ~9ms/frame on Quest 3); this quad
// keeps the composite in-pass and tile-friendly.
//
// Eye selection: the manager sets the global _ImmEyeIndex from OnCameraPreCull
// per multipass eye pass (deterministic; avoids relying on unity_StereoEyeIndex
// semantics in multipass). V orientation: _FlipY is a runtime material knob so
// parity can be corrected live from the flag file - never again a dead toggle.
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

            // Bound per eye pass via CommandBuffer.SetGlobalTexture - the command
            // buffer executes inside each eye's pass, so the binding cannot race
            // Unity's cull-both-then-render-both multipass ordering (a plain
            // Shader.SetGlobalFloat eye index did race, giving both eyes the
            // same texture - "vision feels odd").
            sampler2D _ImmEyeTex;
            float _FlipY;

            struct appdata
            {
                float4 vertex : POSITION;
                float2 uv : TEXCOORD0;
            };

            struct v2f
            {
                float4 pos : SV_POSITION;
                float2 uv : TEXCOORD0;
            };

            v2f vert (appdata v)
            {
                v2f o;
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
                return tex2D(_ImmEyeTex, i.uv);
            }
            ENDCG
        }
    }
    Fallback Off
}
