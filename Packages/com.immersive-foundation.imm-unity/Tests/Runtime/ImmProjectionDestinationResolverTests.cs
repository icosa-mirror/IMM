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
            "EditorGameView",
            true)]
        [TestCase(
            GraphicsDeviceType.Direct3D11,
            CameraType.Game,
            false,
            false,
            false,
            "Backbuffer",
            false)]
        [TestCase(
            GraphicsDeviceType.Direct3D11,
            CameraType.Game,
            false,
            true,
            false,
            "ExplicitRenderTexture",
            true)]
        [TestCase(
            GraphicsDeviceType.Direct3D11,
            CameraType.SceneView,
            false,
            false,
            true,
            "EditorSceneView",
            true)]
        [TestCase(
            GraphicsDeviceType.Direct3D11,
            CameraType.Game,
            true,
            false,
            true,
            "XrDisplay",
            true)]
        [TestCase(
            GraphicsDeviceType.Vulkan,
            CameraType.Game,
            false,
            false,
            false,
            "VulkanHostAttachment",
            true)]
        [TestCase(
            GraphicsDeviceType.Metal,
            CameraType.Game,
            false,
            false,
            true,
            "EditorGameView",
            true)]
        public void ResolvesDestinationAndProjectionMode(
            GraphicsDeviceType graphicsDeviceType,
            CameraType cameraType,
            bool stereoEnabled,
            bool hasExplicitRenderTexture,
            bool isEditor,
            string expectedDestination,
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

            Assert.That(destination.ToString(), Is.EqualTo(expectedDestination));
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
