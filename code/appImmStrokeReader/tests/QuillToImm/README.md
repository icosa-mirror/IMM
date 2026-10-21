# Quill to IMM conversion checks

The .NET 8 harness compiles the production reader, converter, authoring graph,
compiler and native declarations. Small Unity data stand-ins replace engine types;
the real Windows exporter and stroke-reader DLLs perform encode/read-back. This
does not replace Unity compilation or visual playback acceptance.

Windows checks also verify that loading the player plugin loads its packaged
Vorbis encoder without changing PATH. This covers delay-load resolution that
tests which preload dependency DLLs themselves can mask.

Implicit global imports are disabled. Test files declare their own namespaces so
production files must provide the imports required in Unity. This catches missing
imports such as the comic-export LINQ regression; Unity package/player CI remains
necessary for engine-specific compilation and behavior.

The suite also exercises immutable viewpoint preview bytes, native RGBA-to-JPEG
encoding, unchanged encoded JPEG export, structural comparison, and malformed-image
and image-budget output guards. Reader checks cover file/memory encoded JPEG/PNG,
version-1 BGR metadata, absent previews, query bounds and exact import/re-export
payloads. Stored Quill RGB/RGBA previews and malformed offset/format/pixel guards
are covered. `--compare-project` includes preview presence/dimensions/encoding/
color order, but does not compare decoded JPEG pixels. Fishgym's paired IMM has
a preview absent from its source project, so it currently fails that stricter check.

On Windows the suite also places complete/truncated PNG buffers immediately before
a no-access memory page. The old decoder crashes on that fixture; the bounded
reader rejects truncated prefixes and accepts the complete image. This check
tests actual reads beyond the supplied length rather than relying on adjacent
managed-array memory being inaccessible.

Offline picture checks exercise the five existing native projection enum values,
viewer-lock metadata, hierarchy/common properties, exact RGBA/alpha pixels, graph
transactions and immutable snapshots, encoded PNG/JPEG payload identity, malformed
input rejection and image-budget output guards. They do not establish projection
appearance or viewer-lock behavior in playback. Picture asset checks cover file
and memory queries, ABI/index/buffer/lifetime guards, exact encoded PNG/JPEG import/
export/import, hidden pictures and decoded 8-bit/1-bit grayscale. The real sample
picture and all three encoded sounds also pass exact import round trips. Quill
2D source checks cover picture-only RGB/RGBA/alpha, hidden exclusion, viewer-lock
metadata, import round trips, unsupported projections, invalid/truncated image
data, configured resource budgets and cancellation/output preservation.
`--compare-project` includes picture counts, properties and exact decoded pixels.

Sound construction checks cover immutable PCM16 ownership, spatial-setting ABI,
graph transactions/comparison, native mono/stereo/four-channel Opus and Ogg
serialization, encoded headers, common layer properties and failure/output guards.
Sound import checks cover file/memory queries, the 44/68-byte ABI, exact encoded
WAV/Vorbis/Opus passthrough, all spatial settings and legacy source duration/playing
fields, malformed-stream diagnostics, protected-page truncation and Ogg checksum
rejection. These do not establish audible playback, gapless timing or Quill source
conversion. Independent local decode results and codec sample-count
limits are recorded in content pipeline section 6.24.

1. Run the synthetic native regression suite from the repository root. Use a fresh
   artifact directory for each local run; Windows CI runs this with freshly built DLLs:

   ```powershell
   dotnet run --project code/appImmStrokeReader/tests/QuillToImm -- --self-test code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/x86_64/ImmUnityPlugin.dll code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/x86_64/ImmStrokeReader.dll artifacts/quill-converter-regressions
   ```

2. Convert a private project using the same runtime API. The output parent directory
   must exist. Add `--animated` for animated paint or `--with-viewpoints` for the
   project paint/animation/viewpoint/2D-picture entry. `--input-mib=N` sets an
   explicit project input budget; the default remains 256 MiB. `--allow-omissions` permits diagnosed
   partial output (legacy `--paint-only` also grants omission permission):

   ```powershell
   dotnet run --project code/appImmStrokeReader/tests/QuillToImm -- $projectFolder $outputImm code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/x86_64/ImmUnityPlugin.dll code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/x86_64/ImmStrokeReader.dll --paint-only
   ```

3. Compare the decoded paint/group subset with a local reference export. This includes
   every stored stroke/point, rather than skipping source strokes affected by codec
   point removal. Viewpoints, media, root keys and visual playback are outside this check:

   ```powershell
   dotnet run --project code/appImmStrokeReader/tests/QuillToImm -- --compare-paint $referenceImm $outputImm code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/x86_64/ImmStrokeReader.dll
   ```

   `--compare-animated-paint` additionally requires exact reader-exposed root
   properties/keys, layer keys, timeline metadata, point length/time and drawing
   scales. `--compare-project` also requires viewpoint transforms, tracking, volumes,
   locomotion and default selection. Previews/media and actual playback/navigation
   are outside these stored-data comparisons.

