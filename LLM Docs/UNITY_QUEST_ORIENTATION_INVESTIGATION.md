# Unity Vulkan RenderGraph orientation

## Proven mismatch and correction

1. Unity's Vulkan vertex compiler negates `SV_Position.y` after the projection multiply. A compiled Unity reference shader from Unity 6000.6.0f1 contains `OpFNegate` on the Y component of the output decorated `BuiltIn Position`. IMM's GLSL/SPIR-V shaders do not perform this compiler conversion.
2. On Quest, Unity's stereo GPU projection and the projection sent to IMM both had Y scale -0.8354789. Observed Unity eye viewports were positive-height 1680 x 1760, matching IMM's host-pass viewport. Matching matrices and viewports therefore did not establish matching final clip positions: the vertex-stage conversion was missing.
3. Apply the missing clip-Y reflection to the entire projection Y row in `ImmUnityVulkanClipProjection`, including asymmetric frustum terms, for head and both eye projections. Keep managed packets as Unity GPU projections. Do not introduce a second compensating flip in C#.
4. The reflection reverses projected triangle winding. Pair it with `frontIsCCW = true` in this adapter's renderer configuration. Changing only the projection made the scene upright but exposed incorrect faces; orientation and surface-facing are separate acceptance conditions.
5. This correction is confined to the Unity Vulkan RenderGraph adapter. Native-player and Godot Vulkan conventions, Unity's legacy Vulkan integration, Metal and DirectX rendering do not call this conversion. They must retain their existing conventions.

## How the regression escaped

1. The RenderGraph adapter supplied Unity GPU projections to IMM shaders without reproducing Unity's vertex compiler conversion, while explicitly using positive-height host-pass viewports. The legacy Vulkan integration instead uses IMM's negative-height viewport path.
2. The previous adapter GPU probe used positive-Y synthetic projections and checked coverage and horizontal stereo parallax. Vertically reflected output could pass, and the ordinary scene did not strongly distinguish front faces from back faces.
3. The managed attachment-identity heuristic also guessed projection flip state from render-target identifiers. Use RenderGraph's actual texture UV origin, as URP does, so attachment orientation is determined at execution time.
4. A separate guard rejected XR attachments marked dynamic-scale capable even when the camera used fixed resolution and both scale factors were one. Reject enabled or active dynamic resolution, rather than the capability flag alone.

## Regression protection

1. The production adapter GPU probe now receives a negative-Y Unity GPU projection and an off-center scene, and checks vertical placement in both eyes as well as coverage and stereo parallax.
2. `vulkan_unity_face_orientation_readback` uses the existing `face-orientation.imm` fixture. Correct culling preserves a dark open-shell interior; reversed winding exposes its orange backface sentinel. The test checks this independently of cyan exterior coverage.
3. Negative controls were run against the actual GPU readback. Omitting clip-Y conversion failed vertical placement. Restoring the old front-face setting failed the backface-sentinel check. The paired fix passed. All eight local Vulkan checks passed, including existing native multiview, model, pretessellated and fixture roundtrip checks.
4. Comments at the conversion and front-face setting explain the paired contract and the host boundary. Future changes must preserve both checks; do not choose a Y sign solely from whether the scene looks upright.

## Quest acceptance status

1. The corrected 4x MSAA Quest sample builds successfully. Its installed native library was pulled back and verified: SHA256 `e705559556006b80ae9263bb52c50e7f8fe0cf4070590ee93dda0278aab11ef2`.
2. Human confirmation of upright rendering, correct surface-facing, both eyes and head tracking is pending. The app remains stopped until the user is ready. A successful build or GPU fixture test does not substitute for that confirmation.
3. Off-headset eye readbacks and OS captures were black even with the proximity override. Those captures establish neither correct rendering nor a scene-rendering failure. No further capture is required to establish the compiler mismatch above.
4. Full cross-platform CI is required for the final source changes. The earlier green run 38286973696 predates this fix.
