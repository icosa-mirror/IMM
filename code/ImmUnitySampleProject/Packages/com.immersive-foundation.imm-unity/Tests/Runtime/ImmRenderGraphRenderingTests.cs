#if UNITY_EDITOR
using System.Collections;
using System.IO;
using NUnit.Framework;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;
using UnityEngine.SceneManagement;
using UnityEditor.SceneManagement;
using UnityEngine.TestTools;

namespace ImmPlayer.Tests
{
    public sealed class ImmRenderGraphRenderingTests
    {
        [UnityTest]
        public IEnumerator D3D12ConfiguredSampleRendersDocument()
        {
            if (SystemInfo.graphicsDeviceType != GraphicsDeviceType.Direct3D12)
                Assert.Ignore("Requires Unity's Windows D3D12 graphics device.");
            const string scenePath = "Assets/Scenes/SampleSceneURP.unity";
            Assert.IsTrue(File.Exists(scenePath), "Configured URP sample scene is missing.");
            var previousPipeline = QualitySettings.renderPipeline;
            var target = new RenderTexture(new RenderTextureDescriptor(256, 256)
            {
                graphicsFormat = GraphicsFormat.R8G8B8A8_UNorm,
                depthStencilFormat = GraphicsFormat.D32_SFloat,
                msaaSamples = 8
            });
            target.Create();
            var scene = EditorSceneManager.LoadSceneInPlayMode(scenePath,
                new LoadSceneParameters(LoadSceneMode.Additive));
            AsyncOperation unload = null;
            try
            {
                while (!scene.isLoaded) yield return null;
                Camera camera = null;
                foreach (var root in scene.GetRootGameObjects())
                {
                    camera = root.GetComponentInChildren<Camera>();
                    if (camera != null) break;
                }
                Assert.IsNotNull(camera);
                Assert.IsNotNull(camera.GetComponent<ImmCamera>());
                camera.targetTexture = target;
                float deadline = Time.realtimeSinceStartup + 30;
                int visible = 0;
                do
                {
                    yield return null;
                    visible = CountVisiblePixels(target, null);
                } while (visible <= 100 && Time.realtimeSinceStartup < deadline);
                Assert.Greater(visible, 100, "Configured sample did not render IMM content.");
                Assert.IsNotNull(ImmRenderGraphSession.Current);
                Assert.Greater(ImmRenderGraphSession.Current.Transport.RenderSubmissions, 0UL);
                Debug.Log("[IMM_URP_SAMPLE_TEST] PASS configured sample renders through its serialized renderer feature.");
            }
            finally
            {
                // Destroy scene objects first so native shutdown precedes releasing the target.
                foreach (var root in scene.GetRootGameObjects()) Object.DestroyImmediate(root);
                unload = SceneManager.UnloadSceneAsync(scene);
                QualitySettings.renderPipeline = previousPipeline;
                target.Release();
                Object.DestroyImmediate(target);
            }
            yield return unload;
            Assert.IsNull(ImmRenderGraphSession.Current);
        }

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
            GameObject secondCameraObject = null;
            RenderTexture secondTarget = null;
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
                foreach (int samples in new[] { 1, 2, 4, 8 })
                {
                    target.Release();
                    target.antiAliasing = samples;
                    Assert.IsTrue(target.Create());
                    pipeline.msaaSampleCount = samples;
                    Assert.AreEqual(samples, target.antiAliasing);
                    occluderMaterial.SetFloat("_Surface", 0);
                    occluderMaterial.SetFloat("_ZWrite", 1);
                    occluderMaterial.SetFloat("_SrcBlend", (float)BlendMode.One);
                    occluderMaterial.SetFloat("_DstBlend", (float)BlendMode.Zero);
                    occluderMaterial.DisableKeyword("_SURFACE_TYPE_TRANSPARENT");
                    occluderMaterial.renderQueue = (int)RenderQueue.Geometry;
                    occluder.transform.position = bounds.center + Vector3.back * radius * 2;
                    for (int frame = 0; frame < 3; ++frame) yield return null;
                    Assert.AreEqual(0, CountVisiblePixels(target, null), "IMM ignored the nearer Unity opaque depth.");
                    occluder.transform.position = bounds.center + Vector3.forward * radius * 2;
                    for (int frame = 0; frame < 3; ++frame) yield return null;
                    Assert.Greater(CountVisiblePixels(target, null), 100, "IMM failed to render in front of the farther Unity opaque depth.");
                    Debug.Log("[IMM_URP_READBACK] PASS Unity opaque depth occludes IMM only when nearer.");
                    // Submit the same black surface after IMM in URP's transparent pass.
                    // Alpha one makes missing native depth writes erase the IMM image.
                    occluderMaterial.SetFloat("_Surface", 1);
                    occluderMaterial.SetFloat("_ZWrite", 0);
                    occluderMaterial.SetFloat("_SrcBlend", (float)BlendMode.SrcAlpha);
                    occluderMaterial.SetFloat("_DstBlend", (float)BlendMode.OneMinusSrcAlpha);
                    occluderMaterial.EnableKeyword("_SURFACE_TYPE_TRANSPARENT");
                    occluderMaterial.renderQueue = (int)RenderQueue.Transparent;
                    for (int frame = 0; frame < 3; ++frame) yield return null;
                    Assert.Greater(CountVisiblePixels(target, null), 100, "Farther Unity transparent geometry overwrote IMM: native depth was not preserved.");
                    occluder.transform.position = bounds.center + Vector3.back * radius * 2;
                    for (int frame = 0; frame < 3; ++frame) yield return null;
                    Assert.AreEqual(0, CountVisiblePixels(target, null), "Nearer Unity transparent geometry failed to cover IMM.");
                    Debug.Log("[IMM_URP_READBACK] PASS IMM depth occludes farther Unity transparent geometry.");
                    Debug.Log($"[IMM_URP_READBACK] PASS colour/depth composition at {samples} samples.");
                }
                camera.orthographic = true;
                camera.orthographicSize = radius * 1.5f;
                camera.farClipPlane = radius * 2000;
                camera.transform.position = bounds.center + Vector3.back * radius * 1000;
                occluder.transform.position = bounds.center + Vector3.forward * radius * 2;
                for (int frame = 0; frame < 3; ++frame) yield return null;
                Assert.Greater(CountVisiblePixels(target, null), 100, "Distant orthographic IMM content was culled or lost its depth.");
                occluder.transform.position = bounds.center + Vector3.back * radius * 2;
                for (int frame = 0; frame < 3; ++frame) yield return null;
                Assert.AreEqual(0, CountVisiblePixels(target, null), "Nearer transparent surface did not occlude orthographic IMM.");
                Debug.Log("[IMM_URP_READBACK] PASS distant orthographic rendering and depth composition.");
                camera.orthographic = false;
                camera.farClipPlane = radius * 8;
                camera.transform.position = bounds.center + Vector3.back * radius * 3;
                occluder.SetActive(false);
                secondTarget = new RenderTexture(target.descriptor);
                secondTarget.Create();
                secondCameraObject = new GameObject("IMM URP second camera", typeof(Camera), typeof(ImmCamera));
                var secondCamera = secondCameraObject.GetComponent<Camera>();
                secondCamera.CopyFrom(camera);
                secondCamera.targetTexture = secondTarget;
                secondCamera.transform.position = camera.transform.position;
                secondCamera.transform.rotation = camera.transform.rotation * Quaternion.Euler(0, 180, 0);
                for (int frame = 0; frame < 3; ++frame) yield return null;
                Assert.Greater(CountVisiblePixels(target, null), 100, "Second camera changed the first camera's document view.");
                Assert.AreEqual(0, CountVisiblePixels(secondTarget, null), "Second camera reused the first camera's view or target.");
                secondCamera.transform.rotation = camera.transform.rotation;
                cameraObject.GetComponent<ImmCamera>().enabled = false;
                for (int frame = 0; frame < 3; ++frame) yield return null;
                Assert.AreEqual(0, CountVisiblePixels(target, null), "Camera continued rendering IMM after opt-out.");
                Assert.Greater(CountVisiblePixels(secondTarget, null), 100, "Opting out one camera stopped another opted-in camera.");
                // Recreate the attachment in place: the managed RenderTexture stays
                // the same while Unity's native colour/depth resources are replaced.
                secondTarget.Release();
                secondTarget.width = 192;
                secondTarget.height = 128;
                Assert.IsTrue(secondTarget.Create());
                for (int frame = 0; frame < 3; ++frame) yield return null;
                Assert.Greater(CountVisiblePixels(secondTarget, null), 100, "IMM lost its target after attachment resize/recreation.");
                Assert.AreEqual(0, CountVisiblePixels(target, null), "Resizing another camera changed the opted-out target.");
                Debug.Log("[IMM_URP_READBACK] PASS resized and recreated attachment renders without stale native handles.");
                ulong submitted = ImmRenderGraphSession.Current.Transport.RenderSubmissions;
                secondCameraObject.GetComponent<ImmCamera>().enabled = false;
                for (int frame = 0; frame < 3; ++frame) yield return null;
                Assert.AreEqual(submitted, ImmRenderGraphSession.Current.Transport.RenderSubmissions);
                Assert.AreEqual(0, CountVisiblePixels(secondTarget, null), "Second camera continued rendering IMM after opt-out.");
                Debug.Log("[IMM_URP_READBACK] PASS cameras retain independent views, targets and opt-in state.");
                yield return ImmRenderGraphValidation.VerifyStereoPacket(camera, radius);
                submitted = ImmRenderGraphSession.Current.Transport.RenderSubmissions;
                int unloadingDocument = document.DocumentId;
                manager.UnloadDocument(document);
                deadline = Time.realtimeSinceStartup + 30;
                while (ImmNativePlugin.IsDocumentActive(unloadingDocument) && Time.realtimeSinceStartup < deadline)
                    yield return null;
                Assert.IsFalse(ImmNativePlugin.IsDocumentActive(unloadingDocument), "Document unload stalled with no opted-in cameras.");
                Assert.AreEqual(submitted, ImmRenderGraphSession.Current.Transport.RenderSubmissions,
                    "Unloading required an unexpected camera draw.");
                Debug.Log("[IMM_URP_READBACK] PASS camera-free document unload completed.");
                var loadingDocument = manager.LoadDocument(Path.Combine(Application.streamingAssetsPath, "sample1.imm"));
                Assert.IsNotNull(loadingDocument);
                int loadingDocumentId = loadingDocument.DocumentId;
                Assert.IsTrue(ImmNativePlugin.IsDocumentActive(loadingDocumentId));
                Assert.AreNotEqual(ImmDocument.LoadingState.Loaded, loadingDocument.GetStateInfo().Loading);
                manager.UnloadDocument(loadingDocument);
                deadline = Time.realtimeSinceStartup + 30;
                while (ImmNativePlugin.IsDocumentActive(loadingDocumentId) && Time.realtimeSinceStartup < deadline)
                    yield return null;
                Assert.IsFalse(ImmNativePlugin.IsDocumentActive(loadingDocumentId), "Deferred unload stalled during loading without a camera.");
                Assert.AreEqual(submitted, ImmRenderGraphSession.Current.Transport.RenderSubmissions);
                Debug.Log("[IMM_URP_READBACK] PASS deferred camera-free unload completed during loading.");
                manager.Shutdown();
                Assert.IsNull(ImmRenderGraphSession.Current);
            }
            finally
            {
                Object.DestroyImmediate(occluder);
                Object.DestroyImmediate(occluderMaterial);
                Object.DestroyImmediate(secondCameraObject);
                if (secondTarget != null)
                {
                    secondTarget.Release();
                    Object.DestroyImmediate(secondTarget);
                }
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
