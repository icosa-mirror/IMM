using ImmPlayer.Authoring;
using ImmPlayer.Exporter;
using UnityEngine;

namespace ImmPackageConsumer
{
    /// <summary>
    /// Compilation of this assembly proves that application code can consume the
    /// runtime package without referencing package tests or sample assemblies.
    /// </summary>
    public static class ImmPackageConsumerSmoke
    {
        public static string DescribeRuntime()
        {
            ImmAuthoringCapabilities capabilities = ImmAuthoringRuntime.Capabilities;
            return $"{capabilities.Platform} {capabilities.Architecture}: {capabilities.Features}";
        }

        public static ImmAuthoringResult CreateAndDisposeDocument()
        {
            ImmAuthoringResult<ImmAuthoringDocument> create = ImmAuthoringDocument.Create(
                ExportSequenceType.Animated,
                30,
                Color.black);
            if (!create.Succeeded)
                return create.WithoutValue();
            create.Value.Dispose();
            return ImmAuthoringResult.Success();
        }
    }
}
