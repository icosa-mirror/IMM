using UnityEngine;
using N = ImmPlayer.ImmLiveEditNative;

namespace ImmPlayer
{
    public sealed partial class ImmLiveEditPanel
    {
        private const uint ClockPreview = 256;
        private bool previewCameraSaved;
        private Vector3 mainCameraPosition;
        private Quaternion mainCameraRotation;
        private float mainCameraNear, mainCameraFar;
        private bool TimelinePreviewActive => timelineClockValid && (timelineClock.flags & ClockPreview) != 0;

        private bool PreviewCanEditLayer(int layer)
        {
            if (!TimelinePreviewActive) return true;
            return layer != timelineScope && treeNodes.TryGetValue(layer, out LayerNode node) &&
                node.Info.Type != ImmDocument.LayerType.Sound && InTimelineScope(node);
        }

        private bool PreviewCanEditSelected => PreviewCanEditLayer(selected);

        private void ControlTimelinePreview(int action, long ticks = 0)
        {
            if (!attached || pending != 0 || awaitingCommit) return;
            N.Check(N.ImmAuthoring_TimelinePreviewControl(Doc, timelineScope, action, ticks));
            if (action == 0 && view != null)
            {
                mainCameraPosition = view.transform.position; mainCameraRotation = view.transform.rotation;
                mainCameraNear = view.nearClipPlane; mainCameraFar = view.farClipPlane;
                previewCameraSaved = true;
            }
            if (action == 4) RestorePreviewCamera();
            UpdateTimelineClock();
            if (action == 0)
            {
                FramePreviewContents();
                if (!PreviewCanEditSelected && ScopeNode != null)
                    foreach (LayerNode child in ScopeNode.Children)
                        if (PreviewCanEditLayer(child.Info.Id))
                        { selected = child.Info.Id; Run(ReadSelected); break; }
            }
        }

        private void RestorePreviewCamera()
        {
            if (previewCameraSaved && view != null)
            {
                view.transform.SetPositionAndRotation(mainCameraPosition, mainCameraRotation);
                view.nearClipPlane = mainCameraNear; view.farClipPlane = mainCameraFar;
            }
            previewCameraSaved = false;
        }

        private void FramePreviewContents()
        {
            int previous = selected;
            try { selected = timelineScope; FrameSelected(); }
            finally { selected = previous; }
        }
    }
}
