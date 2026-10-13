using NUnit.Framework;
using UnityEngine;
using UnityEngine.Rendering;

namespace ImmPlayer.Tests
{
    public sealed class ImmRenderGraphSessionTests
    {
        [Test]
        public void D3D12SessionAcknowledgesShutdownAndCanBeRecreated()
        {
            if (SystemInfo.graphicsDeviceType != GraphicsDeviceType.Direct3D12)
                Assert.Ignore("Requires Unity's Windows D3D12 graphics device.");
            Assert.IsNull(ImmRenderGraphSession.Current);
            try
            {
                var first = ImmRenderGraphSession.Start(true, false, ImmPaintRenderingTechnique.Static);
                Assert.IsTrue(first.Transport.IsReady);
                Assert.AreSame(first, ImmRenderGraphSession.Current);
                Assert.Throws<System.InvalidOperationException>(() => ImmRenderGraphSession.Start(true, false, ImmPaintRenderingTechnique.Static));
                first.Dispose();
                first.Dispose();
                Assert.IsNull(ImmRenderGraphSession.Current);
                using (var second = ImmRenderGraphSession.Start(false, false, ImmPaintRenderingTechnique.Pretessellated))
                    Assert.IsTrue(second.Transport.IsReady);
                Assert.IsNull(ImmRenderGraphSession.Current);
            }
            finally
            {
                ImmRenderGraphSession.Current?.Dispose();
            }
        }
    }
}
