// Opaque variant of the IMM eye-RT composite blit. The alpha-blended
// Unlit/Transparent composite forces a read-modify-write of the eye buffer;
// IMM's 360 backdrop is opaque across the whole view, so the blend never
// shows Unity content anyway - an opaque overwrite saves the destination
// read on the tiler. Revert with IMM_UNITY_VK_NO_OPAQUE_COMPOSITE.
Shader "Imm/VulkanCompositeOpaque"
{
    Properties
    {
        _MainTex ("Texture", 2D) = "black" {}
    }
    SubShader
    {
        Tags { "Queue"="Transparent" "RenderType"="Opaque" "IgnoreProjector"="True" }
        Lighting Off
        Cull Off
        ZWrite Off
        ZTest Always
        Blend Off

        Pass
        {
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #include "UnityCG.cginc"

            sampler2D _MainTex;
            float4 _MainTex_ST;

            struct appdata
            {
                float4 vertex : POSITION;
                float2 texcoord : TEXCOORD0;
            };

            struct v2f
            {
                float4 vertex : SV_POSITION;
                float2 texcoord : TEXCOORD0;
            };

            v2f vert (appdata v)
            {
                v2f o;
                o.vertex = UnityObjectToClipPos(v.vertex);
                o.texcoord = TRANSFORM_TEX(v.texcoord, _MainTex);
                return o;
            }

            fixed4 frag (v2f i) : SV_Target
            {
                return tex2D(_MainTex, i.texcoord);
            }
            ENDCG
        }
    }
    Fallback Off
}
