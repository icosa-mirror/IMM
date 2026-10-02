using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

namespace ImmPlayer
{
    /// <summary>Submits opted-in IMM cameras through URP RenderGraph after opaque geometry.</summary>
    public sealed class ImmRendererFeature : ScriptableRendererFeature
    {
        private ImmRenderGraphSession session;
        private ImmRenderPass pass;
        private readonly HashSet<string> reported = new HashSet<string>();

        public override void Create()
        {
            session = null;
            pass = null;
            reported.Clear();
        }

        public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData)
        {
            var camera = renderingData.cameraData.camera;
            if (camera == null || camera.cameraType != CameraType.Game ||
                !camera.TryGetComponent<ImmCamera>(out var optIn) || !optIn.isActiveAndEnabled) return;
            if (SystemInfo.graphicsDeviceType != GraphicsDeviceType.Direct3D12)
            {
                Report("The RenderGraph backend currently implements D3D12 only.");
                return;
            }
            var current = ImmRenderGraphSession.Current;
            if (current == null) return; // The document manager owns initialization and shutdown.
            var transport = current.Transport;
            if (transport.IsDisposed) return;
            transport.Poll();
            if (transport.LastError < 0)
            {
                Report($"Native submission failed (0x{transport.LastError:X8}). Restart the IMM session after correcting its configuration.");
                return;
            }
            if (!transport.IsReady) return;
            if (!ReferenceEquals(session, current))
            {
                session = current;
                pass = new ImmRenderPass(transport);
            }
            renderer.EnqueuePass(pass);
        }

        protected override void Dispose(bool disposing)
        {
            // Renderer data assets do not own the shared document session. Disposing
            // one feature must not invalidate another renderer's queued submissions.
            session = null;
            pass = null;
        }

        private void Report(string message)
        {
            if (reported.Add(message)) Debug.LogError($"[IMM_RENDER_GRAPH] {message}");
        }
    }
}
