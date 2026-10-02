using System;
using UnityEngine;
using UnityEngine.Rendering;

namespace ImmPlayer
{
    // One process-wide native bridge is shared by all opted-in cameras and renderer assets.
    // Start/Dispose run on Unity's main thread, outside RenderGraph execution. They may stall;
    // frame submissions only use Transport and its asynchronous packet acknowledgements.
    internal sealed class ImmRenderGraphSession : IDisposable
    {
        internal static ImmRenderGraphSession Current { get; private set; }
        internal ImmRenderGraphTransport Transport { get; }
        private readonly GraphicsBuffer completionBuffer;
        private readonly CommandBuffer lifecycleCommands;
        private bool disposed;

        private ImmRenderGraphSession()
        {
            if (!SystemInfo.supportsAsyncGPUReadback)
                throw new NotSupportedException("IMM session lifecycle requires GPU readback support.");
            Transport = new ImmRenderGraphTransport();
            try
            {
                completionBuffer = new GraphicsBuffer(GraphicsBuffer.Target.Structured, 1, sizeof(int));
                completionBuffer.SetData(new[] { 0 });
                lifecycleCommands = new CommandBuffer { name = "IMM RenderGraph session lifecycle" };
            }
            catch
            {
                completionBuffer?.Dispose();
                Transport.Dispose();
                throw;
            }
        }

        internal static ImmRenderGraphSession Start(bool linearColor, bool enableSound)
        {
            if (Current != null)
                throw new InvalidOperationException("Dispose the current IMM RenderGraph session before starting another.");
            var session = new ImmRenderGraphSession();
            // Retain the owner even if submission or acknowledgement fails. No outstanding
            // packet storage may become unowned while the rendering thread can still use it.
            Current = session;
            Application.quitting += ShutdownCurrent;
#if UNITY_EDITOR
            UnityEditor.AssemblyReloadEvents.beforeAssemblyReload += ShutdownCurrent;
#endif
            try
            {
                session.Transport.QueueInitialize(session.lifecycleCommands, linearColor, enableSound);
                session.ExecuteLifecycle();
                if (!session.Transport.IsReady)
                    throw new InvalidOperationException($"IMM initialization was not acknowledged successfully (0x{session.Transport.LastError:X8}).");
                return session;
            }
            catch (Exception startupError)
            {
                try { session.Dispose(); }
                catch (Exception shutdownError)
                {
                    throw new AggregateException("IMM startup failed and its session still requires shutdown.", startupError, shutdownError);
                }
                throw;
            }
        }

        private void ExecuteLifecycle()
        {
            Graphics.ExecuteCommandBuffer(lifecycleCommands);
            lifecycleCommands.Clear();
            // This request follows the plugin event on Unity's graphics submission path.
            // Waiting is confined to lifecycle boundaries. Packet acknowledgement remains
            // the authority: even a successful readback never permits freeing pending data.
            var completion = AsyncGPUReadback.Request(completionBuffer);
            completion.WaitForCompletion();
            Transport.Poll();
            if (completion.hasError)
                throw new InvalidOperationException("IMM lifecycle synchronization readback failed.");
        }

        private static void ShutdownCurrent() => Current?.Dispose();

        // Invoked only by the dedicated CI lifecycle run, before the smoke scene starts.
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        private static void RunLifecycleSmoke()
        {
            if (Environment.GetEnvironmentVariable("IMM_UNITY_D3D12_LIFECYCLE_SMOKE") != "1") return;
            int exitCode = 1;
            try
            {
                using (Start(true, false)) { }
                using (Start(false, false)) { }
                Debug.Log("[IMM_RENDER_GRAPH_LIFECYCLE] PASS initialized, shut down and recreated on D3D12.");
                exitCode = 0;
            }
            catch (Exception error)
            {
                Debug.LogError($"[IMM_RENDER_GRAPH_LIFECYCLE] FAIL {error}");
            }
            Application.Quit(exitCode);
        }

        public void Dispose()
        {
            if (disposed) return;
            lifecycleCommands.Clear();
            Transport.QueueShutdown(lifecycleCommands);
            ExecuteLifecycle();
            if (!Transport.IsStopped)
                throw new InvalidOperationException($"IMM shutdown is not acknowledged (0x{Transport.LastError:X8}); retaining packet storage.");
            Transport.Dispose();
            lifecycleCommands.Dispose();
            completionBuffer.Dispose();
            Application.quitting -= ShutdownCurrent;
#if UNITY_EDITOR
            UnityEditor.AssemblyReloadEvents.beforeAssemblyReload -= ShutdownCurrent;
#endif
            disposed = true;
            Current = null;
        }
    }
}
