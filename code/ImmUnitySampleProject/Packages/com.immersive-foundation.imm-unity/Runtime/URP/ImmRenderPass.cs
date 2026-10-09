using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;
using UnityEngine.Rendering.RenderGraphModule;
using UnityEngine.Rendering.Universal;

namespace ImmPlayer
{
    // The session owner supplies an acknowledged transport; camera IDs come from opt-in.
    internal sealed class ImmRenderPass : ScriptableRenderPass
    {
        private sealed class PassData
        {
            internal TextureHandle Color, Depth;
            internal bool HasTargetTexture;
            internal bool Stereo;
            internal RenderTargetIdentifier XrTarget;
            internal Rect XrViewport;
            internal ImmRenderGraphTransport Transport;
            internal ImmRenderPass Owner;
            internal int CameraId;
            internal int FrameIndex;
            internal Camera SourceCamera;
            internal Matrix4x4 View, Projection;
            internal Matrix4x4 LeftView, LeftProjection, RightView, RightProjection;
        }

        private readonly HashSet<string> reported = new HashSet<string>();
        private readonly ImmRenderGraphTransport transport;

        internal ImmRenderPass(ImmRenderGraphTransport transport)
        {
            this.transport = transport ?? throw new ArgumentNullException(nameof(transport));
            // Backdrops must survive Unity's skybox while sharing opaque depth;
            // later transparent geometry must still compose over IMM surfaces.
            renderPassEvent = RenderPassEvent.AfterRenderingSkybox;
            // Request addressable attachments explicitly. Do not infer that URP always
            // renders to a RenderTexture, or reinterpret a native texture as a render buffer.
            requiresIntermediateTexture = true;
        }

        public override void RecordRenderGraph(RenderGraph graph, ContextContainer frameData)
        {
            using var allocation = ImmRenderingDiagnostics.Measure(ImmRenderCallback.RecordRenderGraph);
            if (transport.IsDisposed) return;
            transport.Poll();
            if (!transport.IsReady) return;
            var camera = frameData.Get<UniversalCameraData>();
            if (!ImmCamera.TryAcquire(camera.camera, out int cameraId)) return;
            bool stereo = camera.xr.enabled;
            if (stereo && SystemInfo.graphicsDeviceType == GraphicsDeviceType.Metal)
            {
                Report($"IMM {SystemInfo.graphicsDeviceType} RenderGraph currently supports mono cameras only.");
                return;
            }
            if (stereo && (!camera.xr.singlePassEnabled || camera.xr.viewCount != 2 ||
                !camera.xr.isFirstCameraPass || !camera.xr.isLastCameraPass ||
                camera.xr.GetTextureArraySlice(0) != 0 || camera.xr.GetTextureArraySlice(1) != 1 ||
                camera.xr.GetViewport(0) != camera.xr.GetViewport(1)))
            {
                Report("IMM XR requires one XR pass with two single-pass views in slices 0 and 1 and matching viewports.");
                return;
            }
            if (camera.renderType != CameraRenderType.Base || camera.camera.rect != new Rect(0, 0, 1, 1))
            {
                Report("Camera stacks and partial camera viewports are not implemented yet.");
                return;
            }
            var resources = frameData.Get<UniversalResourceData>();
            if (!resources.activeColorTexture.IsValid() || !resources.activeDepthTexture.IsValid())
            {
                Report("The IMM pass requires active colour and depth attachments.");
                return;
            }
            if (SystemInfo.graphicsDeviceType == GraphicsDeviceType.Vulkan)
            {
                // A raster pass establishes the host native render pass. An unsafe
                // pass declares resource access but supplies no attachment binding.
                using (var builder = graph.AddRasterRenderPass<PassData>("IMM Vulkan rendering", out var data))
                {
                    PopulateData(data, camera, resources, cameraId, stereo);
                    builder.SetRenderAttachment(data.Color, 0, AccessFlags.ReadWrite);
                    builder.SetRenderAttachmentDepth(data.Depth, AccessFlags.ReadWrite);
                    builder.AllowGlobalStateModification(true);
                    builder.AllowPassCulling(false);
                    builder.SetRenderFunc(static (PassData pass, RasterGraphContext context) =>
                        ExecuteNative(pass, null, context.cmd));
                }
                return;
            }
            using (var builder = graph.AddUnsafePass<PassData>("IMM native rendering", out var data))
            {
                PopulateData(data, camera, resources, cameraId, stereo);
                builder.UseTexture(data.Color, AccessFlags.ReadWrite);
                builder.UseTexture(data.Depth, AccessFlags.ReadWrite);
                builder.AllowPassCulling(false);
                builder.SetRenderFunc(static (PassData pass, UnsafeGraphContext context) => ExecuteNative(pass, CommandBufferHelpers.GetNativeCommandBuffer(context.cmd)));
            }
        }

