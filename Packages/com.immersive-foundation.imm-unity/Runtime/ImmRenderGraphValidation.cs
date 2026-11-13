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
        public static IEnumerator VerifyStereoPacket(Camera camera, float radius, int samples = 1)
        {
            var descriptor = new RenderTextureDescriptor(128, 128)
            {
                graphicsFormat = GraphicsFormat.R8G8B8A8_UNorm,
                depthStencilFormat = GraphicsFormat.D32_SFloat,
                dimension = TextureDimension.Tex2DArray,
                volumeDepth = 2,
                msaaSamples = samples,
                bindMS = samples > 1
            };
            Require(SystemInfo.GetRenderTextureSupportedMSAASampleCount(descriptor) == samples,
                $"Stereo fixture requires the requested {samples} samples without downgrade.");
            var target = new RenderTexture(descriptor);
            descriptor.msaaSamples = 1;
            descriptor.bindMS = false;
            descriptor.depthStencilFormat = GraphicsFormat.None;
            var resolved = samples > 1 ? new RenderTexture(descriptor) : null;
            var commands = new CommandBuffer { name = "IMM stereo packet readback" };
            ImmSceneSubmission stereoSubmission = default;
            int completedStereo = 0;
            Action<ImmSceneSubmission> onSubmission = submission =>
            {
                if (submission.NativeCameraId != 250 || submission.ViewCount != 2) return;
                stereoSubmission = submission;
                ++completedStereo;
            };
            ImmRenderingDiagnostics.SubmissionCompleted += onSubmission;
            try
            {
                Require(target.Create(), "Could not create stereo target.");
                Require(target.antiAliasing == samples, "Stereo target silently downgraded MSAA.");
                Require(resolved == null || resolved.Create(), "Could not create stereo resolve target.");
                for (int scenario = 0; scenario < 6; ++scenario)
                {
                    commands.Clear();
                    var attachment = new RenderTargetIdentifier(target, 0, CubemapFace.Unknown, -1);
                    commands.SetRenderTarget(attachment);
                    commands.ClearRenderTarget(true, true, Color.black);
                    // Exercise smaller regions in retained full-size attachments, then
                    // return to full size. This runs in the existing stereo CI probe.
                    int viewportSize = scenario == 3 ? 64 : scenario == 4 ? 96 : 128;
                    // Vulkan XR can return offset eye regions. Exercise those in
                    // the GPU readback probe; other backends keep zero-origin coverage.
                    int viewportOrigin = SystemInfo.graphicsDeviceType == GraphicsDeviceType.Vulkan
                        ? (128 - viewportSize) / 2 : 0;
                    var viewport = new RectInt(viewportOrigin, viewportOrigin, viewportSize, viewportSize);
                    commands.SetViewport(new Rect(viewport.x, viewport.y, viewport.width, viewport.height));
                    var view = camera.worldToCameraMatrix;
                    // The smoke scene is centred three radii ahead of this camera.
                    // Move it six radii sideways in view space, outside the head frustum.
                    if (scenario == 1 || scenario == 2) view.m03 += radius * 6;
                    var left = view;
                    var right = view;
                    left.m03 -= radius * 0.1f;
                    right.m03 += radius * 0.1f;
                    var projection = GL.GetGPUProjectionMatrix(camera.projectionMatrix, true);
                    var eyeProjection = projection;
                    if (scenario == 1 || scenario == 2) eyeProjection.m02 += projection.m00 * 2;
                    var cullingProjection = projection;
                    if (scenario == 2)
                    {
                        cullingProjection = eyeProjection;
                        cullingProjection.m00 *= 0.5f;
                        cullingProjection.m02 *= 0.5f;
                    }
                    var transport = ImmRenderGraphSession.Current.Transport;
                    int previousCompleted = completedStereo;
                    int submittedFrame = Time.frameCount;
                    transport.QueueRender(commands, 250, target.colorBuffer.GetNativeRenderBufferPtr(),
                        target.depthBuffer.GetNativeRenderBufferPtr(), 28, 40, viewport,
                        view, cullingProjection, left, eyeProjection, right, eyeProjection,
                        frameIndex: submittedFrame);
                    // Resolve the complete layered surface with one Unity command.
                    if (resolved != null) commands.ResolveAntiAliasedSurface(target, resolved);
                    Graphics.ExecuteCommandBuffer(commands);
                    var readback = AsyncGPUReadback.Request(resolved != null ? resolved : target, 0, TextureFormat.RGBA32);
                    while (!readback.done) yield return null;
                    Require(!readback.hasError, "Stereo texture-array readback failed.");
                    transport.Poll();
                    Require(transport.IsReady, "Stereo transport faulted.");
                    Require(completedStereo == previousCompleted + 1 && stereoSubmission.NativeSceneEvents == 1 &&
                        stereoSubmission.Result == 0 && stereoSubmission.FrameIndex == submittedFrame &&
                        stereoSubmission.XrPassIndex == 0,
                        "Stereo packet did not acknowledge exactly one native scene event with its recorded frame/pass.");
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
                    if (viewportOrigin > 0)
                    {
                        var pixels = readback.GetData<Color32>();
                        for (int slice = 0; slice < 2; ++slice)
                            for (int y = 0; y < 128; ++y)
                                for (int x = 0; x < 128; ++x)
                                {
                                    if (viewport.Contains(new Vector2Int(x, y))) continue;
                                    var pixel = pixels[slice * 128 * 128 + y * 128 + x];
                                    Require(pixel.r == 0 && pixel.g == 0 && pixel.b == 0,
                                        "Vulkan stereo draw escaped its offset eye viewport.");
                                }
                    }

                }
                Debug.Log("[IMM_URP_READBACK] PASS managed stereo packet renders distinct eye slices and retains peripheral content.");
                Debug.Log("[IMM_URP_READBACK] PASS attributed stereo native event.");
                Debug.Log("[IMM_URP_READBACK] PASS changing stereo viewports in retained attachments.");
                Debug.Log($"[IMM_URP_READBACK] PASS stereo packet and layered resolve at {samples} samples.");
            }
            finally
            {
                ImmRenderingDiagnostics.SubmissionCompleted -= onSubmission;
                commands.Dispose();
                target.Release();
                UnityEngine.Object.Destroy(target);
                if (resolved != null)
                {
                    resolved.Release();
                    UnityEngine.Object.Destroy(resolved);
                }
            }
        }

        private static void Require(bool condition, string message)
        {
            if (!condition) throw new InvalidOperationException(message);
        }
    }
}
