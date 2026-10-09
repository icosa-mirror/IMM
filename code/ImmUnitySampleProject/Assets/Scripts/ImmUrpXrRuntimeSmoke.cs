using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using ImmPlayer;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;
using UnityEngine.XR;

/// <summary>Opt-in hardware probe for the built URP/OpenXR sample, not the Editor.</summary>
public sealed class ImmUrpXrRuntimeSmoke : MonoBehaviour
{
    private readonly List<XRDisplaySubsystem> displays = new List<XRDisplaySubsystem>();
    // Completion listeners run inside measured callbacks; avoid fixture growth there.
    private readonly HashSet<int> submittedFrames = new HashSet<int>(256);
    private readonly int[] allocationSamples = new int[3];
    private readonly int[] allocatingCallbacks = new int[3];
    private ImmUrpSample sample;
    private int firstFrame;
    private bool observing;
    private string failure;
    private string runId;

    private IEnumerator Start()
    {
        var arguments = Environment.GetCommandLineArgs();
        if (Array.IndexOf(arguments, "-immUrpXrSmoke") < 0) yield break;
        int runIndex = Array.IndexOf(arguments, "-immUrpXrRunId");
        var correlation = Guid.NewGuid();
        if (runIndex >= 0 && (runIndex + 1 >= arguments.Length ||
            !Guid.TryParse(arguments[runIndex + 1], out correlation)))
        {
            Fail("Invalid -immUrpXrRunId.");
            yield break;
        }
        runId = correlation.ToString("N");
        Debug.Log($"[IMM_URP_XR_SMOKE] BEGIN run={runId}");
        Application.runInBackground = true;
        if (!ImmRenderingDiagnostics.CalibrateManagedAllocationMeasurement())
        {
            Fail("Unity GC.Alloc recorder failed calibration.");
            yield break;
        }
        sample = GetComponent<ImmUrpSample>();
        int captureIndex = Array.IndexOf(arguments, "-immUrpXrCapturePath");
        if (sample == null || captureIndex < 0 || captureIndex + 1 >= arguments.Length)
        {
            Fail("Supply -immUrpXrCapturePath and attach the probe to the URP sample.");
            yield break;
        }
        string capturePath = Path.GetFullPath(arguments[captureIndex + 1]);
        Directory.CreateDirectory(Path.GetDirectoryName(capturePath));
        if (File.Exists(capturePath)) File.Delete(capturePath);
        var expectedApi = Application.platform == RuntimePlatform.Android
            ? GraphicsDeviceType.Vulkan : GraphicsDeviceType.Direct3D12;
        if (SystemInfo.graphicsDeviceType != expectedApi)
        {
            Fail($"Expected {expectedApi}, got {SystemInfo.graphicsDeviceType}.");
            yield break;
        }
        float deadline = Time.realtimeSinceStartup + 120;
        XRDisplaySubsystem display = null;
        while (Time.realtimeSinceStartup < deadline)
        {
            displays.Clear();
            SubsystemManager.GetInstances(displays);
            display = displays.Find(candidate => candidate.running);
            if (display != null && XRSettings.isDeviceActive && sample.DocumentCamera.stereoEnabled &&
                sample.Document != null && sample.Document.GetStateInfo().Loading == ImmDocument.LoadingState.Loaded)
                break;
            yield return null;
        }
        if (display == null || !display.running || sample.Document == null ||
            sample.Document.GetStateInfo().Loading != ImmDocument.LoadingState.Loaded ||
            !XRSettings.isDeviceActive || !sample.DocumentCamera.stereoEnabled)
        {
            Fail("Timed out waiting for a running XR display, stereo camera and loaded document.");
            yield break;
        }
        firstFrame = Time.frameCount + 1;
        observing = true;
        bool measuring = false;
        ImmRenderingDiagnostics.SubmissionCompleted += OnSubmission;
        while (submittedFrames.Count < 120 && failure == null && Time.realtimeSinceStartup < deadline)
        {
            if (!measuring && submittedFrames.Count >= 32)
            {
                measuring = true;
                ImmRenderingDiagnostics.ManagedAllocationMeasured += OnManagedAllocation;
            }
            if (!display.running || display.GetRenderPassCount() != 1)
            {
                failure = "Expected one running XR display render pass.";
                break;
            }
            display.GetRenderPass(0, out var pass);
            if (pass.GetRenderParameterCount() != 2)
            {
                failure = "Expected two views in the XR display render pass.";
                break;
            }
            yield return null;
        }
        observing = false;
        ImmRenderingDiagnostics.SubmissionCompleted -= OnSubmission;
        ImmRenderingDiagnostics.ManagedAllocationMeasured -= OnManagedAllocation;
        if (failure != null || submittedFrames.Count < 120)
        {
            Fail(failure ?? $"Timed out with {submittedFrames.Count}/120 attributed stereo frames.");
            yield break;
        }
        for (int callback = 0; callback < allocationSamples.Length; ++callback)
        {
            if (allocationSamples[callback] < 64 || allocatingCallbacks[callback] != 0)
            {
                Fail($"Managed allocation check failed: callback={(ImmRenderCallback)callback} samples={allocationSamples[callback]} allocatingCallbacks={allocatingCallbacks[callback]}.");
                yield break;
            }
        }
        Debug.Log($"[IMM_URP_XR_SMOKE] PASS zero managed allocations in warmed stereo IMM pass callbacks (64 samples minimum). run={runId}");
        var depthProbe = VerifyStereoDepthComposition(display, Path.GetDirectoryName(capturePath));
        try
        {
            while (true)
            {
                bool next = false;
                Exception probeError = null;
                try { next = depthProbe.MoveNext(); }
                catch (Exception error) { probeError = error; }
                if (probeError != null)
                {
                    Fail($"Stereo depth probe failed: {probeError.Message}");
                    yield break;
                }
                if (!next) break;
                yield return depthProbe.Current;
            }
        }
        finally { (depthProbe as IDisposable)?.Dispose(); }
        // Capture after allocation measurement; write synchronously so Android's
        // asynchronous screenshot writer cannot race the probe's successful exit.
        yield return new WaitForEndOfFrame();
        var capture = ScreenCapture.CaptureScreenshotAsTexture();
        if (capture == null)
        {
            Fail("Could not read the XR mirror.");
            yield break;
        }
        string captureFailure = null;
        try { File.WriteAllBytes(capturePath, capture.EncodeToPNG()); }
        catch (Exception error) { captureFailure = error.Message; }
        finally { Destroy(capture); }
        if (captureFailure != null)
        {
            Fail($"Could not write the XR mirror: {captureFailure}");
            yield break;
        }
        if (!File.Exists(capturePath) || new FileInfo(capturePath).Length == 0)
        {
            Fail("XR mirror capture was not written.");
            yield break;
        }
        Debug.Log($"[IMM_URP_XR_SMOKE] PASS api={expectedApi} attributedStereoFrames={submittedFrames.Count} nativeSceneEventsPerFrame=1 displayPasses=1 views=2 capture={capturePath} run={runId}");
        // This proves submission/layout, not per-eye visual correctness or GPU draw counts.
        Application.Quit(0);
    }