The suite covers native brush/visibility mapping, nonidentity group transform/pivot,
hierarchy, zero-width stroke tips through conversion and authoring import/re-export
(without invalid-width diagnostics), invalid zero-extent/negative-width strokes,
string-valued held frames,
X/Y/Z reflection preservation, unknown reflection/animation rejection, unknown source layers/brushes, explicit partial
output, source-file overwrite protection and preservation of existing output on failure.
The no-argument `QBinReader.ReadDrawingData()` brush fallback remains unchanged;
conversion explicitly requests strict brush validation.

Animated checks cover multiple/empty drawings, held frame maps, sequence-rate
normalization, animated visibility, nested timeline timing, offset/opacity/transform
keys, all five interpolation mappings and key reflections. Invalid timing/frame
maps and unsupported layer/root action keys must preserve existing output. Shared
exporter checks retain infinite/one/finite paint repeats independently of timeline
repeats. These checks inspect stored native data rather than playback behavior.

Root-property checks exercise snapshot/transaction isolation, structural comparison,
invalid-value rejection, native visibility read-back and exact binary root metadata
for opacity, timeline/duration/repeats, transforms/pivots and flips. The animated
converter also checks the observed legacy zero-root normalization. The public reader
does not yet expose root layer properties, so serialization checks explicitly parse
the writer's version-1 Root descriptor. Native root-property queries and static root
import/re-export are also checked, including root visibility, unchanged indexed
child-layer counts and null/bounds/unload handling. Default and nondefault root offsets
are retained. Root animation checks author 12 keys through root layer ID zero, exercise
ownership/lookup/transactions/replacement/removal/limits, and compare every native key
after export/import/re-export. All four action values and the 524/525-tick Play/Stop
separation survive; both files report Comic and three chapters. This does not validate
actual chapter navigation, playback controls or Quill source action mapping.

Viewpoint checks use a viewpoint-only synthetic project and exercise export flags,
gizmo visibility, eye/floor tracking, nondefault sphere/movement mask, hierarchy and
a non-first default. Unsupported preview omissions and default fallback require
explicit permission and mark partial output; strict rejection preserves existing
files. Unknown tracking values fail. The project API does not yet preserve previews.

For fixed-observer native paint playback comparisons, the viewer capture script
`code/appImmViewer/scripts/capture_windows_directx_baseline.ps1` accepts `-LockCamera`.
Set `Playback.PlayerSpawn.Location` to `Custom` in the supplied settings file and
provide the same camera for both files. This opts into
`IMM_VIEWER_VALIDATE_LOCK_CAMERA=1`, preventing automatic spawn selection from
moving the camera. It does not test navigation. Omit the switch for normal behavior;
the script explicitly resets the environment flag. Pixel results and native double
precision can differ even when reader-exposed float metadata compares exactly.

Reflection checks additionally cover native layer/pivot and transform-key flips,
known-flip import, structural comparison, invalid native enum rejection without
partial mutation, and the unchanged 32-byte legacy transform ABI. No-flip keys
continue through the original export entry point. The local Windows DLL must be
built from source containing the two additive reflection exports.

Root-query checks require the additive `StrokeReader_GetRootAnimationKey` export.
They verify implicit-root visibility exclusion, the public sample's Offset=0 key,
invalid index/document/null-output guards and unloaded-document lifetime. The
public fixture is the tracked `exampleImmFiles/sample1.imm`; private projects are
not required for these checks.

Private projects and generated artifacts remain local. Console prefixes are
`[IMM_QUILL_CONVERTER]`, `[IMM_QUILL_CONVERTER_TEST]` and
`[IMM_QUILL_CONVERTER_COMPARE]`.

Quill sound conversion checks cover sound-only mono/stereo PCM16, all observed
spatial settings, hidden retention, visibility/offset keys, omission of redundant
static-opacity keys and preservation of differing keys. Truncated/invalid qbin and
unknown settings reject without replacing output. Project comparisons include
sound settings/channels/legacy metadata and keys; codec/rate may differ and decoded
audio samples/playback are not compared. See shared plan section 6.29.

Hierarchy-reader checks cover a controlled native -> Quill project data round trip
with nested groups, local pivots/reflections, held frames, supported keys and five
interpolations, plus unmapped-channel and exact timing guards. Native re-encoding
succeeds, but strict codec equality is not accepted: the fixture's drawing scale
changes from 3 to 3.0000916 with both legacy and hierarchy readers. Existing exact
forward comparison gates remain unchanged. See shared plan section 6.34.

Offline PCM16 checks cover WAV8/16/24/32/float conversion, Vorbis/Opus decoding,
output byte caps, malformed packets, Opus gain and initial/end granules, complete
copy guards, lease release/unload/End lifetimes, and a fresh child process loading
only an isolated Windows reader package. Independent PCM comparisons and remaining
platform/playback limits are recorded in content pipeline section 6.35.
