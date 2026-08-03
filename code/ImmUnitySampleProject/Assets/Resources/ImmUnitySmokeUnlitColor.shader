Shader "IMM/SmokeUnlitColor"
{
    Properties
    {
        _Color ("Color", Color) = (1, 1, 1, 1)
    }
    SubShader
    {
        Tags { "RenderType" = "Opaque" "Queue" = "Geometry" }
        Pass
        {
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag

            #include "UnityCG.cginc"

            fixed4 _Color;

            struct appdata
            {
                float4 vertex : POSITION;
            };

            struct v2f
            {
                float4 vertex : SV_POSITION;
            };

            v2f vert(appdata v)
            {
                v2f o;
                o.vertex = UnityObjectToClipPos(v.vertex);
                return o;
            }

            fixed4 frag(v2f i) : SV_Target
            {
                return _Color;
            }
            ENDCG
        }

        // Camera depth textures in the built-in pipeline render objects through
        // their ShadowCaster pass. The composition probes must participate so
        // _CameraDepthTexture represents the same geometry visible in _MainTex.
        Pass
        {
            Name "ShadowCaster"
            Tags { "LightMode" = "ShadowCaster" }
            ZWrite On
            ColorMask 0

            CGPROGRAM
            #pragma vertex vertDepth
            #pragma fragment fragDepth

            #include "UnityCG.cginc"

            struct appdataDepth
            {
                float4 vertex : POSITION;
            };

            struct v2fDepth
            {
                float4 vertex : SV_POSITION;
            };

            v2fDepth vertDepth(appdataDepth input)
            {
                v2fDepth output;
                output.vertex = UnityObjectToClipPos(input.vertex);
                return output;
            }

            fixed4 fragDepth(v2fDepth input) : SV_Target
            {
                return 0;
            }
            ENDCG
        }
    }
}
