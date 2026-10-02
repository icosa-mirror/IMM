#if UNITY_EDITOR
using System.Collections;
using System.IO;
using NUnit.Framework;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;
using UnityEngine.TestTools;

namespace ImmPlayer.Tests
{
    public sealed class ImmRenderGraphRenderingTests
    {
        [UnityTest]
        public IEnumerator D3D12UrpRendersDocumentOnlyForOptedInCamera()
        {
            if (SystemInfo.graphicsDeviceType != GraphicsDeviceType.Direct3D12)
                Assert.Ignore("Requires Unity's Windows D3D12 graphics device.");
            var previousPipeline = QualitySettings.renderPipeline;
            var previousTarget = RenderTexture.active;
            var renderer = ScriptableObject.CreateInstance<UniversalRendererData>();
            var feature = ScriptableObject.CreateInstance<ImmRendererFeature>();
            renderer.rendererFeatures.Add(feature);
            var pipeline = UniversalRenderPipelineAsset.Create(renderer);
            pipeline.msaaSampleCount = 8;
            pipeline.supportsHDR = false;
            var descriptor = new RenderTextureDescriptor(256, 256)
            {
                graphicsFormat = GraphicsFormat.R8G8B8A8_UNorm,
                depthStencilFormat = GraphicsFormat.D32_SFloat,
                msaaSamples = 8
            };
            var target = new RenderTexture(descriptor);
            target.Create();
            var cameraObject = new GameObject("IMM URP readback camera", typeof(Camera), typeof(ImmCamera));
            var managerObject = new GameObject("IMM URP readback manager");
            GameObject occluder = null;
            Material occluderMaterial = null;
            var camera = cameraObject.GetComponent<Camera>();
            camera.targetTexture = target;
            camera.clearFlags = CameraClearFlags.SolidColor;
            camera.backgroundColor = Color.black;
            camera.allowHDR = false;
            camera.nearClipPlane = 0.01f;
            camera.farClipPlane = 1000;
            try
            {
                QualitySettings.renderPipeline = pipeline;
                var manager = managerObject.AddComponent<ImmPlayerManager>();
                Assert.IsTrue(manager.IsInitialized);
                var document = manager.LoadDocument(Path.Combine(Application.streamingAssetsPath, "sample1.imm"));
                Assert.IsNotNull(document);
                float deadline = Time.realtimeSinceStartup + 30;
                while (document.GetStateInfo().Loading != ImmDocument.LoadingState.Loaded && Time.realtimeSinceStartup < deadline)
                    yield return null;
                Assert.AreEqual(ImmDocument.LoadingState.Loaded, document.GetStateInfo().Loading);
                var bounds = document.GetBoundingBox();
                float radius = Mathf.Max(bounds.extents.magnitude, 0.1f);
                camera.nearClipPlane = radius * 0.01f;
                camera.farClipPlane = radius * 8;
                camera.transform.position = bounds.center + Vector3.back * radius * 3;
                camera.transform.LookAt(bounds.center);
                document.SetTime(3 * 12600, 0); // IMM ticks, matching the native scene readback.
                document.Show();
                for (int frame = 0; frame < 4; ++frame) yield return null;
                ImmNativePlugin.GetPerformanceInfo(out var perf);
                Debug.Log($"[IMM_URP_READBACK] submissions={ImmRenderGraphSession.Current.Transport.RenderSubmissions} draws={perf.numDrawCalls} paint={perf.numPaintDrawCalls} triangles={perf.numTriangles} culled={perf.numTrianglesCulled} bounds={bounds} camera={camera.transform.position}");
                Assert.Greater(ImmRenderGraphSession.Current.Transport.RenderSubmissions, 0UL);
                string capture = Path.GetFullPath(Path.Combine(Application.dataPath, "../../../artifacts/d3d12-submission/urp-first-frame.png"));
                Assert.Greater(CountVisiblePixels(target, capture), 100, "No visible IMM content in the URP attachment.");
                // A black opaque surface leaves the colour clear unchanged, so only
                // its depth can prevent the native pass from drawing over it.
                var opaqueShader = Shader.Find("Universal Render Pipeline/Unlit");
                Assert.IsNotNull(opaqueShader);
                occluderMaterial = new Material(opaqueShader);
                occluderMaterial.SetColor("_BaseColor", Color.black);
                occluder = GameObject.CreatePrimitive(PrimitiveType.Quad);
                occluder.name = "IMM URP opaque depth occluder";
                occluder.GetComponent<MeshRenderer>().sharedMaterial = occluderMaterial;
                occluder.transform.localScale = Vector3.one * radius * 4;
                occluder.transform.position = bounds.center + Vector3.back * radius * 2;
                for (int frame = 0; frame < 3; ++frame) yield return null;
                Assert.AreEqual(0, CountVisiblePixels(target, null), "IMM ignored the nearer Unity opaque depth.");
                occluder.transform.position = bounds.center + Vector3.forward * radius * 2;
                for (int frame = 0; frame < 3; ++frame) yield return null;
                Assert.Greater(CountVisiblePixels(target, null), 100, "IMM failed to render in front of the farther Unity opaque depth.");
                Debug.Log("[IMM_URP_READBACK] PASS Unity opaque depth occludes IMM only when nearer.");
                ulong submitted = ImmRenderGraphSession.Current.Transport.RenderSubmissions;
                cameraObject.GetComponent<ImmCamera>().enabled = false;
                for (int frame = 0; frame < 3; ++frame) yield return null;
                Assert.AreEqual(submitted, ImmRenderGraphSession.Current.Transport.RenderSubmissions);
                Assert.AreEqual(0, CountVisiblePixels(target, null), "Camera continued rendering IMM after opt-out.");
                manager.Shutdown();
                Assert.IsNull(ImmRenderGraphSession.Current);
            }
            finally
            {
                Object.DestroyImmediate(occluder);
                Object.DestroyImmediate(occluderMaterial);
                Object.DestroyImmediate(managerObject);
                Object.DestroyImmediate(cameraObject);
                RenderTexture.active = previousTarget;
                QualitySettings.renderPipeline = previousPipeline;
                target.Release();
                Object.DestroyImmediate(target);
                Object.DestroyImmediate(pipeline);
                Object.DestroyImmediate(feature);
                Object.DestroyImmediate(renderer);
            }
        }

        private static int CountVisiblePixels(RenderTexture source, string capture)
        {
            var resolved = RenderTexture.GetTemporary(source.width, source.height, 0, RenderTextureFormat.ARGB32);
            var pixels = new Texture2D(source.width, source.height, TextureFormat.RGBA32, false);
            var previous = RenderTexture.active;
            try
            {
                // URP has already resolved its final output. Resolving the transient
                // MSAA surface again can overwrite that output with discarded contents.
                Graphics.Blit(source, resolved);
                RenderTexture.active = resolved;
                pixels.ReadPixels(new Rect(0, 0, source.width, source.height), 0, 0);
                pixels.Apply();
                if (capture != null)
                {
                    Directory.CreateDirectory(Path.GetDirectoryName(capture));
                    File.WriteAllBytes(capture, pixels.EncodeToPNG());
                }
                int visible = 0;
                foreach (var color in pixels.GetPixels32())
                    if (color.r > 5 || color.g > 5 || color.b > 5) ++visible;
                return visible;
            }
            finally
            {
                RenderTexture.active = previous;
                Object.DestroyImmediate(pixels);
                RenderTexture.ReleaseTemporary(resolved);
            }
        }
    }
}
#endif
