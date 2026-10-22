using System;
using ImmPlayer;
using Result = ImmPlayer.ImmUnityRenderReadiness.Result;

static void Require(bool ok, string message) { if (!ok) throw new InvalidOperationException(message); }
Require(ImmUnityRenderReadiness.Evaluate(0, 5, 1, 1) == Result.Ready, "Positive paint and triangles must be ready.");
Require(ImmUnityRenderReadiness.Evaluate(0, 5, 0, 100) == Result.Pending, "Triangles alone cannot pass.");
Require(ImmUnityRenderReadiness.Evaluate(1, 5, 10, 0) == Result.Pending, "Paint calls alone cannot pass.");
Require(ImmUnityRenderReadiness.Evaluate(2, 5, 0, 100) == Result.Pending, "Separate observations cannot combine counters.");
Require(ImmUnityRenderReadiness.Evaluate(4.999, 5, 10, 100) == Result.Ready, "Delayed valid observation before deadline must pass.");
Require(ImmUnityRenderReadiness.Evaluate(5, 5, 0, 0) == Result.TimedOut, "Missing paint must time out.");
Require(ImmUnityRenderReadiness.Evaluate(5, 5, 10, 100) == Result.TimedOut, "An observation at the expired deadline cannot pass.");
Require(ImmUnityRenderReadiness.Evaluate(10, 5, -1, -1) == Result.TimedOut, "Failure must remain terminal past deadline.");
Console.WriteLine("[IMM_UNITY_RENDER_READINESS_TEST] strict same-observation paint/triangle gate, delayed readiness and timeout passed.");
