using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using ImmPlayer;
using UnityEngine;
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
