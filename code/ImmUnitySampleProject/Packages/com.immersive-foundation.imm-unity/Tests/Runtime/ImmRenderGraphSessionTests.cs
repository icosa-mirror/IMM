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
                var first = ImmRenderGraphSession.Start(true, false);
                Assert.IsTrue(first.Transport.IsReady);
                Assert.AreSame(first, ImmRenderGraphSession.Current);
                Assert.Throws<System.InvalidOperationException>(() => ImmRenderGraphSession.Start(true, false));
                first.Dispose();
                first.Dispose();
                Assert.IsNull(ImmRenderGraphSession.Current);
                using (var second = ImmRenderGraphSession.Start(false, false))
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