    private IEnumerator VerifyStereoDepthComposition(XRDisplaySubsystem display, string captureRoot)
    {
        var shader = Resources.Load<Shader>("ImmUrpDepthProbe");
        Require(shader != null && shader.isSupported, "Stereo depth probe shader is unavailable.");
        var material = new Material(shader);
        var mesh = new Mesh
        {
            vertices = new[]
            {
                new Vector3(-.5f, -.5f, 0), new Vector3(.5f, -.5f, 0),
                new Vector3(-.5f, .5f, 0), new Vector3(.5f, .5f, 0)
            },
            triangles = new[] { 0, 2, 1, 2, 3, 1 }
        };
        mesh.RecalculateBounds();
        var quad = new GameObject("IMM XR stereo depth probe", typeof(MeshFilter), typeof(MeshRenderer));
        quad.GetComponent<MeshFilter>().sharedMesh = mesh;
        quad.GetComponent<MeshRenderer>().sharedMaterial = material;
        var bounds = sample.Document.GetBoundingBox();
        float radius = Mathf.Max(bounds.extents.magnitude, .1f);
        quad.transform.localScale = Vector3.one * radius * 4;
        string captureDirectory = Path.Combine(captureRoot, $"imm-urp-xr-{runId}");
        Directory.CreateDirectory(captureDirectory);
        try
        {
            long[] unblendedBrightness = new long[2];
            for (int phase = 0; phase < 7; ++phase)
            {
                bool transparent = phase == 3 || phase == 4;
                bool alphaTest = phase >= 5;
                bool near = phase == 1 || phase >= 4;
                quad.SetActive(phase != 0);
                material.renderQueue = (int)(transparent ? RenderQueue.Transparent : alphaTest ? RenderQueue.AlphaTest : RenderQueue.Geometry);
                material.SetFloat("_Opacity", transparent ? 0.5f : 1);
                material.SetFloat("_AlphaCutoff", phase == 5 ? 2 : alphaTest ? 0.5f : 0);
                material.SetFloat("_ZWrite", transparent ? 0 : 1);
                quad.transform.SetPositionAndRotation(bounds.center + sample.DocumentCamera.transform.forward *
                    (near ? -2 : 2) * radius, sample.DocumentCamera.transform.rotation);
                for (int frame = 0; frame < 3; ++frame) yield return null;
                var visible = new int[2];
                var brightness = new long[2];
                var capture = CaptureStereoEyes(display, captureDirectory, phase, visible, brightness);
                try { while (capture.MoveNext()) yield return capture.Current; }
                finally { (capture as IDisposable)?.Dispose(); }
                for (int eye = 0; eye < 2; ++eye)
                {
                    bool occluded = phase == 1 || phase == 6;
                    Require(occluded ? visible[eye] == 0 : visible[eye] > 100,
                        $"Stereo depth composition failed: phase={phase} transparent={transparent} alphaTest={alphaTest} near={near} eye={eye} visible={visible[eye]}.");
                    if (phase == 2) unblendedBrightness[eye] = brightness[eye];
                    // Headset pose can move between readbacks; reject the much larger
                    // darkening caused by an incorrectly blended half-opacity quad.
                    if (phase == 3)
                        Require(Math.Abs(brightness[eye] - unblendedBrightness[eye]) <= unblendedBrightness[eye] / 10 + 1,
                            $"Stereo transparent geometry behind IMM changed colour: eye={eye}.");
                    if (phase == 4)
                        Require(brightness[eye] > unblendedBrightness[eye] / 3 && brightness[eye] < unblendedBrightness[eye] * 9 / 10,
                            $"Stereo transparent blending failed: eye={eye} baseline={unblendedBrightness[eye]} blended={brightness[eye]}.");
                }
                Debug.Log($"[IMM_URP_XR_SMOKE] depth phase={phase} transparent={transparent} near={near} visible={visible[0]},{visible[1]} run={runId}");
            }
            Debug.Log($"[IMM_URP_XR_SMOKE] PASS stereo Unity opaque and transparent depth composition. run={runId}");
        }
        finally
        {
            quad.SetActive(false);
            Destroy(quad); Destroy(mesh); Destroy(material);
        }
    }

