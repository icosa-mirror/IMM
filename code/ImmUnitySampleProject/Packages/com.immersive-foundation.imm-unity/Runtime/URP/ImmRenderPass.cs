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
            internal Matrix4x4 View, Projection;
            internal Matrix4x4 LeftView, LeftProjection, RightView, RightProjection;
        }

        private readonly HashSet<string> reported = new HashSet<string>();
        private readonly ImmRenderGraphTransport transport;

        internal ImmRenderPass(ImmRenderGraphTransport transport)
        {
            this.transport = transport ?? throw new ArgumentNullException(nameof(transport));
            renderPassEvent = RenderPassEvent.AfterRenderingOpaques;
            // Request addressable attachments explicitly. Do not infer that URP always
            // renders to a RenderTexture, or reinterpret a native texture as a render buffer.
            requiresIntermediateTexture = true;
        }

        public override void RecordRenderGraph(RenderGraph graph, ContextContainer frameData)
        {
            if (transport.IsDisposed) return;
            transport.Poll();
            if (!transport.IsReady) return;
            var camera = frameData.Get<UniversalCameraData>();
            if (!ImmCamera.TryAcquire(camera.camera, out int cameraId)) return;
            bool stereo = camera.xr.enabled;
            if (stereo && (!camera.xr.singlePassEnabled || camera.xr.viewCount != 2 ||
                camera.xr.GetTextureArraySlice(0) != 0 || camera.xr.GetTextureArraySlice(1) != 1 ||
                camera.xr.GetViewport(0) != camera.xr.GetViewport(1)))
            {
                Report("IMM D3D12 XR requires two single-pass views in slices 0 and 1 with matching viewports.");
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
            using (var builder = graph.AddUnsafePass<PassData>("IMM native rendering", out var data))
            {
                data.Color = resources.activeColorTexture;
                data.Depth = resources.activeDepthTexture;
                data.HasTargetTexture = camera.targetTexture != null;
                data.Transport = transport;
                data.CameraId = cameraId;
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
                builder.UseTexture(data.Color, AccessFlags.ReadWrite);
                builder.UseTexture(data.Depth, AccessFlags.ReadWrite);
                builder.AllowPassCulling(false);
                builder.SetRenderFunc(static (PassData pass, UnsafeGraphContext context) => ExecuteNative(pass, context));
            }
        }

        private static void ExecuteNative(PassData data, UnsafeGraphContext context)
        {
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
                data.Owner.Report("IMM D3D12 requires matching mono 2D or stereo two-slice attachments with 1, 2, 4 or 8 samples without dynamic resolution.");
                return;
            }
            uint colorFormat = DxgiFormat(c.graphicsFormat);
            uint depthFormat = DxgiFormat(d.depthStencilFormat);
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
            CommandBuffer commands = CommandBufferHelpers.GetNativeCommandBuffer(context.cmd);
            commands.SetRenderTarget(colorTarget, RenderBufferLoadAction.Load, RenderBufferStoreAction.Store,
                depthTarget, RenderBufferLoadAction.Load, RenderBufferStoreAction.Store);
            commands.SetViewport(new Rect(0, 0, size.x, size.y));
            data.Transport.QueueRender(commands, data.CameraId,
                c.colorBuffer.GetNativeRenderBufferPtr(), d.depthBuffer.GetNativeRenderBufferPtr(),
                colorFormat, depthFormat, viewport, data.View, projection,
                data.Stereo ? data.LeftView : null,
                data.Stereo ? GL.GetGPUProjectionMatrix(data.LeftProjection, flipped) : null,
                data.Stereo ? data.RightView : null,
                data.Stereo ? GL.GetGPUProjectionMatrix(data.RightProjection, flipped) : null);
            // The native event uses its own command list and restores resource states.
            // Rebind Unity attachments/viewport after the event for subsequent graph work.
            commands.SetRenderTarget(colorTarget, RenderBufferLoadAction.Load, RenderBufferStoreAction.Store,
                depthTarget, RenderBufferLoadAction.Load, RenderBufferStoreAction.Store);
            commands.SetViewport(new Rect(0, 0, size.x, size.y));
        }

        private void Report(string message)
        {
            if (reported.Add(message)) Debug.LogError($"[IMM_RENDER_GRAPH] {message}");
        }

        private static uint DxgiFormat(GraphicsFormat format)
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
