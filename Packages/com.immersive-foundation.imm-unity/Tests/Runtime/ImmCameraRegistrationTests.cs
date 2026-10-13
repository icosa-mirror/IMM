using NUnit.Framework;
using UnityEngine;

namespace ImmPlayer.Tests
{
    public sealed class ImmCameraRegistrationTests
    {
        [Test]
        public void OptInMustBeActiveAndCameraIdsAreReleased()
        {
            var first = new GameObject("IMM registration first", typeof(Camera));
            var second = new GameObject("IMM registration second", typeof(Camera));
            var third = new GameObject("IMM registration third", typeof(Camera));
            try
            {
                var a = first.GetComponent<Camera>();
                var b = second.GetComponent<Camera>();
                Assert.IsFalse(ImmCamera.TryAcquire(a, out _));
                var optIn = first.AddComponent<ImmCamera>();
                Assert.IsTrue(ImmCamera.TryAcquire(a, out int firstId));
                Assert.IsTrue(ImmCamera.TryAcquire(a, out int repeatedId));
                Assert.AreEqual(firstId, repeatedId);
                second.AddComponent<ImmCamera>();
                Assert.IsTrue(ImmCamera.TryAcquire(b, out int secondId));
                Assert.AreNotEqual(firstId, secondId);
                optIn.enabled = false;
                Assert.IsFalse(ImmCamera.TryAcquire(a, out _));
                var replacement = third.AddComponent<ImmCamera>();
                Assert.IsTrue(ImmCamera.TryAcquire(third.GetComponent<Camera>(), out int releasedId));
                Assert.AreEqual(firstId, releasedId);
                Object.DestroyImmediate(replacement);
                optIn.enabled = true;
                Assert.IsTrue(ImmCamera.TryAcquire(a, out int reusedId));
                Assert.AreEqual(firstId, reusedId);
                Object.DestroyImmediate(optIn);
                Assert.IsFalse(ImmCamera.TryAcquire(a, out _));
            }
            finally
            {
                Object.DestroyImmediate(first);
                Object.DestroyImmediate(second);
                Object.DestroyImmediate(third);
            }
        }
    }
}
