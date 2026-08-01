Shader "Hidden/IMM/VulkanPresent"
{
    Properties
    {
        _MainTex ("Presentation Texture", 2D) = "black" {}
    }

    SubShader
    {
        Tags { "Queue" = "Overlay" "RenderType" = "Opaque" }

        Pass
        {
            Cull Off
            ZWrite Off
            ZTest Always

            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag

            #include "UnityCG.cginc"

            struct appdata
            {
                float4 vertex : POSITION;
                float2 uv : TEXCOORD0;
            };

            struct v2f
            {
                float4 vertex : SV_POSITION;
                float2 uv : TEXCOORD0;
            };

            sampler2D _MainTex;

            v2f vert(appdata input)
            {
                v2f output;
                // Presentation geometry is already expressed in clip space.
                // Avoid camera matrices here: this pass exists specifically to
                // draw through Unity's active Android Vulkan CameraTarget while
                // overriding the target wrapper's erroneous 1x1 viewport.
                output.vertex = float4(input.vertex.xy, 0.0, 1.0);
                output.uv = input.uv;
                return output;
            }

            fixed4 frag(v2f input) : SV_Target
            {
                return tex2D(_MainTex, input.uv);
            }
            ENDCG
        }
    }
}