    private IEnumerator CaptureStereoEyes(XRDisplaySubsystem display, string directory, int phase, int[] visible, long[] brightness)
    {
        yield return new WaitForEndOfFrame();
        Require(display.running && display.GetRenderPassCount() == 1, "XR display changed during stereo capture.");
        var source = display.GetRenderTextureForRenderPass(0);
        Require(source != null && source.dimension == TextureDimension.Tex2DArray && source.volumeDepth == 2,
            "XR pass does not expose its two-layer colour target for readback.");
        var descriptor = source.descriptor;
        descriptor.depthStencilFormat = GraphicsFormat.None;
        descriptor.msaaSamples = 1;
        descriptor.bindMS = false;
        descriptor.memoryless = RenderTextureMemoryless.None;
        descriptor.useDynamicScale = false;
        descriptor.vrUsage = VRTextureUsage.None;
        var snapshot = new RenderTexture(descriptor);
        var commands = new CommandBuffer { name = "IMM XR per-eye depth evidence" };
        try
        {
            Require(snapshot.Create(), "Could not create the XR readback snapshot.");
            if (source.antiAliasing > 1) commands.ResolveAntiAliasedSurface(source, snapshot);
            else commands.CopyTexture(source, snapshot);
            Graphics.ExecuteCommandBuffer(commands);
            var readback = AsyncGPUReadback.Request(snapshot, 0, TextureFormat.RGBA32);
            float deadline = Time.realtimeSinceStartup + 15;
            while (!readback.done && Time.realtimeSinceStartup < deadline) yield return null;
            Require(readback.done, "XR stereo target readback timed out.");
            Require(!readback.hasError, "XR stereo target readback failed.");
            for (int eye = 0; eye < 2; ++eye)
            {
                var pixels = readback.GetData<Color32>(eye);
                Require(pixels.Length == snapshot.width * snapshot.height, "Unexpected XR eye readback dimensions.");
                for (int pixel = 0; pixel < pixels.Length; ++pixel)
                {
                    int sum = pixels[pixel].r + pixels[pixel].g + pixels[pixel].b;
                    if (sum > 12) ++visible[eye];
                    brightness[eye] += sum;
                }
                var image = new Texture2D(snapshot.width, snapshot.height, TextureFormat.RGBA32, false, true);
                try
                {
                    image.SetPixelData(pixels, 0);
                    image.Apply(false, false);
                    byte[] encoded = image.EncodeToPNG();
                    Require(encoded != null && encoded.Length > 0, "Could not encode XR eye evidence.");
                    File.WriteAllBytes(Path.Combine(directory, $"depth-{phase}-eye-{eye}.png"), encoded);
                }
                finally { Destroy(image); }
            }
        }
        finally
        {
            commands.Dispose(); snapshot.Release(); Destroy(snapshot);
        }
    }

    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }

    private void OnSubmission(ImmSceneSubmission submission)
    {
        if (!observing || !ReferenceEquals(submission.Camera, sample.DocumentCamera) ||
            submission.FrameIndex < firstFrame) return;
        if (submission.Result != 0 || submission.ViewCount != 2 || submission.XrPassIndex != 0 ||
            submission.NativeSceneEvents != 1 || !submittedFrames.Add(submission.FrameIndex))
            failure = $"Invalid or duplicate stereo submission at frame {submission.FrameIndex}: result={submission.Result} views={submission.ViewCount} pass={submission.XrPassIndex} events={submission.NativeSceneEvents}.";
    }

    private void OnManagedAllocation(ImmManagedAllocation measurement)
    {
        int index = (int)measurement.Callback;
        ++allocationSamples[index];
        if (measurement.AllocationDetected) ++allocatingCallbacks[index];
    }

    private void OnDestroy()
    {
        ImmRenderingDiagnostics.SubmissionCompleted -= OnSubmission;
        ImmRenderingDiagnostics.ManagedAllocationMeasured -= OnManagedAllocation;
    }

    private void Fail(string message)
    {
        Debug.LogError($"[IMM_URP_XR_SMOKE] FAIL run={runId} {message}");
        Application.Quit(1);
    }
}