        private void PopulateData(PassData data, UniversalCameraData camera,
            UniversalResourceData resources, int cameraId, bool stereo)
        {
            data.Color = resources.activeColorTexture;
            data.Depth = resources.activeDepthTexture;
            data.HasTargetTexture = camera.targetTexture != null;
            data.Transport = transport;
            data.CameraId = cameraId;
            data.FrameIndex = Time.frameCount;
            data.SourceCamera = camera.camera;
            data.Owner = this;
            data.View = camera.GetViewMatrix();
            data.Projection = camera.GetProjectionMatrix();
            data.Stereo = stereo;
            if (stereo)
            {
                data.XrTarget = new RenderTargetIdentifier(camera.xr.renderTarget, 0, CubemapFace.Unknown, 0);
                data.XrViewport = camera.xr.GetViewport(0);
                data.View = camera.xr.cullingParams.stereoViewMatrix;
                data.Projection = camera.xr.cullingParams.stereoProjectionMatrix;
                data.LeftView = camera.GetViewMatrix(0);
                data.LeftProjection = camera.GetProjectionMatrix(0);
                data.RightView = camera.GetViewMatrix(1);
                data.RightProjection = camera.GetProjectionMatrix(1);
            }
        }

        private static void ExecuteNative(PassData data, CommandBuffer commands, RasterCommandBuffer rasterCommands = null)
        {
            using var allocation = ImmRenderingDiagnostics.Measure(ImmRenderCallback.ExecuteNative);
            if (data.Transport.IsDisposed) return;
            // Resolve handles only while the graph's resource registry is active.
            RTHandle color = data.Color;
            RTHandle depth = data.Depth;
            if (color.rt == null || depth.rt == null)
            {
                data.Owner.Report("IMM requires addressable intermediate colour and depth attachments.");
                return;
            }
            var c = color.rt;
            var d = depth.rt;
            var dimension = data.Stereo ? TextureDimension.Tex2DArray : TextureDimension.Tex2D;
            int slices = data.Stereo ? 2 : 1;
            if (c.dimension != dimension || d.dimension != dimension ||
                c.volumeDepth != slices || d.volumeDepth != slices || c.width != d.width || c.height != d.height ||
                c.antiAliasing != d.antiAliasing ||
                (c.antiAliasing != 1 && c.antiAliasing != 2 && c.antiAliasing != 4 && c.antiAliasing != 8) ||
                c.useDynamicScale || d.useDynamicScale)
            {
                data.Owner.Report("IMM requires matching mono 2D or stereo two-slice attachments with 1, 2, 4 or 8 samples without dynamic resolution.");
                return;
            }
            uint colorFormat = AttachmentFormat(c.graphicsFormat);
            uint depthFormat = AttachmentFormat(d.depthStencilFormat);
            if (colorFormat == 0 || depthFormat == 0)
            {
                data.Owner.Report($"Unsupported IMM attachment formats: {c.graphicsFormat}, {d.depthStencilFormat}.");
                return;
            }
            // URP 17.6's GPU accessor is internal. Follow its public RenderObjects pass:
            // convert the URP projection once, using the actual attachments' flip state.
            // Snapshot camera state at record time instead of retaining URP's pooled
            // UniversalCameraData. This is URP 17.6's flip rule for the supported
            // game camera, evaluated against the resolved attachment and XR target.
            var colorId = new RenderTargetIdentifier(color.nameID, 0, CubemapFace.Unknown, 0);
            bool isBackbuffer = colorId == BuiltinRenderTextureType.CameraTarget ||
                colorId == BuiltinRenderTextureType.Depth || (data.Stereo && colorId == data.XrTarget);
            bool flipped = !SystemInfo.graphicsUVStartsAtTop || data.HasTargetTexture || !isBackbuffer;
            Matrix4x4 projection = GL.GetGPUProjectionMatrix(data.Projection, flipped);
            var size = color.useScaling
                ? color.GetScaledSize(color.rtHandleProperties.currentViewportSize)
                : new Vector2Int(c.width, c.height);
            var depthSize = depth.useScaling
                ? depth.GetScaledSize(depth.rtHandleProperties.currentViewportSize)
                : new Vector2Int(d.width, d.height);
            if (size != depthSize || size.x <= 0 || size.y <= 0 || size.x > c.width || size.y > c.height)
            {
                data.Owner.Report("IMM colour and depth viewport sizes must match and fit their attachments.");
                return;
            }
            var viewport = new RectInt(0, 0, size.x, size.y);
            if (data.Stereo && data.XrViewport != new Rect(0, 0, size.x, size.y))
            {
                data.Owner.Report("IMM D3D12 XR requires full-size eye viewports.");
                return;
            }
            var colorTarget = new RenderTargetIdentifier(color.nameID, 0, CubemapFace.Unknown, data.Stereo ? -1 : 0);
            var depthTarget = new RenderTargetIdentifier(depth.nameID, 0, CubemapFace.Unknown, data.Stereo ? -1 : 0);
            if (commands != null)
            {
                commands.SetRenderTarget(colorTarget, RenderBufferLoadAction.Load, RenderBufferStoreAction.Store,
                    depthTarget, RenderBufferLoadAction.Load, RenderBufferStoreAction.Store);
                commands.SetViewport(new Rect(0, 0, size.x, size.y));
            }
            else rasterCommands.SetViewport(new Rect(0, 0, size.x, size.y));
            IntPtr colorBuffer = c.colorBuffer.GetNativeRenderBufferPtr();
            IntPtr depthBuffer = d.depthBuffer.GetNativeRenderBufferPtr();
            // iOS MSAA depth can have a texture without a Unity RenderBuffer wrapper.
            // Keep the native texture's type explicit in the packet operation.
            bool depthIsMetalTexture = depthBuffer == IntPtr.Zero && SystemInfo.graphicsDeviceType == GraphicsDeviceType.Metal;
            if (depthIsMetalTexture) depthBuffer = d.GetNativeDepthBufferPtr();
            if (depthBuffer == IntPtr.Zero && SystemInfo.graphicsDeviceType == GraphicsDeviceType.Vulkan)
            {
                d.GetNativeDepthBufferPtr();
                depthBuffer = d.depthBuffer.GetNativeRenderBufferPtr();
            }
            if (colorBuffer == IntPtr.Zero && (SystemInfo.graphicsDeviceType == GraphicsDeviceType.Metal ||
                SystemInfo.graphicsDeviceType == GraphicsDeviceType.Vulkan))
            {
                // RenderGraph allocation can precede native surface creation.
                // Materialize the texture, then fetch its Unity buffer wrapper again.
                // The texture pointer may refer to the resolve image, so do not submit
                // it in place of the multisample colour attachment.
                c.GetNativeTexturePtr();
                colorBuffer = c.colorBuffer.GetNativeRenderBufferPtr();
            }
            if (colorBuffer == IntPtr.Zero || depthBuffer == IntPtr.Zero)
                throw new InvalidOperationException($"[IMM_RENDER_GRAPH] Missing native attachments: colorMissing={colorBuffer == IntPtr.Zero} depthMissing={depthBuffer == IntPtr.Zero} color={c.name} format={c.graphicsFormat} samples={c.antiAliasing} memoryless={c.memorylessMode} created={c.IsCreated()} depth={d.name} format={d.depthStencilFormat} samples={d.antiAliasing} memoryless={d.memorylessMode} created={d.IsCreated()}.");
            data.Transport.QueueRender(commands, data.CameraId,
                colorBuffer, depthBuffer,
                colorFormat, depthFormat, viewport, data.View, projection,
                data.Stereo ? data.LeftView : null,
                data.Stereo ? GL.GetGPUProjectionMatrix(data.LeftProjection, flipped) : null,
                data.Stereo ? data.RightView : null,
                data.Stereo ? GL.GetGPUProjectionMatrix(data.RightProjection, flipped) : null,
                depthIsMetalTexture: depthIsMetalTexture, rasterCommands: rasterCommands,
                sourceCamera: data.SourceCamera, frameIndex: data.FrameIndex);
            // The native event uses its own command list and restores resource states.
            // Rebind Unity attachments/viewport after the event for subsequent graph work.
            if (commands != null)
            {
                commands.SetRenderTarget(colorTarget, RenderBufferLoadAction.Load, RenderBufferStoreAction.Store,
                    depthTarget, RenderBufferLoadAction.Load, RenderBufferStoreAction.Store);
                commands.SetViewport(new Rect(0, 0, size.x, size.y));
            }
            else rasterCommands.SetViewport(new Rect(0, 0, size.x, size.y));
        }

        private void Report(string message)
        {
            if (reported.Add(message)) Debug.LogError($"[IMM_RENDER_GRAPH] {message}");
        }

        private static uint AttachmentFormat(GraphicsFormat format)
        {
            switch (format)
            {
                case GraphicsFormat.R8G8B8A8_UNorm: return 28;
                case GraphicsFormat.R8G8B8A8_SRGB: return 29;
                case GraphicsFormat.B8G8R8A8_UNorm: return 87;
                case GraphicsFormat.B8G8R8A8_SRGB: return 91;
                case GraphicsFormat.R16G16B16A16_SFloat: return 10;
                case GraphicsFormat.B10G11R11_UFloatPack32: return 26;
                case GraphicsFormat.D16_UNorm: return 55;
                case GraphicsFormat.D24_UNorm_S8_UInt: return 45;
                case GraphicsFormat.D32_SFloat: return 40;
                case GraphicsFormat.D32_SFloat_S8_UInt: return 20;
                default: return 0;
            }
        }
    }
}
