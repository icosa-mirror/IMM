namespace ImmPlayer
{
    // Pure policy for the smoke's bounded observation of asynchronous native work.
    // Both counters must come from the same observation; this is not visual proof.
    internal static class ImmUnityRenderReadiness
    {
        internal enum Result { Pending, Ready, TimedOut }

        internal static Result Evaluate(double elapsedSeconds, double timeoutSeconds, int paintDrawCalls, int triangles)
        {
            if (elapsedSeconds >= timeoutSeconds) return Result.TimedOut;
            return paintDrawCalls > 0 && triangles > 0 ? Result.Ready : Result.Pending;
        }
    }
}
