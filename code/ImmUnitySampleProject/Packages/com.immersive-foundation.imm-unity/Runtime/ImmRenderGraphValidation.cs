using System;
using System.Collections;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;

namespace ImmPlayer
{
    // Diagnostic for isolated test players with a loaded, visible document.
    // This tests native stereo submission, not an OpenXR session.
    public static class ImmRenderGraphValidation
    {
        public static IEnumerator VerifyStereoPacket(Camera camera, float radius)
        {
            var target = new RenderTexture(new RenderTextureDescriptor(128, 128)
            {
                graphicsFormat = GraphicsFormat.R8G8B8A8_UNorm,
                depthStencilFormat = GraphicsFormat.D32_SFloat,
                dimension = TextureDimension.Tex2DArray,
                volumeDepth = 2,
                msaaSamples = 1
            });
            var commands = new CommandBuffer { name = "IMM stereo packet readback" };
            try
            {
                Require(target.Create(), "Could not create stereo target.");
                for (int scenario = 0; scenario < 3; ++scenario)
                {
                    commands.Clear();
                    var attachment = new RenderTargetIdentifier(target, 0, CubemapFace.Unknown, -1);
                    commands.SetRenderTarget(attachment);
                    commands.ClearRenderTarget(true, true, Color.black);
                    commands.SetViewport(new Rect(0, 0, 128, 128));
                    var view = camera.worldToCameraMatrix;
                    // The smoke scene is centred three radii ahead of this camera.
                    // Move it six radii sideways in view space, outside the head frustum.
                    if (scenario != 0) view.m03 += radius * 6;
                    var left = view;
                    var right = view;
                    left.m03 -= radius * 0.1f;
                    right.m03 += radius * 0.1f;
                    var projection = GL.GetGPUProjectionMatrix(camera.projectionMatrix, true);
                    var eyeProjection = projection;
                    if (scenario != 0) eyeProjection.m02 += projection.m00 * 2;
                    var cullingProjection = projection;
                    if (scenario == 2)
                    {
                        cullingProjection = eyeProjection;
                        cullingProjection.m00 *= 0.5f;
                        cullingProjection.m02 *= 0.5f;
                    }
                    var transport = ImmRenderGraphSession.Current.Transport;
                    transport.QueueRender(commands, 250, target.colorBuffer.GetNativeRenderBufferPtr(),
                        target.depthBuffer.GetNativeRenderBufferPtr(), 28, 40, new RectInt(0, 0, 128, 128),
                        view, cullingProjection, left, eyeProjection, right, eyeProjection);
                    Graphics.ExecuteCommandBuffer(commands);
                    var readback = AsyncGPUReadback.Request(target, 0, TextureFormat.RGBA32);
                    while (!readback.done) yield return null;
                    Require(!readback.hasError, "Stereo texture-array readback failed.");
                    transport.Poll();
                    Require(transport.IsReady, "Stereo transport faulted.");
                    var leftPixels = readback.GetData<Color32>(0);
                    var rightPixels = readback.GetData<Color32>(1);
                    int leftVisible = 0, rightVisible = 0, different = 0;
                    for (int i = 0; i < leftPixels.Length; ++i)
                    {
                        var l = leftPixels[i]; var r = rightPixels[i];
                        if (l.r + l.g + l.b > 12) ++leftVisible;
                        if (r.r + r.g + r.b > 12) ++rightVisible;
                        if (l.r != r.r || l.g != r.g || l.b != r.b) ++different;
                    }
                    if (scenario == 1)
                    {
                        Require(leftVisible == 0 && rightVisible == 0,
                            "Head-only culling did not reject the peripheral test scene.");
                        continue;
                    }
                    Require(leftVisible > 100, "Left stereo slice has no scene.");
                    Require(rightVisible > 100, "Right stereo slice has no scene.");
                    Require(different > 100, "Stereo eye transforms produced identical images.");

                }
                Debug.Log("[IMM_URP_READBACK] PASS managed stereo packet renders distinct eye slices and retains peripheral content.");
            }
            finally
            {
                commands.Dispose();
                target.Release();
                UnityEngine.Object.Destroy(target);
            }
        }

        private static void Require(bool condition, string message)
        {
            if (!condition) throw new InvalidOperationException(message);
        }
    }
}
