using NUnit.Framework;
using UnityEngine;

namespace ImmPlayer.Tests
{
    public sealed class ImmCameraMatrixFrameGateTests
    {
        [Test]
        public void TwoCamerasCanSubmitInTheSameFrameWhileEachReusesItsMultipassPose()
        {
            var sceneCamera = new ImmCameraMatrixFrameGate();
            var xrCamera = new ImmCameraMatrixFrameGate();
            const int frame = 120;
            Matrix4x4 scenePose = Matrix4x4.Translate(new Vector3(8.0f, 0.0f, 0.0f));
            Matrix4x4 xrPose = Matrix4x4.Rotate(Quaternion.Euler(0.0f, 35.0f, 0.0f));
            Matrix4x4 submittedScenePose = Matrix4x4.zero;
            Matrix4x4 submittedXrPose = Matrix4x4.zero;

            Assert.That(TrySubmitPose(sceneCamera, frame, scenePose, ref submittedScenePose), Is.True);
            Assert.That(TrySubmitPose(xrCamera, frame, xrPose, ref submittedXrPose), Is.True);
            Assert.That(submittedScenePose, Is.EqualTo(scenePose));
            Assert.That(submittedXrPose, Is.EqualTo(xrPose));
            Assert.That(TrySubmitPose(sceneCamera, frame, Matrix4x4.identity, ref submittedScenePose), Is.False);
            Assert.That(TrySubmitPose(xrCamera, frame, Matrix4x4.identity, ref submittedXrPose), Is.False);
            Assert.That(submittedScenePose, Is.EqualTo(scenePose));
            Assert.That(submittedXrPose, Is.EqualTo(xrPose));
        }

        [Test]
        public void XrCameraAcceptsTheNextFramesChangedPose()
        {
            var xrCamera = new ImmCameraMatrixFrameGate();
            Matrix4x4 firstPose = Matrix4x4.Translate(new Vector3(0.0f, 1.6f, 0.0f));
            Matrix4x4 changedPose = Matrix4x4.TRS(
                new Vector3(0.2f, 1.7f, -0.1f),
                Quaternion.Euler(12.0f, 28.0f, 0.0f),
                Vector3.one);
            Matrix4x4 submittedPose = Matrix4x4.zero;

            Assert.That(TrySubmitPose(xrCamera, 120, firstPose, ref submittedPose), Is.True);
            Assert.That(TrySubmitPose(xrCamera, 120, changedPose, ref submittedPose), Is.False);
            Assert.That(submittedPose, Is.EqualTo(firstPose));
            Assert.That(TrySubmitPose(xrCamera, 121, changedPose, ref submittedPose), Is.True);
            Assert.That(submittedPose, Is.EqualTo(changedPose));
        }

        [Test]
        public void MonoAndDiagnosticModesCanSubmitEveryCallback()
        {
            var monoCamera = new ImmCameraMatrixFrameGate();
            var diagnosticCamera = new ImmCameraMatrixFrameGate();
            const int frame = 120;

            Assert.That(monoCamera.TryBeginSubmission(frame, false, false), Is.True);
            Assert.That(monoCamera.TryBeginSubmission(frame, false, false), Is.True);
            Assert.That(diagnosticCamera.TryBeginSubmission(frame, true, true), Is.True);
            Assert.That(diagnosticCamera.TryBeginSubmission(frame, true, true), Is.True);
        }

        private static bool TrySubmitPose(
            ImmCameraMatrixFrameGate gate,
            int frame,
            Matrix4x4 pose,
            ref Matrix4x4 submittedPose)
        {
            if (!gate.TryBeginSubmission(frame, true, false))
                return false;

            submittedPose = pose;
            return true;
        }
    }
}
