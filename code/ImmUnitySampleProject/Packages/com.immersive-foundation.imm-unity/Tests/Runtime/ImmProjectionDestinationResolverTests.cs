using NUnit.Framework;
using UnityEngine;
using UnityEngine.Rendering;

namespace ImmPlayer.Tests
{
    public sealed class ImmProjectionDestinationResolverTests
    {
        [TestCase(
            GraphicsDeviceType.Direct3D11,
            CameraType.Game,
            false,
            false,
            true,
            ImmProjectionDestination.EditorGameView,
            true)]
        [TestCase(
            GraphicsDeviceType.Direct3D11,
            CameraType.Game,
            false,
            false,
            false,
            ImmProjectionDestination.Backbuffer,
            false)]
        [TestCase(
            GraphicsDeviceType.Direct3D11,
            CameraType.Game,
            false,
            true,
            false,
            ImmProjectionDestination.ExplicitRenderTexture,
            true)]
        [TestCase(
            GraphicsDeviceType.Direct3D11,
            CameraType.SceneView,
            false,
            false,
            true,
            ImmProjectionDestination.EditorSceneView,
            true)]
        [TestCase(
            GraphicsDeviceType.Direct3D11,
            CameraType.Game,
            true,
            false,
            true,
            ImmProjectionDestination.XrDisplay,
            false)]
        [TestCase(
            GraphicsDeviceType.Vulkan,
            CameraType.Game,
            false,
            false,
            false,
            ImmProjectionDestination.VulkanHostAttachment,
            true)]
        [TestCase(
            GraphicsDeviceType.Metal,
            CameraType.Game,
            false,
            false,
            true,
            ImmProjectionDestination.EditorGameView,
            true)]
        public void ResolvesDestinationAndProjectionMode(
            GraphicsDeviceType graphicsDeviceType,
            CameraType cameraType,
            bool stereoEnabled,
            bool hasExplicitRenderTexture,
            bool isEditor,
            ImmProjectionDestination expectedDestination,
            bool expectedRenderIntoTexture)
        {
            ImmProjectionDestination destination = ImmProjectionDestinationResolver.Resolve(
                graphicsDeviceType,
                cameraType,
                stereoEnabled,
                hasExplicitRenderTexture,
                isEditor,
                false,
                false);

            Assert.That(destination, Is.EqualTo(expectedDestination));
            Assert.That(
                ImmProjectionDestinationResolver.UsesRenderTextureProjection(destination),
                Is.EqualTo(expectedRenderIntoTexture));
        }

        [Test]
        public void DiagnosticBackbufferOverrideKeepsExistingPrecedence()
        {
            ImmProjectionDestination destination = ImmProjectionDestinationResolver.Resolve(
                GraphicsDeviceType.Direct3D11,
                CameraType.Game,
                false,
                true,
                true,
                true,
                true);

            Assert.That(destination, Is.EqualTo(ImmProjectionDestination.ForcedBackbuffer));
            Assert.That(
                ImmProjectionDestinationResolver.UsesRenderTextureProjection(destination),
                Is.False);
        }
    }
}
