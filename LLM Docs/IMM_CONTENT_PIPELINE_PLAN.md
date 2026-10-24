# IMM content pipeline: shared roadmap

Baseline source review: `ee5e02f3` on 2026-10-21; subsequent implementation and
validation revisions are recorded below.
This is the planning entry point for Quill conversion, IMM content extraction and
runtime authoring. C1 static conversion and initial C2 paint animation, root defaults
and viewpoint mapping use the shared offline graph/compiler. Native root-key/comic
construction and import round trips also exist. Quill source chapter/startup mapping,
broader playback/navigation, media, reverse conversion and live-authoring acceptance
remain open; none of these initial slices closes the full roadmap.

Current checkpoint (2026-10-23): full hosted run 39945112846 on 47a0240d
completed SUCCESS: 49 jobs passed, 22 skipped, no failures. Actual final artifact
12501160640 passes all 20 required hosted evidence rows, with zero invalid/missing
required rows and all 43 preserved manifests identifying the correct source.
The repeated FFmpeg setup failure is closed by consumer-only pinned provisioning,
verified on Linux and in both physical-device paths (6.74, 6.77). The native live
Loop/Offset milestone is included in that full regression source; its operation-
specific acceptance remains scoped to the documented Windows/native tests.
Hardware-only Unity Windows Vulkan and deferred headset evidence remain outside
hosted acceptance. Full M2, managed cutover and scene-wide Quill fidelity are open.

C5 milestones now include the extracted standalone library (026abe3b), production
CLI (c78d5206), and the prototype self-contained Windows CLI archive (6.75-6.78).
Those library/CLI/package sources and the ANSI host-trace correction are now
included in accepted full CI source 47a0240d. The shared application host
(e7f5c100), desktop window (3ec46758) and Unity editor window (6.82) are later
milestones: local checks and matched Editor/CLI conversion pass (6.85), and
full run 39963637687 on cab50dfa included these and all later native repeat
milestones, but is superseded for the reviewed native Editor licence lifecycle
correction (6.91). A corrected replacement is pending; full acceptance remains
unproved. Run 39954000025 was superseded for the test-platform correction (6.88-6.89).
Live UI input, clean-machine and dependency licence documentation acceptance remain open. Earlier numbered
checkpoints retain historical states; use this checkpoint and the latest entries
for current acceptance. Source chapter/startup mapping, broader timing/navigation,
media/reverse fidelity and revision-selected live save retain their own exit gates.

The C5 full run 39942955962 on fe5d91ed failed in the Windows archive verifier:
conversion and local-runtime checks passed, but implicit Python text decoding
corrupted the native ANSI trace's Unicode path. The failure is reproduced/fixed
under forced Python UTF-8 mode with the exact path checks preserved (6.79).
Cancellation of that superseded run was requested; fresh full validation of the
isolated correction passed in run 39945112846, including the Windows archive gate.
Desktop shared-host and form work passes local component/runtime checks
(6.80-6.81), outside the validation source. Live mouse/keyboard acceptance remains
open because the Computer Use Windows native pipe is unavailable.

Later local native repeat milestones (6.84, 6.86) add canonical static-paint
setter/getter publication, CPU guards and six rendered Windows DirectX cases.
They were included in superseded full CI source cab50dfa and remain required in
its corrected replacement. Linked production C-export delivery
also passes locally in a loaded-document WARP host (6.87). Unity/PInvoke live
delivery, managed mapping, broader backend and full M2 acceptance remain open.

## 1. Objective and document ownership

Provide reusable content reading, representation and writing so applications can
convert Quill projects, inspect/import IMM content, and author IMM documents.
Preserve ordinary playback performance and report unsupported or lossy mappings.

| Document | Owns |
|---|---|
| This roadmap | Shared capability status, dependencies, delivery order and cross-track decisions |
| [Quill converter findings](QUILL_TO_IMM_CONVERTER.txt) | Source corpus, reader fixes, Quill export investigation and converter-specific evidence |
| [Runtime authoring](IMM_RUNTIME_AUTHORING.md) | Offline authoring architecture and live editing contracts, milestones and acceptance |
| [Unity plugin reference](../docs/unity-plugins.md) | Plugin inventory, API/build/packaging reference; declarations remain authoritative |
| [Managed engine contract](../docs/runtime-authoring-engine-contract.md) | Detailed original managed contracts and historical evidence, subject to the authoring document's corrections |

1. Record implementation status and acceptance evidence separately. A parsed field,
   native format type, exported function and working consumer are different capabilities.
2. Keep shared work here; keep detailed contracts and investigation evidence in the
   documents above. Do not maintain another independent backlog for the same work.
3. Quill's own IMM export is the converter's behavioral reference. Binary identity
   is not required. Open Brush import restrictions do not define converter scope.
4. Live mutation and offline serialization have distinct requirements. Converter
   delivery does not wait for live SDK cutover; converter media support does not
   implicitly add live picture/sound/model mutation.

## 2. Current capability matrix

These are source-level assessments, not new platform or visual acceptance results.
"Existing" means implementation is present; mapping/consumer acceptance can remain open.

| Capability | Quill source reader | IMM reader / adapter | Managed offline authoring / export | Native live editing | Quill-to-IMM work |
|---|---|---|---|---|---|
| Paint and four brush sections | Existing | Existing point/brush extraction and SharpQuill mapping | Existing; zero-width tips accepted with nonzero stroke extent | Geometry replacement exists | Static mapping implemented; two local reference paint subsets match exactly after decode |
| Groups and hierarchy | Existing | Authoring reader exposes hierarchy; legacy adapter flattens, opt-in hierarchy mode retains local groups/pivots | Existing group/paint graph | Group hierarchy operations within limits | Map local structure without flattening |
| Static properties and pivots | Existing, including flip | Local/world properties exposed | Pivots and explicit X/Y/Z flips implemented with additive offline APIs; native platform acceptance pending | Canonical properties within existing live ABI limits | Static layer/pivot flip mapping implemented and tested locally |
| Drawings, held frames and repeats | Existing | Frame buffers/rates/repeats exposed; adapter consumes them | Existing drawings/frame references; one document frame rate | Stable-handle drawing/frame edits, atomic whole tables and static-paint Loop/Offset keys pass local Windows checks; canonical repeat property editing remains open | Preserve reuse; resolve mixed source rates explicitly |
| Layer animation and nested timing | Keys/timing parsed | Legacy adapter omits keys; hierarchy mode copies verified visibility/opacity/offset/transform channels | Supported keys/timeline properties; offline key flip preservation implemented | Visibility/opacity/transform keys plus static-paint stepped Loop/Offset keys; no new live flip contract | Map offsets, duration, repeats and interpolation against references |
| Root actions, chapters and startup | Source actions and nullable startup metadata parsed | Root keys/properties exposed and supported keys imported; Windows local round trips pass; cross-platform CI pending | Root keys and comic export with action markers implemented; stored chapter count round-trips; native root-stop/seek controls locally checked, Quill semantics remain open | No complete chapter authoring claim | Quill root-action/startup mapping still needs paired reference and playback evidence |
| Viewpoints / spawn areas | Tracking defaults and preview offsets parsed; project reader/writer retains raw RGB/RGBA previews | Spawn metadata, encoded previews and on-demand pixels exposed; hierarchy adapter maps spheres, previews and unique default paths, legacy flat mode skips them | Spawn graph/import/export/default selection and immutable preview assets; full common-property constructor with hidden startup visibility encoded as a key | Spawn edits within limits | Stored preview conversion implemented both directions; generated previews, exact JPEG parity and broader navigation acceptance open |
| Pictures and panoramas | Stored 2D and full-sphere equirectangular mono/stereo RGB/RGBA pictures mapped structurally; explicit panorama FOV and depth remain unsupported | Decoded pixels and original encoded picture assets exposed; optional adapter layers | Offline picture construction/import with shared immutable assets, projection and viewer locking | Outside live scope | 19 isolated 2D reference pictures match pixels/metadata; real stereo panorama pixels/metadata match source qbin; orientation, eye order and playback acceptance remain open |
| Sound | Project conversion maps observed mono/stereo PCM16, settings and keys; hidden sounds retained | Encoded WAV/Vorbis/Opus queries, metadata import and offline PCM16 decoding; hierarchy adapter maps supported mono/stereo sound | Offline PCM16 construction plus encoded sound import/passthrough | Outside live scope | 31 isolated paired sounds match settings/keys/timing metadata; audible timing, gain, looping and spatial playback remain open |
| Models | Reader skips Model/Unknown | No model mesh query/conversion surface in stroke reader | No model construction in current graph/Unity exporter ABI | Outside live scope | Conditional on suitable Quill reference evidence |
| File/memory output | SharpQuill writer supports a limited Quill-project subset | IMM-to-Quill entry point exists with losses below | Existing native IMM writer/compiler | Presented-revision save outstanding | Static converter reuses atomic file export; default rejects unsupported layers, explicit paint-only omission reports partial output |

Native IMM picture/sound structures and model mesh serialization are broader than
the managed graph and Unity `ImmExporter_*` ABI. Do not interpret the empty managed
cells as absent native format support. Model serialization is implemented in
`toImmersiveLayerModel.cpp` / `fromImmersiveLayerModel.cpp`; it does not establish
Quill model mapping or rendering parity.

## 3. Reuse assessment and implementation boundaries

The preferred first candidate is:

`Quill project -> SharpQuill source -> explicit mapping -> ImmAuthoringDocument -> ImmAuthoringCompiler -> IMM`

1. Reuse graph transactions, validation, hierarchy, drawings, frames, keys, bounds
   compilation and export diagnostics where their contracts fit. The first static
   and initial animated paint mappings now use this graph/compiler;
   animation acceptance, navigation and media remain outstanding.
2. Keep Quill semantics in the mapping layer. Extend the shared graph/exporter only
   when a demonstrated source case needs a reusable capability. Do not introduce
   another full document model or route conversion through Open Brush strokes.
3. Reflection preservation is implemented through an explicit graph flip enum and
   additive offline layer/pivot and transform-key exports. The legacy 32-byte native
   transform structure and exports are unchanged. Windows native build, X/Y/Z
   export/import/comparison checks and the existing two reference subsets pass.
   Native builds and packaged exports pass on all four platforms in run 39609583383.
   Unity/device acceptance remains pending; this does not
   extend the live mutation ABI's scope.
4. `ImmAuthoringCompiler` uses the snapshot's document frame rate. The existing
   importer reports mismatched layer rates. C0 references normalize paint rates to
   the sequence rate while preserving frame buffers (748 mixed-rate paint layers
   in the large case, plus Fishgym's 12-to-24 fps mapping). Retain the single-rate
   graph for the initial converter; do not add resampling or another rate API on
   the strength of the source rates alone. Playback acceptance remains pending.
5. Root actions are not equivalent to ordinary layer keys merely because enum names
   match. Complete chapter authoring and startup semantics need separate acceptance.
6. IMM paint encoding quantizes alpha/width, omits direction in always-visible mode,
   and derives point length/time. Compare against the target codec and Quill exports,
   rather than promise arbitrary-float losslessness.
7. Use offline file/memory export for conversion. Live commit/publication, GPU
   retirement and revision-selected save remain under the runtime authoring plan.
8. Place the mapping where it can reference both SharpQuill and the authoring
   graph. `ImmUnity.Runtime` already references `ImmStrokeReader.Runtime`; avoid
   adding a reverse dependency from the reader package to its authoring consumer.
   Check the chosen assembly/CLI packaging in C0 before adding the entry point.

## 4. Delivery tracks and dependencies

Current execution order (2026-10-23):

1. Fix known regressions and restore full CI; regressions interrupt later work.
2. Deliver the interactive live-edit lab on real IMM files before further batch
   expansion (user decision). Then finish the remaining native M2 breadth.
3. Implement presented-revision save/publish (M3).
4. Connect managed Unity transactions/preview to native live editing (M4).
5. Complete remaining backend/surface integration (M5).
6. Close remaining Quill conversion and reverse-conversion fidelity gaps.
7. Complete distribution dependency-licence/notices documentation.

Continue independent roadmap work while full CI runs. The dependency rationale
and initial conversion milestones below remain part of the original scope.

M2 implementation exit work (source audit, 2026-10-23):

Investigate the real-file rename Busy result recorded in 6.107 before claiming
real-world rename acceptance. Its cause has not been established.

1. Finish held-time key/cache correctness, including group/timeline visibility;
   static-paint visibility and opacity corrections are recorded in 6.104.
2. Broaden native atomic batches to represent managed transactions spanning keys,
   drawings, layers and hierarchy operations. Current CommitEdits still accepts
   restricted single-operation shapes; same-layer metadata accumulation is not
   general transaction support.
3. Retain the managed graph's timeline properties and supported channels through
   live mutation: timeline flag/duration/group repeat, DrawInTime/Action and
   applicable group/root timing controls. Timeline-containing subtree lifecycle
   must also be supported where the managed API permits it. Do not silently
   reduce the full cutover to a static-only subset. Component position/rotation/
   scale channels already reject in the managed API and are not new requirements.
4. Existing static group/paint/spawn lifecycle, names, pivots, common properties,
   reflections, paint repeat and frame tables are implemented within their stated
   limits. Their remaining cache/backend/latency acceptance is separate from new
   implementation work. Picture/model/sound/effect lifecycle is outside agreed
   live scope. M3 save and M4 managed preview cutover follow these M2 requirements.

| Milestone | Work and dependency | Exit evidence |
|---|---|---|
| C0: reference and reuse decision | Initial evidence and C1 scope recorded in converter findings: all 21 source inventories, four decoded pairs and one large metadata pair | Four-brush/static reuse supported; reflected-transform gap demonstrated; controlled re-export, rotational-opacity case and playback remain unverified |
| C1: static paint conversion | Implemented in d0e7ea31; exact decoded paint-subset matches for two cases; identical taperecorder DirectX capture; Windows regressions and Unity package compilation/tests pass; Unity player/device jobs pass; hosted full CI subsequently passed in 39645977224 and 39646549914 | Four brushes, hierarchy and static properties compared to Quill export; safe output and actionable diagnostics; native visual check |
| C1b: shared reflection preservation | Implemented for layer/pivot and offline transform keys, including importer/comparer; Windows reflection/converter CI, four-platform builds/exports and Unity graph tests pass; hosted full CI subsequently passed in 39645977224 and 39646549914 | Valid X/Y/Z retained without legacy ABI changes or live-scope expansion; platform exports and regression gates pass |
| C2: animation and navigation | Drawings/holds, keys, nested timing, root properties/actions in the graph, viewpoint/default mapping and stored previews implemented. Local metadata, headless evaluated paint/group/root comparisons and default-camera capture pass within recorded scope. Finite ancestor-loop and root stop/seek corrections locally verified. Source chapter/startup mapping, generated previews, precision differences and broader playback/navigation acceptance remain open | Matched timing/hold/navigation behavior on representative Quill reference exports and native playback |
| C3: media and coverage | Offline picture construction/import, source 2D and structural equirectangular mono/stereo conversion, and PCM16 sound construction implemented. Encoded sound queries/import and Quill PCM16 source mapping implemented. Panorama orientation/eye order and playback acceptance remain open | Panorama/picture appearance and audio playback/timing compared with Quill output; unsupported data reported |
| C4: unrepresented source cases | Investigate models, depth/cubemap and other cases only when suitable evidence exists | Explicit supported/rejected mappings; no inferred completeness from enums |
| C5: user-facing converter packaging | Standalone library, production CLI and prototype portable CLI have full hosted acceptance on 47a0240d. Desktop window, extracted form probe and Unity editor conversion/import pass locally (6.80-6.82), awaiting next full CI. Live input, clean-machine and dependency-documentation acceptance remain open; see 4.1 | Conversion works from a clean download without Unity or an installed .NET runtime, and from Unity, using the same conversion implementation |
| R1: reader and consumer contract | Zero-width tip import loss fixed; local and Windows CI export/import/export checks pass. Opt-in loss reporting and hierarchy/supported-key reconstruction implemented; remaining consumer coverage and fidelity acceptance stay open | Reader extraction and selected adapter output preserve intended content or report loss |
| R2: reverse conversion | Picture and PCM16 sound project serialization/read-back implemented; unsupported media rejected before writing. Opt-in hierarchy/supported keys and native sound decoding/mapping implemented; codec drift, model coverage and application acceptance remain open | Quill-project read-back and application checks for the declared subset; no silent media omission |
| A1: live slice acceptance | Existing authoring M0/M1; independent of C1 | Matched commit-to-presentation latency, failure and revision consistency evidence |
| A2: live breadth and save | Authoring M2 then M3 | Resource-owning layer lifecycle and revision-selected save accepted |
| A3: managed live cutover/platforms | Authoring M4/M5 after required native contracts | Managed preview uses accepted live path; per-operation/backend evidence |

1. Start with C0, then C1. C0 must inspect existing local source/reference resources
   before requesting new fixtures. The converter findings identify the local corpus
   and small reference candidates; avoid starting with the largest project.
2. R1 supports C1/C2 read-back checks, but a full reverse converter is not a C1
   prerequisite. Use the hierarchy-aware authoring reader when validating hierarchy;
   the flattened SharpQuill adapter cannot prove its preservation.
3. Shared graph/reader/API extensions should be focused changes with their own
   acceptance. Extend tests for real mapping boundaries, not another parallel framework.
4. C1 can proceed through existing offline export while A1-A3 remain separate.
   C3 does not change the declared live media exclusions.
5. C0 selected an unreflected static paint/group C1 slice through the existing graph
   and compiler. Use taperecorder and consul-room as the first data references;
   report out-of-profile content explicitly. No new graph type or rate API is
   required for that slice. C1b now supplies the additive reflection extension;
   its full platform acceptance is pending. See the converter findings for limits.
6. C2 distinguishes serialized base metadata from evaluated animation. The native
   evaluator uses the first transform key before its time, but hides a layer before
   its first visibility key, and interpolates transforms around their pivots.
   Preserve source base fields and keys separately; validate startup, key-boundary,
   midpoint, seek/restart and nested timing behavior before declaring animation
   parity. The converter findings record the source-level contract and remaining
   reference questions. The initial converter implementation is recorded in 6.5;
   this evaluator audit alone does not establish playback parity.
7. C2 root/startup reference inspection now has a root-key value query, locally
   verified on Windows; other-platform CI is pending. It uses the existing count,
   which excludes implicit default visibility but includes other properties and
   is not an action/chapter count. The public sample's counted key is Offset=0
   at time zero, not an action. No root construction or importer acceptance changed.
   The local Chapters source supplies Stop/MakeDefault cases. A matching Quill IMM
   export has not been located by project/name search; the user has been asked for
   a possible path. Continue other C2 work independently of that optional lookup.

### 4.1. C5 converter packaging plan (agreed 2026-10-22)

The converter must be usable both inside Unity and as a standalone application.
The standalone .NET 8 library and application hosts reuse the package C#
sources with Unity-type compatibility definitions. The portable distribution is
still awaiting its remaining acceptance. Python remains validation tooling, not a conversion
dependency. Native C++ exporter libraries remain part of the shipped runtime.

1. Establish a supported shared converter library and production command-line
   entry point outside the test project. Keep SharpQuill reading, mapping, graph
   construction and native export shared with Unity; do not fork conversion rules.
   Separate Unity-specific integration from the reusable conversion API. Retain
   existing regression coverage and explicit supported/omitted-content reporting.
   The standalone library prerequisite now exists at code/tools/ImmConversion
   with the shared source list and standalone compatibility definitions; the
   regression harness consumes it through a project reference (6.75). Production
   CLI entry point now exists at code/tools/ImmConvert with explicit omission/
   overwrite handling and JSON reports (6.76). Distributable/public API acceptance
   remains outstanding.
2. First user-facing packaging milestone: a portable Windows download. Provide
   project-folder selection and output selection, conversion progress, readable
   warnings/errors and a clear report of omitted content. Bundle the .NET runtime,
   native exporter and required codec dependencies, with applicable notices, so
   users need neither Unity nor developer tools. Preserve safe output replacement
   and require explicit opt-in for partial conversion. The implemented desktop
   toolkit is WinForms; see 6.81 for local component and extracted archive checks.
3. Include a supported CLI for batch conversion and CI, with documented arguments,
   reliable exit codes and optional machine-readable reports. The desktop and CLI
   entry points call the same library. Conversion must not require Python.
4. Provide a Unity "Convert Quill Project..." editor window using the shared
   converter, with project/output selection and diagnostics, and import the
   resulting IMM into the Unity project. This is offline conversion integration,
   separate from managed live scene-graph editing/cutover.
5. Acceptance: test the extracted portable download on a clean Windows environment
   with neither Unity nor .NET installed. Exercise representative supported source
   projects, paths with spaces/non-ASCII characters, invalid input, explicit
   omissions and output-preservation failure cases. Verify CLI exit/report behavior
   and a real Unity package consumer. Compare decoded outputs from both hosts on
   matched inputs; record artifact/version provenance and scope of visual checks.
   Add appropriate packaging/consumer checks to CI before declaring delivery.
6. C5 depends on the existing converter contract and regression gates; it does not
   require completing native live authoring. Packaging may deliver a clearly
   documented supported subset while C2/C3 fidelity work continues, but must not
   imply full Quill parity. Regressions and the pending live-key full CI validation
   retain priority. No packaging implementation or clean-machine acceptance is
   claimed by adding this plan.

## 5. Reader and reverse-conversion reference

1. The stroke reader is an independent document-data plugin. It reuses importer
   decoding through the optional `IStrokeCollector`; it does not need playback or
   rendering. Existing playback behavior must remain unchanged.
2. Packaging, managed bindings and Windows/macOS/Android/iOS build configurations
   already exist. The former February setup checklists are not outstanding goals.
   Current build configuration is not a fresh all-platform validation result.
3. `ReadImmAsSequence(path, includePictures, chapterIndex)` builds a flattened
   SharpQuill sequence. Paint includes decoded points, brush/rotational-opacity
   mapping, pivots and static properties, plus real frame buffers/rates/repeats.
   Chapter-specific loading reduces frame selection to the requested chapter.
4. Picture input is optional; spawn areas are skipped. Layer animation keys and
   original group/timeline structure are not reconstructed by this adapter. Its
   current brush conversion has a Cylinder fallback; full conversion must report
   unsupported mappings rather than silently treating that fallback as fidelity.
   `ReadImmAsSequenceWithReport` now returns this same legacy subset plus explicit
   source-layer omissions and hierarchy/key/root/chapter limitations (section 6.31).
   `ReadImmAsHierarchyWithReport` is a separate authoring-indexed mode retaining
   groups/local properties, verified key channels and supported sounds (sections
   6.34 and 6.37). Editor menus use this mode with a review and saved loss report
   (section 6.39). Legacy callers
   keep the flattened behavior.
5. `WriteImmAsQuillProject` invokes the bundled Quill writer. The writer now
   serializes RGB/RGBA picture children with supported projection metadata and
   rejects unsupported layer types or Gallery pictures before touching output
   (section 6.32). Legacy adapter reading still omits sound/hierarchy/key content;
   picture writing alone does not establish full reverse conversion.
6. The adapter includes file-load fallback through memory for paths that native
   file loading cannot open. Unity editor conversion commands and output location
   remain documented in the stroke-reader package README.
7. Open Brush's historical flattening policy is a consumer decision. This review
   did not inspect a current external Open Brush checkout; verify that consumer's
   actual code before declaring its current feature coverage or planning changes.

## 6. Source authority and validation

Numbered checkpoints record evidence at their stated revision. CI states in older
checkpoints are historical snapshots; the current checkpoint at the top and latest
numbered entry record their replacements and outstanding acceptance.

| Surface | Authoritative implementation |
|---|---|
| Quill parsing and project writing | [SharpQuill](../code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Runtime/ThirdParty/SharpQuill) |
| IMM read API and native storage | [main.cpp](../code/appImmStrokeReader/src/main.cpp), [strokeStore.h](../code/appImmStrokeReader/src/strokeStore.h) |
| IMM-to-SharpQuill adapter | [SharpQuillCompat.cs](../code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Runtime/SharpQuillCompat.cs) |
| Managed graph/compiler/import | [Authoring](../code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/Authoring) |
| Offline Unity export ABI | [ImmExporter.cs](../code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmExporter.cs), [main.cpp](../code/appImmUnity/src/main.cpp) |
| Native live ABI | [imm_authoring.h](../code/appImmUnity/src/imm_authoring.h) |

1. On 2026-10-21, reader synthetic regressions and the private corpus metadata/media
   check passed: 21 project trees, 34 PCM sounds and 35 picture settings. Native
   adapter regression passed against the bundled Windows DLL and `sample1.imm`:
   30 paint layers, with zero/one picture according to the requested mode.
2. These are reader/adapter data results. C1 additionally has static converter
   comparisons and one matched DirectX capture below. Animated Quill reference
   playback, general visual/audio parity and new live-edit acceptance remain
   unverified. C0 compared existing local candidate reference files at the
   data level; the converter findings record scope, skipped strokes and hashes.
3. Reproduction commands and reader logs are in the converter findings and
   `code/appImmStrokeReader/tests/SharpQuillReader/README.md` and
   `tests/SharpQuillAdapter/README.md` under the same directory. The adapter check
   runs in Windows build CI. C1 adds synthetic reader regressions to the same job;
   the reader corpus remains a local check, not a CI fixture. Metadata-only reading
   is implemented and its local locked-qbin, full-read and corpus regressions pass;
   its Windows reader and adapter gates passed in
   [run 39600334760](https://github.com/icosa-mirror/IMM/actions/runs/39600334760).
   The static converter and reader/adapter gates passed in
   [run 39602508539](https://github.com/icosa-mirror/IMM/actions/runs/39602508539),
   along with the Windows IMM content baseline.
   Full-validation run 39603480459 also passed Unity package import. Its GLES job
   failed Kotlin dependency resolution before compilation; Vulkan rendered but
   timed out at player frame 38 before the required frame-60 capture. Its URP
   evidence failure exposed the scanner defect below. That run is complete;
   fresh full validation after the scanner fix passed the affected Android and
   Unity jobs, but remains failed on the separate Godot iOS capacity issue below.
   No gate or timeout has been weakened.
5. C1 native visual evidence: taperecorder reference and converted paint-only output
   rendered at 960x540, DirectX, fixed timestep and player frame 60, with matched
   eye-level camera. Captures are pixel-identical, with 4 paint draw calls and 9962
   triangles. The existing local standalone viewer was used, not a fresh viewer
   build. Hashes, logs, settings and scope are recorded in the converter findings.
6. Preserve existing player regression and package/export gates when code changes.
   Live work must retain the authoring document's timing/failure/platform criteria
   and the web performance investigation procedure where applicable.

### 6.1 CI failure investigation and regression priority

Regressions always take priority over roadmap features. Diagnose failed gates,
resolve confirmed defects and verify the result before advancing feature work.
Do not interpret an infrastructure classifier or an unrelated passing job as
closure of a failed gate.

Historical status checked at 2026-10-21 14:36 UTC; superseded by the
replacement-run closure in section 6.26 (20:23 UTC):

| Gate | Evidence and current disposition | Required next evidence |
|---|---|---|
| Android standalone GLES, run 39603480459, job 119237704546 | Earlier Kotlin dependency-resolution failure is superseded by successful fresh job 119258531518 in run 39609583383: APK build, Firebase smoke, face-orientation and result gates pass. No dependency version or gate was changed. | Standalone gate verified; keep full engine/device acceptance separate. |
| Android standalone Vulkan, same run, job 119237705048 | Earlier frame-60 capture timeout is superseded by successful fresh job 119258531746 in run 39609583383. Firebase smoke, required capture, frame-sample validity, face-orientation and result gates pass. The 90-second deadline remains unchanged. | Standalone gate verified; keep Unity Vulkan/URP acceptance separate. |
| Unity Android URP, job 119246251194 | Confirmed scanner defect: the 26,465,675-byte log exceeded the old 25 MiB skip threshold despite all 11 markers being present. Fixed scanner regression and real-log recheck pass; fresh URP Firebase job 119265832698 and its frame-sample gate now pass. | Scanner repair verified in fresh CI. |
| Device/engine evidence reports | Both regenerated successfully in run 39609583383. | Verified; preserve independent successes. |
| Godot iOS, fresh run job 119254613104 | Original job and retry 119275413801 both cancelled with zero steps and no runner assigned. Both cite hosted macOS ARM64 capacity. Missing GodotIOSMetal then failed final aggregation, including retry report job 119282652963. | Automatic retry allowance exhausted. Full-run acceptance remains unresolved; needs an available suitable runner or explicit user direction for any further retry. No source fix or gate relaxation. |

1. GitHub's job-rerun API explicitly rejected the Vulkan request with HTTP 403:
   "The workflow run containing this job is already running." The GLES retry
   request was also rejected. Neither request executed a retry. The parent run
   was then active; neither retry executed. It has since completed with failure.
2. Earlier same-day run
   [39563833083](https://github.com/icosa-mirror/IMM/actions/runs/39563833083),
   Vulkan job 119106926359, passed the required frame-60 capture about 39 seconds
   after loading. Its native player library and sample are byte-identical to the
   failed run: `libappImmPlayer.so` SHA256
   `5cbf4909d5e541a1a91cda44fa3eb0d9ebfc11f1102c90b51a8060d229889f31`,
   `sample1.imm` SHA256
   `baa05c125ed0ca130760f1b76a8cde39a475f66a60e5b4e96644a1839b0b2095`.
   `libc++_shared.so` also matches. APK files and some DEX entries differ, so this
   establishes native-library/content identity, not whole-APK identity. No viewer,
   core/player, Android project or device-workflow source changed between
   `ee5e02f3` and `c339541b`. This supports timing variability rather than a new
   native-player code regression; it does not close the failure.
3. Local evidence is under ignored `artifacts/quill-c1-vulkan-device/` and
   `artifacts/quill-vulkan-prior-pass/`, with job logs and rerun responses under
   `artifacts/quill-*`. Full reflection validation
   [39607327739](https://github.com/icosa-mirror/IMM/actions/runs/39607327739)
   passed standalone Vulkan before cancellation following discovery of the scanner
   source defect. Cancellation is now confirmed complete. No broad acceptance
   claim is made.
4. For a confirmed source failure, cancel the remaining run, fix the cause and
   push a fresh full validation run. For an isolated infrastructure failure,
   preserve independent successes and retry only the affected job and dependents
   once. Never weaken a required gate or rerun indefinitely.
5. The scanner now streams log lines without excluding large files, both for
   required markers and bounded diagnostics. It retains rejection of markers
   quoted in failure/assertion messages. A synthetic log larger than 25 MiB proves
   markers at the start/end, negated-marker rejection and crash diagnostics near
   the end. The test failed before the fix and passes afterward. The real URP log
   recheck finds 11/11 required markers and no explicit test-result failures;
   `artifacts/quill-urp-scanner-recheck.json` retains that local evidence. This is
   a scanner repair, not permission to bypass CI or relax rendering acceptance.
   Because this is a source failure, fresh full validation supersedes the planned
   isolated retries of the original run.
6. Scanner fix `452d2629` is pushed. Fresh full-validation
   [run 39609583383](https://github.com/icosa-mirror/IMM/actions/runs/39609583383)
   completed its second attempt with the capacity-related failure below. Core
   Matrix Status passed, including the Firebase wrapper's new
   large-log regression (confirmed in the job log). Baseline content and package
   source layout gates also passed. Windows, Android, macOS and iOS native builds
   have passed. Windows job 119254613009 explicitly passed the X/Y/Z
   layer/pivot/key export/import/comparison and legacy ABI checks, the four-brush
   converter and output-preservation checks, and reader/adapter regressions.
   Unity packaging/export verification also passed on all four platforms; the
   existing iOS registration function is supplied by Objective-C glue. Subsequent
   Unity player/device jobs passed; final full-run acceptance remains blocked by
   Godot iOS capacity. Converter feature work remains held while independent
   reader inspection work proceeds with separate validation. These native build results do not
   establish device rendering or Unity integration acceptance.
7. Both standalone Android jobs in the fresh run are now complete and successful.
   Their job logs are retained locally as `artifacts/quill-scanner-fix-gles.log`
   and `artifacts/quill-scanner-fix-vulkan.log`. This closes the earlier standalone
   acceptance failures with new successful evidence; it does not rewrite the
   original failed run or prove full Unity/device acceptance. No timeout,
   dependency version or required rendering check was relaxed.
8. Unity Package Import job 119258531602 passed: 40 tests passed, zero failed,
   three skipped. `TransformReflectionsAreValidatedAndCompared` passed, as did all
   15 authoring document tests. The three skips explicitly require a Windows
   D3D12 graphics device and ran on the Linux host: configured-sample rendering,
   opted-in URP camera rendering, and shutdown/recreation session tests. These
   skips do not establish D3D12 runtime acceptance. The saved job log at
   `artifacts/quill-reflection-unity-import.log` contains the exact test results;
   subsequent Unity player/device jobs also passed.
9. Fresh full validation completed with only Godot iOS capacity cancellation and
   consequent final evidence aggregation failure. Windows DirectX/Vulkan, macOS
   Metal, Unity iOS, Android non-XR/URP device validation and the independent
   core/device/engine/GPU reports passed. The permitted isolated retry targeted
   Godot iOS and dependents, preserving independent passing jobs. Diagnostic
   evidence: `artifacts/quill-godot-ios-cancelled-annotations.json` and
   `artifacts/quill-scanner-fix-validation-evidence.log`. Full-run acceptance
   remains open after that retry also failed to acquire a runner.
10. Attempt 2 is now terminal with failure. Godot iOS retry job 119275413801 was
    again cancelled without acquiring a runner (zero steps); its annotation repeats
    the hosted macOS ARM64 capacity constraint. The final evidence job failed on
    the absent GodotIOSMetal artifact. No second automatic retry is permitted by
    the repository policy. Keep the original independent successes and unresolved
    full-run result distinct. Evidence: `artifacts/quill-godot-ios-retry-annotations.json`.

### 6.2 R1 zero-width importer fidelity checkpoint

1. The importer changed valid zero-width tips to 0.000001 despite the graph and
   codec supporting zero. A new native four-brush fixture failed before the fix
   with "Importer changed valid zero-width tips."
2. Width sanitization now accepts finite values greater than or equal to zero.
   Negative/nonfinite fallback behavior remains unchanged. Import preserves zero
   without an invalid-width diagnostic; re-export/native read-back preserves both
   zero tips and a positive middle width on all four brush sections.
3. The full local QuillToImm self-test passes, including existing reflection,
   converter, negative-width rejection and output-preservation checks. Evidence:
   `artifacts/quill-zero-tip-import-before-fix.log` and
   `artifacts/quill-zero-tip-import-fixed.log`. The expanded test runs in Windows
   build CI. This managed importer fix is subsequent to run 39609583383's source
   revision; that full run does not validate this new change. Its Windows job
   119277784654 in run 39616195981 passed the new import round-trip check plus
   existing reflection/converter regressions. Workflow 39616195981 subsequently
   completed successfully; that normal validation run does not replace the full
   run's still-missing Godot iOS evidence.
4. This closes one R1 data-loss defect locally, not the broader reader/consumer or
   reverse-conversion tracks. No player/rendering code or native binary changed.

### 6.3 C2 root-key inspection prerequisite

1. Added `StrokeReader_GetRootAnimationKey(docId, keyIndex, out key)`, using the
   existing `StrokeAnimationKey` layout and `StrokeDocumentInfo.rootAnimationKeyCount`.
   The implicit default visibility key remains excluded from both count and query.
   No existing public layout or count policy changed.
2. The reader retains root key values while capturing metadata; this adds no
   player/rendering code or per-frame work. The reference inspector now records
   those keys and chapter count separately from ordinary layers. It requires a
   reader binary built with the new export.
3. Windows native build, all QuillToImm self-tests and Windows export verification
   pass locally (40 reader P/Invokes). New checks cover a root with no non-default
   keys, the public sample's Offset=0 key, invalid index/document/null-output
   queries and unload lifetime. Existing zero-tip/reflection/converter checks pass.
   Evidence: `artifacts/quill-root-key-regressions.log`,
   `artifacts/quill-root-exports-windows.json`, `artifacts/quill-root-reader-build.log`.
4. Matching Windows DLLs are included with source. Other-platform compilation,
   packaging/export verification and CI for this addition remain pending. This
   diagnostic API does not implement chapter/root authoring, prove Quill startup
   mapping or close the unrelated Godot iOS capacity failure.
5. Native build ID is now `2026-10-21-ROOT-KEYS` to distinguish the new query from
   older binaries. The existing adapter check passes (30 paint layers, zero/one
   pictures). Read-only checks of the four earlier small pairs found only a root
   Offset=0 key at time zero in taperecorder, consul-room and Snoopy, and no counted
   root keys in Fishgym; all report one chapter. This does not supply a chapter-
   action reference or establish startup behavior. Reports remain under ignored
   `artifacts/quill-reference/*-root-metadata.json`.
6. Root-query validation run 39622051362 failed its Windows self-test because the
   new test referenced a locally present, untracked Godot sample copy. CI could not
   load that path. This is a test-source defect, not an infrastructure retry case.
   The test now requires the tracked `exampleImmFiles/sample1.imm`, checks file
   existence and load success explicitly, and retains the exact root-key assertions.
   The tracked file and local copy are byte-identical (SHA256
   `baa05c125ed0ca130760f1b76a8cde39a475f66a60e5b4e96644a1839b0b2095`).
   Local regressions pass with the tracked path; fresh full validation is required
   after this source fix. The older Godot iOS capacity failure remains unresolved.

7. The reference CLI also supports `--inspect-root` without a paired source. It
   performs a full native import and records input/library hashes, build ID,
   native load duration, root keys and chapters. Report/log paths cannot replace
   inputs or the native DLL; a public-sample overwrite rejection preserves the
   input hash. Public sample inspection returns one Offset key and one chapter.
   A local Bakery inspection returns 20 chapters and 21 root keys: 19 Play actions,
   one Stop action and one Offset key. Its first Play/Stop actions are separated
   by one tick (524/525). This is native-only evidence: no paired Quill source is
   available, so it does not establish source action mapping or chapter authoring.

### 6.4 Paint repeat-count exporter repair (2026-10-21)

1. C2's first native animated-paint test exposed a shared offline exporter defect:
   `ImmExporter_PaintSetMaxRepeatCount` updated the layer's timeline repeat field
   instead of its paint implementation. A requested paint repeat count of three
   read back as one. The setter now updates `LayerPaint`, preserving the independent
   timeline repeat count.
2. A focused native export/read-back regression covers drawing repeat counts
   zero (infinite), one and three, each with an independent timeline repeat count
   of seven. All pass with the rebuilt Windows plugin; existing static conversion,
   zero-width import, root-key and reflection checks also pass locally. Windows
   export verification passes. Other platforms and runtime playback require CI.
3. Full validation run 39624464050 is still running; its Godot iOS job was cancelled
   before this repair was committed. The prior hosted-runner capacity failure
   remains unresolved. Confirmed source regressions retain priority over features;
   independent C2 implementation continues while infrastructure is unavailable.
4. Run 39628344285 failed Windows job 119319527234 in the existing exporter bridge
   smoke test: it asserted that the paint setter overwrote the timeline repeat field.
   The remaining run was cancelled. The smoke fixture now independently authors and
   checks timeline repeats=7 and paint repeats=3 through their respective reader
   APIs. Its local native round trip passes (`artifacts/c2-bridge-smoke.log`). This
   retains the gate and checks both fields rather than accepting the old corruption.
   A new full run is required after this test-source correction.

### 6.5 C2 animated-paint implementation checkpoint (2026-10-21)

1. `QuillToImmConverter.ConvertAnimatedPaintProject` uses the shared graph/compiler
   for multiple drawings, held/empty drawing references, paint repeats, timeline
   duration/repeats, visibility, opacity, offset and reflected transform keys.
   Interpolation names map explicitly to IMM enums. Sequence frame rate is used
   without resampling frame maps, matching inspected Quill exports. The existing
   static entry still rejects animation.
2. Raw JSON declarations distinguish absent channels from SharpQuill's constructor
   defaults. Statically hidden paint is excluded, while visibility-keyed paint can
   be retained through animated ancestors. StartOffset is not applied a second
   time when an offset key already represents it. Unrepresented offsets, fractional
   or out-of-range timing, invalid frame references and unsupported action channels
   fail without replacing existing output.
3. Local native export/read-back checks pass for drawings/held frames, hidden paint,
   independent repeats, timeline timing, key values/ticks, all five source
   interpolation enums and reflected transform keys. Existing static, importer,
   root-query and reflection regressions pass. These are stored-data checks, not
   evaluated interpolation/startup/playback acceptance.
4. Fishgym's candidate matches its co-located Quill-exported IMM exactly for the
   paint/group subset: one paint layer, 4,085 strokes, 143,833 stored points,
   drawings/frame maps, transforms/pivots, all stored layer keys and nested timing.
   Viewpoint/startup metadata are explicitly omitted with partial-output warnings.
   Local logs: `artifacts/c2-fishgym-animated-compare.log` and
   `artifacts/c2-tests-06.log`. This case does not prove animated transform playback.
5. C2 remains incomplete: native playback at boundary/interpolation times, startup,
   pause/seek/restart, broader paired assets, viewpoints, root actions/chapters and
   source cases outside the implemented timing range remain to be completed.
   C3 media and the remaining reader/reverse/live-authoring tracks retain their scope.

### 6.6 C2 native playback capture checkpoint (2026-10-21)

1. Native viewer spawn selection overrides the initial Custom camera. This made
   Fishgym's original default viewpoint unsuitable for a fixed-observer paint
   comparison. The DirectX capture script now accepts opt-in `-LockCamera`, backed
   by `IMM_VIEWER_VALIDATE_LOCK_CAMERA=1`. It requires a Custom camera and prevents
   spawn selection from moving it; document spawn state still updates normally.
   Normal navigation is unchanged and is not validated by these locked captures.
2. Rebuilt Windows viewer SHA256:
   `64e624fee5eed48653b862c2d7f7073735a70930e2ea07a410b7e41080235286`.
   Fishgym reference/candidate captures at player frames 1, 8, 16, 30 and 45 use
   fixed dt=1/30, 960x540 and an identical fixed camera. Actual player frames match
   requests, and paint draw/triangle counts match in each pair. Changing triangle
   counts across frames confirms different drawing content was rendered.
3. Pixel differences are respectively 12, 17, 10, 7 and 0 out of 518,400 pixels.
   Frame-8 repeat captures are individually pixel-identical, so the discrepancy is
   reproducible. Do not declare exact animation parity or silently relax gates.
   Logs/captures/metrics remain under ignored `artifacts/c2-playback/`.
4. The earlier exact metadata comparison covers reader-exposed float fields.
   Binary layer metadata inspection finds additional double-precision differences
   hidden by that API (for example a 2.37e-8 translation delta on NewGroup1).
   A controlled local probe copying only reference transform/pivot bytes into the
   candidate reduced frame-8 differences from 17 to 10 pixels, but did not eliminate
   them. Precision contributes to the discrepancy; its remaining cause is unresolved.
   The probe is an ignored experimental file, not converter output or acceptance.
5. Without `-LockCamera`, taperecorder's existing frame-60 capture remains exactly
   unchanged after the viewer rebuild: SHA256
   `92e422b899bd32de1f758110550d20b808eab9fcb1937d64078a109f08611455`.
   Next C2 work must resolve remaining raster/precision differences and exercise
   transform interpolation, boundary/startup/seek/restart and navigation behavior.
6. Follow-up comparisons also require exact decoded point length/time and drawing
   quantization scales; Fishgym passes. All 12 original/candidate drawing headers
   and all 16 decompressed codec streams per drawing are byte-identical. The raster
   discrepancy is therefore outside the encoded paint data. The ignored inspection
   script/report are `artifacts/c2-playback/inspect_paint_streams.py` and
   `paint-stream-differences.json`.
7. Binary inspection additionally exposed missing root timing: the reference root
   is a timeline of 45,360,000 ticks, while the graph/compiler default root is not a
   timeline and has duration zero. A controlled probe restoring this root timing
   on top of the transform probe leaves the same ten differing pixels, so it is
   not the cause of that discrepancy. Nevertheless, root timing must be represented
   through the shared graph/compiler rather than silently dropped by C2. This is
   the next implementation prerequisite for root/chapter acceptance.

### 6.7 Shared offline root properties (2026-10-21)

1. `ImmAuthoringDocument.SetRootProperties` and nullable snapshot `RootProperties`
   now represent the canonical Root group's visibility, opacity, transform/pivot
   (including flips), timeline flag, duration and repeats. Transactions clone these
   values; snapshots retain their revision's values; structural comparison checks
   them. An unset root preserves legacy compiler behavior. Root naming remains
   canonical; root keys/chapter construction and direct live mutation are separate.
2. Additive `ImmExporter_SetRootLayerProperties` configures the sequence root before
   compiling child layers. Visibility requires a serialized time-zero constant key;
   setting the native layer field alone would be lost because visibility is stored
   in key channels. Legacy transform structs/exports and playback code are unchanged.
3. The animated converter applies declared root properties. Inspection of all 21
   sources found 20 ordinary one-hour roots and one legacy false/zero root (Fishgym).
   Fishgym's paired Quill IMM normalizes that legacy root to the ordinary timeline
   of 45,360,000 ticks. The converter now performs that observed normalization with
   a `NormalizedRootTimeline` informational diagnostic. This uses SharpQuill's
   existing default root duration, not a frame resampling or new time unit.
4. Windows source/binary and native export verification pass. New native tests check
   exact serialized root timing/opacity/transform/pivot/flips, visibility read-back,
   transaction isolation/commit, immutable snapshots, invalid-value rejection and
   structural comparison. The existing repeat/static/import/root-key/reflection/
   animation suite and exporter bridge smoke pass locally. Other-platform builds
   and Unity compilation remain pending CI for this revision.
5. Fishgym's regenerated root timeline/duration/repeats now match its reference;
   its decoded paint/key comparison still passes. Frame-8 locked-camera capture
   differs from the reference by one pixel, compared with 17 before root mapping.
   Exact-transform probes do not monotonically improve raster equality (the newest
   combined probe differs by 18 pixels), so do not claim a complete precision cause
   or exact visual parity. Other animation boundaries/navigation remain open.
6. At this checkpoint the reader exposed root keys but not root layer properties.
   Section 6.8 adds static root property reading/import acceptance; root animation,
   reverse-converter consumption and full root/chapter round trips remain incomplete.
7. One local self-test run (`c2-root-properties-05.log`) failed replacing an existing
   output in the pre-existing static partial-output check. The unchanged source
   passed on the next fresh directory (`c2-root-properties-06.log`); its cause is
   unproven. No retry logic or weaker assertion was added. Keep the failure recorded
   if it recurs. Windows CI for the preceding revision f18d77b4 now passes, including
   the corrected exporter bridge smoke; the full run remains incomplete.

### 6.8 R1 static root reading/import (2026-10-21)

1. Additive `StrokeReader_GetRootLayerProperties` returns the existing authoring-layer
   info and float transform layouts for the root, separately from indexed child
   layers. It exposes timing/repeats, opacity, transform/pivot/flips and static
   visibility. A sole None visibility key at tick zero supplies static visibility,
   since the file has no separate visibility field. Root keys remain separately
   queryable. Metadata is copied once at import; playback and per-frame work are
   unchanged. Null outputs, invalid documents and unloaded lifetimes return false.
2. The authoring importer now populates shared root properties. Constant time-zero
   visibility and the sole default None/zero Offset key are represented by static
   settings/default clock behavior; other root keys still produce explicit lossy
   `UnsupportedRootAnimation` diagnostics. Noncanonical root names are normalized
   with `UnsupportedRootProperties`. Explicit default roots and unset legacy roots
   compare semantically equal; nondefault root settings still participate in
   structural comparison.
3. Native tests pass for root query values, child-layer count stability, null/bounds/
   lifetime handling, static-root import without root-loss diagnostics and exact
   root metadata re-export. A copied tracked fixture proves default zero offsets
   are accepted while nonzero root offsets remain explicitly lossy. The test uses
   a paint child to expose the file's paint-level frame rate; codec-derived point
   length/time are outside the exact pre-encoding root-property comparison.
4. The complete local native converter/import/reflection/root suite, bridge smoke,
   adapter checks and Windows export verification pass. The reader now declares
   41 native entry points, with build ID `2026-10-21-ROOT-PROPERTIES`; both committed
   Windows reader DLLs match. Other-platform publication and Unity CI remain
   pending for this new API. The preceding root-writer run 39634328582 has passed
   Windows job 119340634233. Its Godot iOS job 119340633744 was cancelled without
   acquiring a runner or executing any steps; annotations explicitly identify
   hosted macOS ARM64 capacity. This is still unresolved infrastructure, not a
   passing full run. Fresh reader/import validation is run 39637172881 at 7cb26ffb.
5. R1 is not complete: root animation/chapter acceptance (construction added in 6.9), native double precision,
   remaining consumer/adapter contracts and reverse-conversion acceptance retain
   their scope. C2 still needs broader evaluated animation and navigation evidence.

### 6.9 Shared root keys and comic/chapter export (2026-10-21)

1. `CreateAnimationKey(layerId: 0, ...)` now targets the document root. Existing
   replacement/removal operations, ownership validation, transaction cloning and
   immutable snapshot lookup support those keys. `RootAnimationKeys` is separate
   from indexed child layers and participates in the existing key resource limit.
2. Additive `ImmExporter_GetRootLayer` supplies a borrowed sequence-owned handle.
   The compiler uses the same key-writing path for root and child channels. Explicit
   root keys precede static root properties; the native root-property setter supplies
   a default visibility key only when no visibility channel exists. This preserves
   pre-first-key hiding and explicit channels without adding an earlier default key.
3. Supported native root visibility/opacity/transform/draw-in/action/loop/offset keys
   now import into the graph. Root action values retain native Stop/Play/Loop/
   MakeDefault enums and exact tick times. Comic import/export is enabled with root
   action markers; comic export without those markers still fails explicitly.
   Legacy component transform keys and invalid values retain existing diagnostics.
4. Root structural comparison canonicalizes static visibility carried by a sole
   None/time-zero key, and the absence of an independent serialized base visibility
   for animated roots. Explicit/unset default roots remain semantically equivalent;
   remaining key fields/times/interpolation still compare normally.
5. A native synthetic fixture round-trips all 12 supported root keys, including all
   four action values, a reflected Smoothstep transform and Play/Stop at ticks
   524/525. Both encoded files report Comic and three chapters; every stored root
   key compares exactly after import/re-export. Root-only snapshot ownership,
   transaction isolation/commit, replacement/removal and limit/output-preservation
   checks pass. Default and nondefault root offsets are both retained, replacing
   the previous unsupported-offset behavior. Existing native regressions and bridge
   smoke also pass; Windows export verification reports 113 player/exporter and
   41 reader declarations, with matching local binaries.
6. This proves stored-data construction and native chapter indexing, not actual
   chapter navigation, pause/seek/restart or Quill source action mapping. The Quill
   converter still rejects unverified root action channels. The local Chapters
   project's missing paired export and the remaining native playback cases retain
   their scope. No direct live-edit root mutation or per-frame player change was added.
7. The preceding reader/import revision 7cb26ffb has passed Windows CI job
   119350736467 in run 39637172881. Full validation and this new writer API's
   other-platform/Unity acceptance remain pending; the old Godot capacity failure
   is still unresolved.

### 6.10 C2 declared root-default preservation (2026-10-21)

1. The animated converter now maps its already-accepted declared root visibility
   and zero-offset defaults through the shared root-key operations. Root defaults
   are no longer discarded while child keys are exported. Unverified source root
   action channels still fail before output replacement; their diagnostic now
   identifies the verified-root mapping boundary rather than calling it static paint.
2. `--compare-animated-paint` additionally checks reader-exposed root properties
   and every retained root key, alongside existing child keys/timing, paint geometry,
   drawing scales and point length/time. All exact exposed-field comparisons pass
   for taperecorder (114 strokes/2,068 points), consul-room (2,495/57,187) and
   Fishgym (4,085/143,833) against their co-located Quill IMM exports. Root metadata
   comparisons use the public float ABI and do not prove native double identity.
3. The synthetic native suite passes with root-default preservation and unchanged
   unsupported-root-action/output-preservation assertions. Logs remain under
   ignored `artifacts/root-converter-*` / `root-compare-*`. This does not close
   startup metadata, source chapter-action mapping, navigation or playback acceptance.

### 6.11 C2 viewpoint reference/default audit (2026-10-21)

1. Reference reports now include native spawn tracking, volume and locomotion fields.
   The four paired projects contain eight exported viewpoints: eye-level cases in
   taperecorder/consul-room/Fishgym and floor-level cases in Snoopy. Sphere centre
   (0,1,0), radius 2, locomotion mask 7 and source default-path selection match.
   Snoopy's default is the third viewpoint, so creation-order fallback is insufficient.
2. All 37 local source viewpoints have identity pivots and opacity one. Exporting
   flags select 24 for export and exclude 13 independently of gizmo visibility.
   Fifteen source declarations contain preview offsets; managed preview assets
   are not yet represented and must be diagnosed until the shared model supports them.
3. Fishgym's colour-only legacy viewpoint has neither Version nor TypeStr and its
   paired IMM uses eye-level tracking. SharpQuill previously supplied FloorLevel.
   The reader now defaults this legacy form to EyeLevel while retaining FloorLevel
   for versioned/new viewpoints and preserving explicit settings. Reader regressions
   and all 21 source metadata trees, 34 PCM sounds and 35 picture settings pass.
   Mapping/navigation acceptance remains in progress.

### 6.12 C2 default-visibility export regression (2026-10-21)

1. Snoopy's broader project comparison exposed an animated-converter bug: a default
   true/None visibility key at tick zero was treated as evidence that a statically
   hidden branch might animate into view. This included paint the paired Quill
   export omits. The mapper now distinguishes that constant default from genuine
   timed/multiple visibility keys and excludes the hidden paint branch.
2. Snoopy has 12 effectively visible paints present in the reference and 41 hidden
   paints absent from it. Across all 21 source projects, the 98 individually hidden
   paint/group declarations have only absent/default visibility channels. Nondefault
   keyed visibility on a statically hidden source flag remains a synthetic case,
   not Quill-reference acceptance; do not infer that unrepresented source behavior.
3. The synthetic native suite now includes a hidden ancestor carrying a default
   visibility channel and requires its paint to stay excluded. Existing genuine
   timed visibility remains covered. The suite passes; Snoopy's strict exposed-field
   project comparison now matches 12 paints, 68,733 strokes, 928,609 points, root/
   layer keys, drawing scales, point timing and three viewpoint settings. Viewpoint
   implementation and preview/navigation acceptance are recorded separately.

### 6.13 Unity compiler regression and harness correction (2026-10-21)

1. Run 39641429920 reported a source regression introduced with root-key comic
   export: `ImmAuthoringCompiler` used LINQ `Any` without `using System.Linq`.
   Unity reports CS1061 in iOS simulator, Android URP and both Windows graphics
   builds. Non-XR Android also aborts for script compilation errors; its captured
   console does not retain the exact compiler diagnostic. Android Vulkan then
   fails its required build/device evidence gate. These gates are unresolved until
   fresh Unity/device validation passes; missing captures are not rendering proof.
2. The remaining latest run and the preceding known-affected root-key run 39640885319
   were cancelled under the source-failure policy. No required gate, deadline or
   assertion was weakened. The compiler now imports System.Linq explicitly.
3. The .NET harness's implicit global imports masked this error. They are now
   disabled; test files declare their own required namespaces, so production files
   must compile with their own imports. An ignored copy of the compiler with the
   import removed reproduces CS1061 in `artifacts/unity-import-guard/before-build.log`.
   The corrected production sources build without warnings/errors and the native
   suite passes (`artifacts/unity-import-fixed-suite.log`). This does not substitute
   for Unity compilation; a fresh full validation run is required.
4. Viewpoint mapping was held while this regression fix was prioritized. Its
   separate implementation checkpoint follows in 6.14; no viewpoint changes were
   included in compiler correction ac34f4f1.

### 6.14 C2 viewpoint mapping and default camera (2026-10-21)

1. `QuillToImmConverter.ConvertProject` maps paint/animation and exported viewpoints
   through the shared graph/compiler. Existing paint-only entry points retain their
   scopes. Viewpoint Exporting controls inclusion independently of gizmo visibility;
   tracking, sphere centre/radius, translation mask, hierarchy and explicit default
   path are retained. Viewpoint-only supported projects can export. Unknown tracking
   and currently unrepresented pivot/opacity/timeline settings fail explicitly.
2. Embedded previews remain unsupported image assets. Strict conversion rejects
   them; explicit omission permission produces `OmittedViewpointPreview` and partial
   output. An absent/ambiguous declared default similarly requires explicit fallback
   permission and reports `OmittedDefaultViewpoint`. Exporting=false exclusion is an
   intentional source export rule, not a lossy omission. Preview/full-fidelity and
   broader navigation acceptance remain open.
3. Native synthetic checks pass for hidden gizmos, non-exported viewpoints, eye/floor
   tracking, nondefault sphere and locomotion mask, hierarchy, non-first default,
   viewpoint-only output and failure/output-preservation diagnostics. The complete
   native suite passes with implicit imports disabled.
4. Strict reader-exposed project comparisons now pass all four paired references:
   taperecorder (one viewpoint), consul-room (three), Fishgym (one) and Snoopy (three).
   Comparisons include all paint data, root/layer keys/timing and viewpoint local
   transforms, tracking, volume, locomotion and default selection. Float API limits
   still apply. Native taperecorder capture uses normal automatic viewpoint selection
   with camera locking disabled; candidate/reference are pixel-identical at frame
   60, fixed dt=1/30, 960x540, SHA256
   `92e422b899bd32de1f758110550d20b808eab9fcb1937d64078a109f08611455`.
5. CLI `--with-viewpoints` selects the project entry; `--allow-omissions` permits
   diagnosed partial output. `--compare-project` checks the exposed viewpoint data
   as well as animated paint/root fields. Local logs use ignored `artifacts/viewpoint-*`.
   Native navigation switching, floor-level camera controls, source startup/chapter
   mapping and image previews need separate work. Fresh full CI must also validate
   the LINQ correction and this revision; previous failures remain unresolved.

### 6.15. Viewpoint preview export foundation (2026-10-21)

1. Added immutable `ImmAuthoringImage` assets for RGB8/RGBA8 and encoded JPEG/PNG,
   shared safely by graph snapshots and transactions. Spawn layers own previews;
   other layer types reject them. Structural comparison checks image metadata and
   bytes. Export limits count the larger of encoded and decoded byte sizes.
2. Added `ImmExporter_SpawnAreaSetPreview`. Raw pixels use the existing native
   JPEG codec; encoded assets retain their bytes and IMM v1 BGR/v2 RGB version.
   Encoded dimensions/channels are checked by native decoding before assignment.
   This is an offline export capability; it adds no per-frame playback work.
3. Local Windows native and managed builds pass. The native harness covers image
   byte ownership, snapshot isolation, comparison, raw RGBA encoding, unchanged
   JPEG payload export, malformed-image rejection and output preservation under
   image limits. The export verifier sees 114 player/exporter declarations and
   41 reader declarations. Full platform validation remains pending.
4. Preview import/query support and Quill qbin preview mapping are the next
   milestone. `ConvertProject` still diagnoses embedded previews as unsupported;
   this foundation alone does not close preview round-trip or C3 acceptance.
   The current full CI run is `39646549914`; at the latest check it reported no
   failures and iOS Metal validation was running. Earlier Unity compiler failures
   remain unresolved until the replacement platform gates pass.

### 6.16. Encoded viewpoint preview import (2026-10-21)

1. Added reader preview-info/data queries indexed by the authoring-layer array.
   File and memory loads retain original JPEG/PNG payloads, dimensions, channels
   and IMM v1 BGR/v2 RGB version. Managed import attaches immutable image assets;
   re-export preserves their encoded bytes without another JPEG generation.
2. Declared previews are distinguished from exporter-generated blank resources.
   Legacy non-image resources and missing asset IDs remain absent, matching the
   native importer's existing behavior. Unreadable declared image assets fail
   the load instead of silently losing their contents.
3. This exposed an existing native importer defect: successful thumbnail decoding
   never set `mHasAsset`, so `GetScreenshot()` returned null. The load now marks
   the asset available, enabling thumbnail access and version-1 channel swapping.
4. Local native/managed regression checks pass for file and memory JPEG/PNG
   queries, exact import/re-export bytes, v1 BGR metadata, absent previews,
   buffer/index/init/lifetime guards, and the existing sample/root/paint suite.
   Both Windows plugins are rebuilt together; Windows export verification sees
   114 player/exporter and 43 reader declarations. Platform CI remains pending.
5. Quill qbin preview mapping is still the next step. This does not close C3
   media support, runtime thumbnail display acceptance or cross-platform gates.

### 6.17. Stored Quill viewpoint previews (2026-10-21)

1. `ConvertProject` reads declared viewpoint qbin previews as raw RGB8/RGBA8
   assets and exports them through the shared graph/compiler. Offsets, image
   format, dimensions and full pixel bounds are checked before allocation. Invalid
   previews fail even with omission permission and preserve existing output.
   Non-exported viewpoints remain excluded without reading their payloads.
2. SharpQuill metadata retains viewpoint preview offsets, including `0x`-prefixed
   values. The local corpus still passes 21 metadata-tree, 34 PCM-sound and 35
   picture-setting regressions. Its 15 stored viewpoint previews are all exported:
   five RGBA and ten RGB; observed RGBA alpha values are all 255. This milestone
   does not add preview payload support to SharpQuill's project writer/full reader.
3. Synthetic native checks cover RGB/RGBA payloads, absent previews, truncated
   pixels, invalid formats and overflowing offsets. Existing paint/root/animation/
   reflection checks pass. Taperecorder now converts with no omissions; three
   paired projects (taperecorder, consul-room, Snoopy) pass the stricter exposed
   preview metadata comparison as well as prior paint/root/viewpoint checks.
4. The new preview comparison **fails Fishgym**: its source HomeViewpoint has no
   preview offset, while the paired IMM has a 512x512 JPEG. The source directory
   has only Quill JSON/qbin/state and its IMM reference; no separate preview was
   found. Generating export-time previews from scene content remains open; the
   comparator retains this failure rather than ignoring preview presence.
5. Generated JPEGs are not proven pixel-identical to Quill exports. Taperecorder
   has identical quantization tables but mean decoded RGB differences of
   0.4183/0.2288/0.4140 (0-255), maximum per-channel differences 24/12/34. Native
   IMM import/re-export byte identity is separately established in section 6.16.
   Native runtime thumbnail display, other preview pairs and full platform CI
   remain unaccepted. Local evidence is under ignored `artifacts/quill-preview-*`.
6. CI update: `39646549914` (viewpoint revision `740eec8a`) passed Unity iOS
   Metal validation, providing fresh evidence for the LINQ compiler correction
   on that platform. Native Windows jobs also passed in `39649419977` (preview
   export) and `39650746564` (encoded preview import). These do not close Android
   or remaining platform/device gates. Stored Quill preview conversion revision
   `7c4d90a0` is pushed and full validation run `39651826435` is queued.

### 6.18. PNG memory-reader bounds regression (2026-10-21)

1. C3 picture tests exposed an existing shared PNG decoder defect: its stream
   callback copied requested bytes without checking the supplied input length,
   and signature inspection also assumed at least eight bytes. New encoded image
   entry points and existing imports both use this decoder.
2. The callback now checks remaining bytes before copying; short inputs fail via
   the decoder's error path. Successful reads require the complete PNG end record.
   Decoder structs and row allocations are freed on success and decode errors.
3. A Windows no-access-page regression test reproduces an access violation with
   the old committed plugin. Rebuilt player and reader plugins reject seven PNG
   prefixes ending at that page boundary and accept a complete PNG. The existing
   preview, paint, root, animation, reflection and converter regression suite passes.
   Cross-platform compiler/device validation remains pending.
4. Picture graph work was retained separately while this fix was validated, so
   the decoder correction and its matching binaries can be committed independently.
   Offline picture construction/import/conversion and C3 media acceptance remain
   outstanding; the entire pipeline goal is still active.

### 6.19. Offline picture construction foundation (2026-10-21)

1. Added `ImmAuthoringLayerType.Picture`, `CreatePictureLayer` and required shared
   immutable image properties. The compiler writes projection, viewer locking,
   hierarchy, opacity, transforms/pivots/reflections and timeline metadata through
   new `ImmExporter_CreatePictureLayer` / `ImmExporter_PictureSetImage` entry points.
   Graph validation rejects missing images, invalid projection enums, BGR picture
   payloads and image ownership on other layer types. Picture layers cannot own
   paint drawings. Transactions/snapshots retain immutable assets and comparison
   checks image bytes and settings; export budgets include image memory.
2. Raw RGB/RGBA assets use the existing native picture encoder (lossless PNG after
   RGBA conversion). Valid supplied PNG/JPEG payloads retain their encoded bytes.
   Decoded image validation is shared with viewpoint previews. Native picture
   asset export rejects a missing image instead of dereferencing it.
3. Local native regression checks pass for all five existing native projection
   enum values and viewer-lock metadata, exact asymmetric RGBA pixels including
   partial/zero alpha, common properties, transactions/snapshots, encoded PNG/JPEG
   identity, malformed inputs and output-preserving resource limits. Existing
   preview/PNG bounds/paint/root/animation/reflection/converter checks also pass.
   These prove file construction/read-back; projection appearance, panorama/stereo
   eye layout, viewer-lock behavior and platform/device acceptance remain open.
4. Windows build/export verification passes with 116 player/exporter and 43 reader
   declarations. Picture import still reports unsupported content, and source
   conversion still diagnoses picture omissions until its mapping is implemented.
   No live mutation or runtime rendering path was extended. The next C3 step is
   picture import and source conversion against representative local references.

### 6.20. Encoded picture import and reader contract (2026-10-21)

1. Added authoring-indexed `StrokeReader_GetPictureAssetInfo` / `Data` queries
   exposing original PNG/JPEG bytes, source channels, dimensions, projection and
   viewer locking. The existing legacy decoded-pixel queries retain their indexing
   and RGBA representation. The new 32-byte metadata struct is additive.
2. Managed import creates picture layers with immutable encoded assets, preserving
   common layer metadata, hierarchy and animation through the existing graph path.
   File and memory import/re-export retain picture bytes exactly. Missing/unreadable
   picture assets fail rather than being silently omitted. Source sound layers are
   still explicitly reported as unsupported.
3. Original channel counts are captured before native RGBA conversion. Encoded
   grayscale pictures are supported alongside RGB/RGBA; grayscale is expanded to
   RGB with opaque alpha for rendering. PNG grayscale depths 1/2/4 are expanded
   to 8-bit values before conversion. Viewpoint previews retain their RGB/RGBA
   contract. Redundant RGBA conversion is skipped and actual conversion failures
   are checked.
4. Local native checks pass for file/memory queries, metadata ABI, index/buffer/
   lifetime guards, all five projection enums, encoded JPEG/PNG identity, graph
   round trips, hidden pictures, and decoded 8-bit/1-bit grayscale values. The
   real `sample1.imm` picture also passes; its three sound omissions are reported,
   so this is not whole-document lossless import. Existing PNG bounds, paint,
   root, animation, reflection, viewpoint and converter checks pass.
5. Both Windows plugins are rebuilt; export verification sees 116 player/exporter
   and 45 reader declarations. Full platform/device validation remains pending.
   Source picture conversion, projection/stereo/viewer-lock visual acceptance and
   sound construction/import are the remaining C3 work.

### 6.21. Declared identity root opacity default (2026-10-21)

1. The Alex reference project declares one root opacity key: time zero, value 1,
   interpolation None, with static root opacity 1. Binary inspection of its paired
   Quill export shows no root opacity channel. The converter previously rejected
   this otherwise valid identity default.
2. Preflight now accepts that exact case, and root mapping omits the identity
   channel to match the inspected export. Nonidentity static root opacity combined
   with that channel, nondefault values, additional keys and other interpolation
   still require separate reference evidence; genuine root opacity animation is
   not newly accepted.
3. Local native regressions pass, including default acceptance, absence of an
   extra native root opacity key, nondefault rejection and the existing picture,
   PNG bounds, paint, root, animation and viewpoint checks. Windows platform CI
   passed the preceding picture-construction milestone; newer import/full-platform
   gates remain pending. Source 2D picture wiring is still in progress separately.

### 6.22. Quill 2D picture conversion (2026-10-21)

1. `ConvertProject` now maps `2D` / `TwoD` picture declarations through the shared
   graph/compiler, including RGB/RGBA pixels, alpha, local properties, hierarchy,
   animation and viewer-lock metadata. Hidden pictures are excluded by the same
   effective-visibility rule as paint. Picture-only projects can produce output.
   Paint-only entry points retain their previous scope. Unsupported projections
   require explicit omission permission; malformed mapped images always fail.
2. Existing stored-preview reading is shared with picture input and checks format,
   dimensions and complete pixel bounds. The explicit operation-options overload
   supports caller-selected resource limits, cancellation checkpoints and shared
   exporter options. Existing calls retain the 256 MiB input default; the harness
   exposes `--input-mib=N` for the project profile. Input/output budget and cancelled
   conversion failures are checked to preserve existing output.
3. Current-source/reference inspection of the Alex pair verifies 19 visible source
   RGB pictures, 2D native projection, viewer locking off and exact decoded pixels.
   Cached transform evidence was checked against current source JSON/reference IMM hashes;
   direct binary/image inspection independently verifies the media mapping.
4. An isolated source corpus retained those 19 pictures, viewpoints and their
   ancestors, with qbin offsets rewritten after removing unrelated content. Its
   qbin is 432,471,208 bytes; an explicit 512 MiB budget produces 16,182,127 bytes.
   All 19 converted/reference RGBA pixel arrays match exactly, as do projection,
   viewer lock, local transform/pivot at reader float precision, opacity and
   timeline/repeat metadata. This is **not** full-scene conversion or playback
   acceptance. The corpus reports a default-viewpoint fallback, so output is partial.
   Original large-project conversion still exceeds the default input budget.
5. Native synthetic checks pass for RGB/RGBA/alpha, picture-only content, hidden
   exclusion, viewer-lock metadata, import round trips, unsupported projection
   diagnostics, malformed/truncated input, configured limits and cancellation.
   `--compare-project` now includes picture counts/properties and exact decoded
   pixels. Snoopy retains its previous paint/root/viewpoint comparison matches;
   its five hidden source pictures are absent in both converted/reference IMM.
6. Picture import revision `3adc000a` passed native Windows CI (`39656221903`,
   job `119417092975`). Full platform/device gates remain pending. Stereo source
   mapping, eye layout, panorama/depth/cubemap evidence, runtime appearance and
   sound support remain open. A matching Quill export of the local Pano project
   has been requested; the repository stereo IMM is a synthetic fixture.
   Private corpus paths/reports remain in ignored `artifacts/quill-picture-*`.

### 6.23. Windows Vorbis encoder dependency loading (2026-10-21)

1. Sound construction testing exposed a Windows delay-load failure before Vorbis
   encoder initialization. The packaged `vorbisenc.dll` was not found from a
   standalone .NET host using the normal DLL search path. Adding only the player
   plugin directory to that test process's PATH made all Ogg cases pass, isolating
   dependency loading from codec/PCM correctness.
2. The existing native Windows preload helper now loads the encoder from the
   player plugin's own directory after its shared reader-package dependencies.
   This is a one-time module-load operation; no per-frame path was added. It does
   not alter the process PATH or change codec settings.
3. A regression test reproduces missing encoder preload with the previous plugin
   and passes with the rebuilt plugin without a search-path workaround. The full
   existing local PNG/picture/paint/root/animation/viewpoint/converter suite passes.
   The preceding 2D converter revision `8a242062` passed native Windows CI
   (`39659838195`, job `119429666287`); full platform/device gates remain pending.
4. Sound graph implementation remains a separate in-progress milestone. Encoding
   success is not audible playback or source-to-Quill fidelity acceptance.

### 6.24. Offline PCM16 sound construction (2026-10-21)

1. Added immutable `ImmAuthoringAudio` PCM16 assets and `CreateSoundLayer` in the
   shared graph. Supported construction uses interleaved little-endian PCM16,
   8-192 kHz, mono/stereo or four-channel ambisonic data. Four channels require the
   ambisonic spatial type; flat/positional construction uses mono or stereo.
   Audio bytes are defensively copied and shared safely by snapshots/transactions.
2. Added a 44-byte native sound-settings struct plus additive sound creation/PCM
   setup calls. Settings include native spatial type, gain, looping, attenuation
   mode/range and cone/frustum parameters in radians. Validation rejects invalid
   enums, nonfinite/negative values, inconsistent channel/spatial layouts and
   unused modifier values that could otherwise disappear on serialization.
   Common hierarchy, transforms/pivots/reflections and timing use the existing path.
3. The compiler copies PCM into an owned native wave and uses existing Opus/Ogg
   encoding. Missing native audio assets now fail export instead of reaching an
   uninitialized wave. Sound bytes count toward the shared asset input limit.
   Reader sound extraction/import, source conversion and live mutation were not added.
4. Local native checks pass for immutable bytes, ownership, snapshots/transactions,
   comparison, invalid parameters, five spatial-setting cases, mono/stereo/four-
   channel Opus and Vorbis output, encoded headers, hierarchy/common properties,
   resource limits and output preservation. Existing picture/PNG/paint/root/
   animation/viewpoint/converter tests remain green. Both Windows API declarations
   are exported: 118 player/exporter and 45 reader declarations in the verifier.
5. Independent FFmpeg decode succeeds for all ten generated streams. Channel
   counts, nonzero signals and per-channel frequency signatures are preserved.
   Opus yields 4,800 decoded frames at 48 kHz for the 4,410-frame/44.1 kHz input
   (0.1 seconds); reported container duration includes pre-skip. Vorbis reports
   0.1 seconds but yields 4,282 frames at 44.1 kHz. A separately generated
   FFmpeg/libvorbis control also yields 4,282, so this is not shown to be a new
   encoder regression. Exact timing/gapless behavior in the native player remains
   an explicit acceptance item; file validity is not audible playback acceptance.
6. The local Quill corpus has 34 PCM16 sounds at 44.1 kHz: 33 stereo and one mono.
   Source gain/loop/attenuation/modifier mapping still needs reference comparison;
   compressed asset passthrough/import is the next dependency. Source sound layers
   remain diagnosed omissions. Full CI/platform/device validation is pending.
   Local evidence is in ignored `artifacts/sound-authoring-*` reports and streams.
7. CI update: the loader correction `5ea66d57` passed native Windows validation
   (`39662841936`, job `119440076064`). Sound construction revision `4dd37545`
   is pushed; full validation run `39664513588` is queued. These do not establish
   full platform/device or audible playback acceptance.

### 6.25. Encoded sound extraction, import and passthrough (2026-10-21)

1. Added authoring-indexed sound metadata and byte queries (44-byte settings,
   68-byte asset info), with file/memory parity and guarded buffer/lifetime behavior.
   The graph imports WAV, Ogg Vorbis and Ogg Opus without recompression, preserving
   spatial settings, legacy source duration and initially-playing fields. Encoded
   assets report frame count -1; Opus reports its 48 kHz decode rate. Original
   encoded bytes retain the source header. Global export codec selection affects
   PCM construction only.
2. Added bounded WAV/Ogg container validation, including Ogg page CRC, serial,
   sequence, continuation and end-of-stream checks. This checks framing and headers,
   not complete packet decoding. Unsupported sound assets receive an explicit
   import diagnostic and are omitted from the partial graph. Validation runs during
   offline import/export and adds no per-frame playback work.
3. Fresh Windows reader/player builds and the full local converter/authoring suite
   pass. Generated mono/stereo/four-channel Vorbis/Opus, WAV, hidden sound and all
   three sounds in sample1 pass exact encoded-byte and graph round trips. Protected
   page truncation, corrupt checksums and failed-export output preservation pass.
   Windows exports match all 120 player/exporter and 47 reader declarations.
   The existing Emscripten Web decoder also rebuilds successfully with the shared
   reader validation code; this establishes compilation, not browser playback.
4. Quill sound source mapping is the next C3 dependency. Audible playback, exact
   timing/gapless behavior, source gain/loop/spatial reference comparison, live sound
   mutation and full platform/device CI acceptance remain open. Evidence is in
   ignored artifacts/sound-import-portable-* logs. The preceding sound-construction
   run 39664513588 passed native Windows job 119445872854; its remaining gates are
   pending. This milestone's full CI run has not yet been queued.

### 6.26. Replacement CI closure and current regression audit (2026-10-21)

1. Live GitHub verification at 20:23 UTC confirms compiler-fix run 39645977224
   (ac34f4f1) and viewpoint run 39646549914 (740eec8a) both completed successfully.
   Previous statements that these replacement gates were still pending were stale.
   The earlier source/compiler regression is closed for the passing hosted gates.
2. Compiler-fix evidence includes Unity Android URP/non-XR builds 119385387725 /
   119385387418, corresponding Firebase validations 119394946887 / 119394946900,
   Unity Android Vulkan 119399696727, Windows Vulkan/DirectX builds 119385655376 /
   119385655431, and final Validation Evidence 119404707553, all successful.
   The independent viewpoint run also passes all corresponding gates.
3. Godot iOS package/Metal jobs 119381540258 and 119383528137 passed in those
   fresh full runs. The old capacity failure and exhausted targeted retry remain
   historical records, not an active blocker. No further old-run retry is needed.
   VR/hardware and optional gates skipped in hosted mode remain unverified.
4. Latest sound-import run 39669262483 (179a9f30) is incomplete with no failed jobs
   at this check. Build/macOS job 119462462605 is queued with runner_id 0 and
   macos-14 labels. Device/engine jobs depend on the build workflow completing;
   their absence so far is not a new compiler failure. Do not treat older successes
   as validation of the later sound APIs. Preserve required gates and inspect any
   actual source failure before continuing implementation.
5. The source sound-reference inspector has uncommitted observations in progress.
   That independent work does not change the closure evidence above. Audio timing,
   generated previews and animation fidelity are remaining acceptance gaps, not
   newly observed CI source failures.

### 6.27. Paired Quill sound mapping evidence (2026-10-21)

1. Extended the read-only reference inspector with source sound settings, native
   encoded-sound metadata and explicit sound-setting comparisons. Correspondence
   requires unique hierarchy paths excluding the source root. Reports retain input
   and reader hashes; they do not establish export provenance or audible parity.
2. Fresh metadata-only inspections match all 3 Snoopy and 28 Alex source sounds
   to their reference IMM layers. All 31 sound settings match exactly at the
   reader's float precision. Flat maps to native 0 and Positional to native 2;
   gains 0.47-1 are copied directly, without a decibel conversion. Both loop values
   occur (26 looped, 5 nonlooped). Attenuation None/Linear/Logarithmic maps to native
   0/1/2 (3/20/8 cases), preserving minimum/maximum directly. The one observed Cone
   maps to native modifier 1 with unchanged radian angle/band and outside gain;
   the other 30 use None. No source ambisonic/frustum mapping is established.
3. The references retain 30 effectively hidden sounds (2 Snoopy, all 28 Alex).
   Sound conversion must retain those layers and their serialized visibility;
   paint/picture hidden-content exclusion must not be applied to sounds. Common
   timing/keys still need candidate comparison; setting equality does not prove
   evaluated startup, playback duration, audio phase, seeking or gapless looping.
4. Ignored reports are artifacts/quill-reference/snoopy-sound-mapping.json and
   alex-sound-mapping.json. Both inspector runs pass and all 31 unique-path,
   supported-mapping and settings-equality checks pass. Next implement source PCM
   loading and graph mapping with those settings, retaining hidden layers and
   validating malformed input/output preservation before paired candidate checks.

### 6.28. Latest Android SDK infrastructure failure (2026-10-21)

1. Sound-import run 39669262483 Android job 119462462600 failed while configuring
   libImmCore: Android SDK installation of CMake 3.22.1 raised ZipException,
   "Archive is not a ZIP archive". Project compilation did not begin. This is an
   SDK archive/install infrastructure failure, not evidence of a source regression;
   the new native sound APIs are still unverified on Android.
2. Requested the permitted isolated job retry through GitHub's job-rerun API.
   GitHub returned HTTP 403: the parent workflow is already running. No retry was
   executed and the one permitted infrastructure retry remains available. Preserve
   successful independent jobs, wait for the parent to finish, then retry this job
   and its dependents once. Repeated failure remains unresolved. Do not relax gates
   or cancel independent jobs for this download failure.
3. Diagnostic evidence is saved in ignored sound-passthrough-android-failed.log,
   sound-passthrough-android-job.json and sound-passthrough-android-retry-response.txt
   under artifacts/. The earlier compiler-fix and viewpoint hosted runs remain
   successfully completed as recorded in section 6.26.

### 6.29. Quill PCM16 sound conversion (2026-10-21)

1. ConvertProject now maps Sound alongside paint, viewpoints and 2D pictures.
   Source mono/stereo, 8-192 kHz, nonempty PCM16 samples become immutable graph
   audio and use the existing native default Opus encoder. Sound-only projects
   work. Hidden sounds and ancestor groups remain present; existing transform,
   pivot/reflection, timeline and animation-key mapping applies. Paint-only profiles
   retain their explicit sound exclusion. Live sound mutation is unchanged.
2. Flat/Positional, direct gain, loop, None/Linear/Logarithmic attenuation and one
   None/Cone modifier follow section 6.27's paired evidence. Unknown settings,
   invalid offsets/headers, unsupported channels/format, oversized/truncated samples
   and graph validation failures reject conversion without replacing output, even
   when omission permission is enabled. Source ambisonic/frustum mapping is not
   established. A single time-zero None opacity key matching static sound opacity
   is omitted, matching all observed reference sounds (including static 0.75).
   Keys with a different value or other timing/interpolation remain preserved.
3. The full local converter/authoring regression suite passes with new sound-only
   mono/stereo, hidden retention, spatial settings, visibility/offset/nondefault
   opacity keys, encoded import round trips and malformed-input/output checks.
   The project comparison mode now checks sound counts, hierarchy/common metadata,
   settings, channels, legacy fields and supported asset presence. It explicitly
   does not compare decoded audio samples or claim codec/rate identity.
4. Built isolated sound corpora from the original Snoopy and Alex sources, retaining
   ancestor groups and original per-sound PCM/header bytes while rewriting offsets.
   Their qbin sizes are 34,278,980 and 165,706,192 bytes; default input limits suffice.
   Both conversions succeed without omissions. All 3 Snoopy and 28 Alex sounds
   match reference spatial settings, channel counts, legacy duration/playing fields,
   keys and timeline metadata exactly at reader precision. Alex static transforms
   match exactly; Snoopy maximum reported component difference is about 2e-9.
   These are isolated sound comparisons, not full-scene or evaluated playback proof.
5. Candidate audio uses Opus at a 48 kHz decode rate; reference sounds use Vorbis at
   44.1 kHz. No compressed-byte equality, decoded waveform equality, audible spatial
   playback, exact/gapless looping, startup/seek/restart or full platform acceptance
   is claimed. Those remain the next sound acceptance work. Evidence and corpus
   scripts are ignored artifacts/quill-sound-* and artifacts/quill-reference/*sound*;
   paired summary is artifacts/quill-sound-paired-summary.json. Latest prior sound
   API CI still has the deferred SDK-install retry described in section 6.28.

### 6.30. Integrated Snoopy conversion and independent audio decode (2026-10-21)

1. Converted the complete original Snoopy source with the project profile. Strict
   conversion correctly rejects its unsupported Camera layer; explicit omission
   permission produces an 8,516,733-byte partial IMM. This is not complete source
   coverage. Statically hidden paint/pictures follow their recorded exclusions;
   sound layers remain present. Native counts are 12 paint, 33 groups, 3 viewpoints
   and 3 sounds. No synthetic media subset is used for this integration check.
2. Reference comparison passes exactly for 68,733 strokes and 928,609 points across
   all 12 paint layers, hierarchy/transforms/pivots/frame buffers and reader-exposed
   root/layer animation and nested timing. All 3 integrated sounds match the
   reference's spatial settings and animation keys. This establishes coexistence
   with paint/animation/viewpoints at serialized-data scope; it is not evaluated
   playback, full image/navigation or audible acceptance.
3. Extracted the integrated candidate's original encoded sound bytes through the
   reader C API and decoded each independently with FFmpeg. All 3 stereo Opus
   streams decode successfully at 48 kHz with the expected channel count. Decoded
   frame counts exactly equal source PCM frame counts rescaled and rounded to
   48 kHz. Source durations are approximately 35.683265, 127.843265 and 30.798367
   seconds. Subsample duration rounding is recorded in the ignored report; Opus
   container duration is separately recorded and includes encoder timing metadata.
4. Evidence is in ignored artifacts/quill-sound-Snoopy-full-* logs/IMM,
   artifacts/quill-reference/snoopy-sound-full-candidate.json and
   artifacts/quill-sound-independent-decode/report.json. The reusable machine-local
   check script is artifacts/check-quill-sound-decoding.py. File decoding does not
   establish native-player gapless looping, seek/restart, spatial gain/attenuation
   or waveform identity after lossy encoding. These remain sound acceptance work.
5. Converter run 39671797855 (9b619ca5) is queued with no reported failure at this
   check. Prior sound-API run 39669262483 remains active with the SDK CMake archive
   failure from section 6.28; its isolated retry is still deferred until that run
   finishes. No new source failure was observed and no required gate was changed.

### 6.31. SharpQuill adapter loss-report contract (2026-10-21)

1. Added opt-in ReadImmAsSequenceWithReport returning SharpQuillReadResult with
   Sequence, Succeeded, IsPartial and a read-only Issues collection. Issues identify
   code, source layer ID/name/type and message. Existing ReadImmAsSequence and
   WriteImmAsQuillProject signatures/output remain unchanged; no new authoring API
   calls are made by the legacy read path. The report path uses current authoring
   queries on the already loaded document, without decoding the file a second time.
2. Reports explicitly identify omitted sound/spawn/unsupported layers, picture
   exclusion, flattened groups/nesting, uncopied key channels/timeline properties,
   chapter-only frame selection and default sequence/root/viewpoint reconstruction.
   Source root metadata is conservatively marked not reconstructed, so successful
   reported reads remain partial; this is not a lossless-conversion certificate.
   Missing diagnostic entry points produce DiagnosticUnavailable rather than an
   empty success report. Unknown brushes using the existing Cylinder fallback and
   failed layer/frame/stroke/transform reads are identified on the reporting path.
3. The Windows native adapter regression passes against sample1.imm for both
   picture options: legacy output remains 30 paint layers, zero/one pictures.
   Reporting identifies all 3 omitted sounds and spawn layers by native identity,
   every observed flattened group/nesting and uncopied key/timeline case. Picture
   exclusion follows the option; chapter-subset reporting follows selection. A
   missing input returns a failed result with LoadFailed. The valid fixture has no
   unexpected read failures or unavailable diagnostic APIs. Logs are ignored
   artifacts/adapter-loss-report-accepted*.log. Native source/binaries are unchanged.
4. This advances R1's explicit consumer contract. It does not reconstruct hierarchy,
   copy keys, decode sound for SharpQuill, map spawn areas or fix the Quill writer's
   picture/sound/model filtering. R2 and live authoring acceptance remain open.
   Managed Unity/platform validation of this additive API still requires CI.

### 6.32. Quill project picture serialization (2026-10-21)

1. QBinWriter and QuillSequenceWriter now write Picture payloads and implementation
   metadata instead of filtering them from group children. RGB8/RGBA8 dimensions,
   alpha, raw pixels, ViewerLocked, projection, import path and rewritten data
   offsets are retained. The emitted 20-byte header follows fixed bytes observed
   in local Quill 2D and stereo source images. Supported picture metadata covers
   2D, equirectangular mono and stereo; no eye-layout interpretation is introduced.
2. Preflight checks picture dimensions/exact byte count and supported projection
   before creating directories or replacing project files. Depth/Unknown pictures,
   Sound/Model/unknown layer types and nonempty Gallery pictures are explicitly
   rejected instead of silently filtered. This is validation of the provided
   sequence; legacy IMM adapter reading still omits sound before the writer sees
   it. This change does not make that legacy combined path lossless.
3. Synthetic project read-back passes for nested RGB/RGBA pictures in all three
   projection types, including alpha, viewer lock and static metadata. Observed
   header bytes and rewritten payload offsets are checked. Invalid/missing/oversized
   image buffers, unsupported projections and model children preserve existing
   Quill.json, Quill.qbin and State.json. Invalid direct QBinWriter picture calls
   emit no partial header. These are data-format checks, not application proof.
4. Real sample1.imm picture pixels and picture metadata survive native adapter ->
   Quill project -> production reader exactly. Reader/adapter and complete local
   Quill-to-IMM/authoring suites pass; paint/sound construction remains unaffected.
   Evidence is in ignored artifacts/quill-picture-writer-* logs and the adapter's
   generated read-back project beside its log. No native ABI/binary changed.
5. Quill application acceptance, depth, Gallery pictures, sound/model project
   writing, adapter hierarchy/key reconstruction and broader reverse conversion
   remain open. Mono/stereo metadata round trips do not establish native IMM stereo
   appearance or source stereo conversion; the missing paired Pano export remains
   a separate evidence gap. Full CI for this managed writer change is pending.

### 6.33. Quill project PCM16 sound serialization (2026-10-21)

1. QBinWriter and QuillSequenceWriter now serialize supported LayerSound children.
   SoundData.AudioBytes must contain mono/stereo PCM16 WAV at 8-192 kHz. A bounded
   RIFF/chunk inspection derives channels/rate/frame count from the actual bytes,
   validates PCM format/alignment/byte rate and complete nonempty frames, and writes
   the observed Quill 20-byte sound header plus exact interleaved PCM samples.
   Ancillary WAV chunks are not stored in qbin; the source WAV container itself is
   not promised byte-identical when it contains extra chunks. No resampling occurs.
2. Sound type, gain, loop, attenuation mode/range and one None/Cone modifier are
   serialized with metadata and rewritten offsets. Validation follows the observed
   Flat/Positional profile, rejects invalid enums/nonfinite or negative settings,
   invalid cone placement and unused None values before replacing project files.
   Source four-channel/ambisonic, extensible/float/compressed WAV and encoded
   Vorbis/Opus SoundData remain unsupported; model/depth/Gallery cases still reject.
3. Mixed picture/mono/stereo project write/read-back passes exact PCM/sample rate,
   channel layout, gain/loop/attenuation/cone and visibility checks. Raw qbin header
   fields and payload boundaries match the observed layout. Odd-sized ancillary
   WAV chunks are handled without changing PCM samples. Malformed WAV, unsupported
   formats/channels, invalid settings and modifier cases preserve all three existing
   project files. Invalid direct sound writes emit no partial qbin header.
4. The optional local corpus check passes all 21 metadata trees, 35 picture settings
   and 34 sounds; every sound's PCM payload also passes qbin write/read-back exactly.
   Existing reader/picture writer, adapter and full converter/authoring suites pass.
   Evidence is in ignored artifacts/quill-sound-writer-* logs. Native source, ABI and
   binaries are unchanged; these are local data checks, not platform/app acceptance.
5. This completes sound serialization only for valid supplied PCM16 SoundData.
   The native IMM-to-SharpQuill adapter still omits native sound, as its opt-in
   report identifies. Decoding encoded native audio into Quill PCM, hierarchy/key
   reconstruction, Quill application acceptance, evaluated/native playback and the
   other full-roadmap requirements remain open. Full CI for this change is pending.

### 6.34. Hierarchy-aware SharpQuill reading and fidelity baseline (2026-10-21)

1. Added ReadImmAsHierarchyWithReport as a separate mode. It rebuilds ordered
   groups/paint/supported pictures using authoring layer IDs and parent/child indices,
   copies local transforms/pivots/flips, root properties, background color, stored
   paint rates/repeats/drawing reuse/held frames and decoded point attributes. It
   does not bake world transforms or insert a synthetic default viewpoint. Legacy
   flattened read APIs and consumers retain their behavior. No native ABI changed.
2. Copies visibility, opacity, offset and combined-transform keys with explicit
   native-to-SharpQuill interpolation mapping for None/Linear/Smoothstep/EaseIn/
   EaseOut. Unverified channels (including actions/components/draw-in timing) and
   Spline/Auto keys are reported, not silently reinterpreted. Keys/offsets outside
   SharpQuill's integer fields and duration/repeat/start-offset values that cannot
   fit its float fields exactly cause a failed result with ValueNotRepresentable.
3. Missing original export range/startup/default navigation stays reported. A native
   document frame rate of zero selects an explicitly reported 24 fps Quill sequence
   default while retaining paint rates. Native root flags are retained in memory;
   a false timeline root is reported because Quill project reading normalizes it.
   Unsupported sound/spawn/model layers, picture projections and branches under
   unsupported parents remain explicit omissions. Stroke IDs are reconstructed;
   native quantization scale and per-point length/time are not explicit Quill fields.
4. Real sample1.imm checks pass for 36 groups, 30 paint layers and 58,405 points:
   ordered hierarchy, local transforms/pivots, supported root/layer keys, stored
   paint rates/repeats/frame maps and all decoded point attributes match native
   queries. A controlled nested-group/picture fixture passes project read-back for
   holds, pivots/X/Y/Z flips and all five interpolations. Unsupported key/channel
   diagnostics and exact float/integer timing guards pass. A Unity Animation name
   collision was caught with improved stand-ins and corrected before publication;
   local adapter and full converter/authoring suites pass.
5. Strict native -> Quill -> native codec equality remains unaccepted: a controlled
   drawing's stored scale is 3, while re-export computes 3.0000916 from decoded
   widths. The legacy adapter produces the identical drift, proving this is not
   introduced by hierarchy reconstruction. Reader-reported stroke bounds already
   span about 3.0000917, so copying those bounds cannot recover the original scale.
   The new report exposes potential requantization; existing C1 exact comparison
   gates remain intact (their failure message now includes both scale values).
   Data read-back success is not native re-encoding identity or visual acceptance.
6. Evidence is in ignored artifacts/hierarchy-mode-* and hierarchy-adapter-* logs
   and fixture projects. The profile suite records the legacy/hierarchy codec
   baseline instead of claiming strict native equality. R2 still needs native audio
   decoding, broader field/model coverage, codec/fidelity decisions, Quill application
   and playback acceptance. Current managed changes need full CI; prior sound API
   run 39669262483 is still active with its SDK-install failure and deferred retry.

### 6.35. Offline native sound decoding foundation (2026-10-21)

1. The reader now decodes stored WAV, Vorbis and Opus to interleaved PCM16 through
   DecodeSoundPcm16, CopyDecodedSoundPcm16 and ReleaseDecodedSoundPcm16. Positive
   64-bit handles own immutable PCM independently of document lifetime; release or
   End frees them. Invalid/stale handles cannot copy or release another allocation.
   Errors distinguish invalid data, output limit, allocation and unsupported format.
   The caller's positive byte limit bounds decoded PCM, not input or codec scratch.
   Decoding is synchronous and offline; player/per-frame decoding is unchanged.
2. WAV supports inspected PCM8/16/24/32 and float32 with explicit PCM16 conversion;
   nonfinite floats reject. Vorbis uses the existing synthesis library. Opus uses
   existing multistream decoding at 48 kHz with pre-skip, initial granule offset,
   EOS trimming and header gain. Malformed codec packets reject even when their
   container/header passes the encoded-asset inspector.
3. Windows Opus DLL ownership moves from player to reader with its Unity GUID
   retained. The player already depends on the reader package and preloads shared
   dependencies before its encoder DLLs. Packaging checks reject obsolete duplicate
   player Opus copies. A fresh child process loaded only an isolated reader package
   and decoded Opus using that package's DLL. No reader-to-player dependency exists.
4. Fresh Windows native builds, the full QuillToImm suite, package layout and export
   checks pass (120 player/exporter, 50 reader declarations). Tests cover byte caps,
   exact PCM16 WAV, other WAV profiles, Opus gains/offsets/end trimming, corrupt
   Vorbis setup, copy bounds, double release, document unload and End/re-init lifetime.
   Existing audio pipeline contracts and sample-project entrypoint checks pass.
5. Independent checks: five generated Opus profiles (1/2/4 channels) matched FFmpeg
   frame counts and differed by at most one PCM16 unit. Five 0.1-second Vorbis
   profiles yielded 4,410 frames in both this decoder and libvorbisfile, with at most
   one unit difference. FFmpeg returned 4,282 matching-prefix frames; this decoder
   comparison does not establish audible timing or native playback acceptance.
   Evidence: ignored artifacts/sound-pcm16-commit-suite*, sound-pcm16-final-native-build.log,
   sound-pcm16-final-exports*, sound-pcm16-final-*-layout.log,
   sound-pcm16-independent-all.json and sound-pcm16-vorbisfile-independent.json.
6. This foundation is not yet connected to ReadImmAsHierarchyWithReport: it still
   reports/omits native sounds. Next, map supported mono/stereo spatial settings and
   PCM WAV SoundData, release leases in finally blocks, and report unsupported
   ambisonic/frustum/legacy fields and resource failures explicitly. R2 remains open.
   Windows standalone reader decoding is verified locally; other platforms require
   CI. The iOS reader static archive does not itself merge dependency archives;
   standalone reader-only iOS link completeness is unproven. No Web decoder or
   Quill application/playback acceptance is claimed.
7. Latest live check still shows sound-import run 39669262483 active/queued with
   three iOS jobs waiting and the Android SDK-install failure from section 6.28.
   GitHub rejected the earlier isolated retry while the parent is active; the one
   permitted retry remains deferred. Older compiler/Godot failures are closed by
   passing replacement runs (section 6.26). This milestone needs fresh full CI.

### 6.36. Windows fixture dependency regression correction (2026-10-21)

1. Decoder run 39685893797 failed Windows job 119519720370 with a stale player
   Opus staging path after section 6.35 moved ownership to the reader. This was
   initially attributed to the face-orientation generator; section 6.38 corrects
   that diagnosis using the actual traceback. Remaining work was cancelled;
   fresh full validation is required after the source correction.
2. The generator now stages Opus from the reader package. Local reproduction also
   exposed loaded dependency references preventing temporary DLL deletion. Native
   generation runs in a bounded child process; parent cleanup follows child exit
   instead of guessing Windows loader reference counts. No validation gate changed.
3. Local face-orientation generation passes (6,344 bytes), and the existing
   urp_content_fixture_roundtrip CTest passes. Evidence: ignored
   artifacts/sound-pcm16-face-orientation.log, sound-pcm16-urp-fixtures.log and
   sound-pcm16-windows-failed.log. Hosted Windows acceptance remains pending.

### 6.37. Hierarchy reader native sound mapping (2026-10-21)

1. ReadImmAsHierarchyWithReport now maps native mono/stereo WAV/Vorbis/Opus assets
   to PCM16 WAV SoundData. Supported Flat/Positional, gain/loop, attenuation and
   None/Cone settings accompany ordered hierarchy, local transforms/pivots/flips,
   common timing/visibility and the existing supported key channels. Legacy flat
   readers are unchanged. This supersedes the adapter audio gap in section 6.35.
2. Ambisonic layouts and frustum/unrepresentable modifier settings produce typed,
   layer-specific omissions. Each mapped sound reports original codec bytes/import
   path and native legacy source-duration/initially-playing fields as unpreserved.
   Decoded PCM is not a promise of codec identity or audible timing equivalence.
3. Optional maxDecodedSoundBytes defaults to 256 MiB across decoded PCM in the
   returned sequence. It does not bound total memory, encoded assets or codec
   scratch. Nonpositive input throws; exhausted budgets and invalid assets fail
   without returning a partial sequence. Native leases release in finally blocks,
   including failed copy/allocation paths. Platform entrypoint absence is reported.
4. The fresh full local suite passes with generated Opus/Vorbis, WAV and hidden
   sound cases plus sample1's three Vorbis sounds. Checks compare exact decoded
   samples, spatial/common properties, local transforms/pivots, supported key counts,
   project payload read-back, exact aggregate cap and one-byte-short failure.
   Ambisonic/frustum omissions and malformed-asset failure are explicit. Evidence:
   ignored artifacts/hierarchy-sound-commit-suite*.
5. Integrated Snoopy native candidate (section 6.30) also passes: all three Opus
   sounds survive alongside its paint/groups as exact decoded PCM through Quill
   project write/read-back, including aggregate budget checks. Evidence: ignored
   artifacts/hierarchy-sound-Snoopy-final*. Original source camera omission and
   reported native spawn omissions remain; this is not complete project fidelity.
6. R2 still needs broader fields/models, quantization/fidelity decisions, source
   startup/navigation, Quill application and playback acceptance. Windows decoder
   support is locally verified; cross-platform gates and reader-only iOS linking
   remain pending. Regression correction aa1e334f is in fresh full run 39686518984;
   its passing replacement Windows gate must be verified, not inferred locally.

### 6.38. Complete shared Opus test-staging correction (2026-10-21)

1. Replacement run 39686518984 Windows job 119521854740 failed with the same stale
   Opus dependency path. The traceback identifies exporter_bridge_smoke.py, not
   the face-orientation generator corrected in section 6.36. That earlier fix was
   incomplete; its locally passing fixture checks did not cover the failing harness.
2. The exporter bridge dependency list now stages reader-owned Opus. Its benchmark,
   Unicode-path, logging and viewpoint test consumers share this list. A targeted
   scan of Windows test/build staging found no other obsolete player Opus path.
   Godot has separate unchanged dependency packaging. A new CI check verifies that
   bridge and fixture staging agree, have unique DLL names, resolve existing files
   and select reader-owned Opus. No gate was weakened.
3. Actual exporter bridge export/read-back (11,696 bytes), Unicode-path and native
   logging checks pass locally, as does the new dependency check. Evidence: ignored
   artifacts/opus-owner-exporter-smoke.log, opus-owner-unicode-smoke.log,
   opus-owner-native-logging.log and opus-owner-dependency-test.log.
4. Cancelled remaining runs 39686518984 and 39686749499 because both contain this
   source defect. A fresh full run is required; the regression remains unaccepted
   until replacement Windows validation passes. This is separate from the older
   Android SDK installation infrastructure failure and its deferred isolated retry.

### 6.39. Reviewed hierarchy conversion in Unity editor menus (2026-10-21)

1. All three existing IMM-to-Quill menu commands now consume the hierarchy reader
   instead of the legacy flat adapter. Supported groups, paint, pictures, sounds
   and keys therefore reach project writing. The public legacy flat API remains
   unchanged. The picture choice now says Exclude Pictures rather than Paint Only.
2. Before writing, the menu logs every typed issue and shows a review of conversion
   limitations and the destination. Existing destination content is explicitly
   called out for replacement. Declining leaves existing files unchanged and does
   not create a new output directory. Batch summaries distinguish skipped files
   from failures and reveal only a successfully written output folder.
3. A written project includes IMM-conversion-report.txt with source, destination
   and all issue codes/layer identities/messages. Failed reads never reach writing
   or request write confirmation. This exposes current spawn/root/codec and other
   limitations; it does not complete their reconstruction or Quill app acceptance.
4. The production editor source now compiles in the existing .NET/native harness
   with simulated Editor interaction. Tests exercise declined overwrite, declined
   new destination, accepted hierarchy/sound project read-back, complete saved
   reports and malformed-read output preservation. The full local suite passes;
   this is not live Unity Editor or Quill application validation. Evidence: ignored
   artifacts/editor-hierarchy-commit-suite*.
5. Windows smoke regression correction 4ec3f148 needs replacement hosted acceptance
   (section 6.38). This editor milestone also requires fresh full CI. Pending gates
   and the earlier Android SDK infrastructure failure are not passing results.

### 6.40. Sound-aware independent adapter regression check (2026-10-21)

1. Run 39687422704 passed the corrected shared DLL staging and reached the separate
   SharpQuillAdapter suite. Windows job 119524924234 failed its hierarchy child
   count check: the expected list still excluded sounds after section 6.37 added
   them. The QuillToImm suite did not cover this independent harness expectation.
2. The independent adapter check now includes all three supported sample1 sounds
   in ordered child comparisons. It verifies their common properties/keys, exact
   decoded PCM/rate/channels and gain/loop/attenuation/modifier settings rather
   than removing the child-count gate. Legacy flat omission tests remain unchanged.
3. The failing suite passes against the committed pre-viewpoint adapter source:
   36 groups, 30 paint layers, three sounds and 58,405 points. Its existing
   AdapterSource override selected an ignored copy of the committed Git object;
   no temporary dependency reference is committed. SharpQuillReader regressions
   and the reader authoring API smoke check also pass. Evidence: ignored
   artifacts/sound-adapter-regression.log, sound-adapter-reader-regressions.log
   and sound-adapter-authoring-smoke.log.
4. Cancelled remaining work in runs 39687422704 and 39687578973 for this source/test
   contract failure. Push a fresh full validation; hosted acceptance remains open.

### 6.41. Reverse spherical viewpoint metadata and default selection (2026-10-21)

1. The hierarchy adapter now maps spherical spawn areas to Quill viewpoints with
   volume offset/radius, floor/eye tracking, movement permissions and Exporting=true.
   Existing ordered hierarchy, local/common metadata and supported keys also apply.
   A uniquely representable reader-selected default becomes its full Quill path;
   duplicate paths, slash/empty names, nonunique or omitted defaults are reported
   without silently choosing a different default. Legacy flat reads are unchanged.
2. Box volumes remain explicit omissions. Original gizmo color/version/show-volume
   metadata is unavailable and uses reported defaults. Native availability is not
   Quill gizmo visibility, so navigation equivalence is not claimed. Stored encoded
   previews remain exposed by the reader but are explicitly reported as unwritten
   by the reverse project writer; decoded preview writing remains the next gap.
3. Fresh local QuillToImm and independent SharpQuillAdapter suites pass. Tests cover
   sphere/tracking/movement, hierarchy/reflections/keys, project read-back, stored
   preview/box reports and unique/ambiguous/omitted-default paths. The independent
   sample comparison now checks viewpoint settings as well as geometry and sounds.
   Evidence: ignored artifacts/viewpoint-hierarchy-final-suite.log and
   viewpoint-hierarchy-final-adapter.log. No live Quill/Unity or playback acceptance.
4. A synthetic nonzero pivot exposed a separate existing compiler gap: its spawn
   constructor accepts transform/tracking only; pivot position, visibility/opacity
   and timeline properties default natively (flip setup and keys still apply).
   The reader correctly reproduces the native zero pivot, not the original graph's
   position. Preserve these fields through an additive full-property constructor
   before claiming complete source spawn round-trip fidelity.
5. One initial local float-boundary fixture export setup failed before adapter
   reading with an undetailed message. Subsequent diagnostic and final suites
   passed that boundary. Setup assertions now distinguish root update/export and
   retain the underlying message. The initial cause is unestablished; do not call
   it a proven adapter bug or a fixed regression. Keep improved diagnostics active.
6. Latest checked replacement run 39688086871 (a8c4432c) has no reported failures,
   but Windows job 119527150527 remains running. The previous adapter-test failure
   is not closed until replacement Windows validation passes. This viewpoint
   milestone requires fresh full CI too. R2 and the complete shared roadmap remain
   open, including previews, common spawn serialization, codec/fidelity decisions,
   broader navigation and live authoring/platform/application acceptance.

### 6.42. Full common-property offline spawn construction (2026-10-21)

1. Added ImmExporter_CreateSpawnAreaLayerWithProperties and switched the offline
   compiler to it. Spawn pivots, opacity, timeline flag, duration and repeat count
   now reach native serialization. Transform/pivot flips still use the existing
   additive flip setter. This closes the constructor gap found in section 6.41;
   no live mutation or per-frame path changes.
2. Initial visibility has no Layerdes field. The new constructor encodes hidden
   startup state as a false time-zero visibility key; later explicit caller keys
   at zero replace that value. The reader's base visible field can remain true,
   while its keys retain effective hidden startup state. This is visibility-track
   preservation, not base-boolean identity. The original constructor still has its
   exact prior defaults and adds no new key.
3. Fresh Windows build, full local QuillToImm suite, legacy exporter bridge smoke
   and ABI verifier pass (121 player/exporter, 50 reader declarations). Checks
   cover nonidentity pivot/reflections, opacity/timeline/duration/repeats, hidden
   startup plus later key, explicit true-at-zero override, authoring import/re-export,
   Quill-project read-back, null sequence and old constructor defaults. Evidence:
   ignored artifacts/spawn-properties-final-suite.log, spawn-properties-legacy-smoke.log,
   spawn-properties-visibility-build.log and spawn-properties-exports.json.
4. Independent read-only source review confirmed native/managed argument widths,
   old-wrapper defaults and time-zero replacement behavior. Separate CI auditing
   found no failures: 39688086871 passed the previously failing adapter/converter
   steps and Android; its Windows job remains in the Godot build. Latest viewpoint
   run 39688927555 reached its adapter step. Both still have queued macOS/iOS jobs;
   neither is a completed full validation. Current API/binaries require fresh CI.
5. Reverse preview writing, unsupported volumes/source navigation, codec fidelity,
   Quill application/live-authoring and broader platform acceptance remain open.
   The one earlier undetailed local fixture-setup failure from section 6.41 has not
   recurred in the final full suites; its cause remains unestablished.

### 6.43. Reverse viewpoint preview project writing (2026-10-22)

1. Added authoring-indexed StrokeReader_GetSpawnAreaPreviewPixels. It decodes stored
   PNG/JPEG on demand into a complete caller-owned RGB/RGBA buffer, returning zero
   on failure or insufficient space without partial writes. Version-one previews
   receive the same BGR correction as the importer. Encoded bytes remain unchanged;
   no additional decoded cache or playback/per-frame behavior is introduced.
2. LayerViewpoint now has optional PreviewData. The project writer validates it
   before output replacement, writes the observed raw image header and pixels to
   qbin, regenerates the offset and emits it in JSON. Absent data clears stale
   offsets. Ordinary project reads hydrate preview pixels with bounded offset,
   format, dimension and payload checks; metadata-only reads leave data unloaded.
3. The hierarchy adapter now supplies those pixels. Its independent optional
   maxDecodedPreviewBytes budget defaults to 256 MiB across all decoded viewpoint
   preview pixels. This bounds retained output, not encoded input, codec scratch,
   other picture layers or total process memory. Nonpositive budgets throw;
   exhausted budgets or failed decoding fail without returning a partial sequence.
   Original compressed bytes/version changes remain explicitly reported losses.
4. Fresh Windows native build, complete local QuillToImm suite, independent reader
   and adapter suites and export checks pass (121 player/exporter, 51 reader).
   Checks cover PNG exact pixels/alpha, JPEG read-back, version-one BGR correction,
   file/memory queries, complete-copy/invalid-handle guards, aggregate exact/short
   budgets, absent/RGB/RGBA previews, mixed offsets, malformed payload rejection,
   stale-offset removal and invalid-writer output preservation. Integrated Snoopy
   still passes its sound/whole-project read-back checks; no visual fidelity claim.
   Evidence: ignored artifacts/preview-write-suite.log, preview-write-reader.log,
   preview-write-adapter.log, preview-write-Snoopy.log, preview-write-exports.json
   and preview-pixels-native-build.log.
5. Delegated writer implementation and a separate native/adapter review accompany
   the parent-owned integration; the combined local tests above use the actual
   connected path. Cross-platform exports, Quill application/rendered preview and
   playback acceptance still need CI/runtime evidence. This closes reverse stored
   preview writing, not generated previews, box/source navigation, codec identity
   or the remaining live-authoring roadmap.
6. Latest independent CI audit found no failed jobs: replacement 39688086871 Windows
   is successful (11 jobs passed, five queued). Viewpoint run 39688927555 passed its
   Windows source checks and is building Godot. Spawn run 39689819341 native Windows
   build passed but later source checks remain pending. macOS/iOS waits prevent
   full-run acceptance; this preview API also requires fresh full validation.
7. Older sound-import run 39669262483 remains nonterminal with its SDK-install
   Android failure and three queued iOS visual jobs. The one permitted targeted
   infrastructure retry remains deferred until GitHub allows it (section 6.28).

### 6.44. Evaluated animation comparisons and finite-loop restart correction (2026-10-22)

1. Added a CPU-only native comparison harness using ImportMetadataFromMemory with
   a no-op stroke collector and the actual MngrPlayer. It loads asset-table/frame
   metadata without decoding drawing geometry or creating renderers/audio engines.
   Supported group/paint/root full-path sets must match; other types are counted
   explicitly as outside this comparison scope. No production ABI was added.
2. Comparisons cover local/world visibility, opacity and transforms, paint timing
   metadata/playing and active drawing ordinal, at sequential samples, reverse paused
   seeks using recorded GetTime clock pairs, and restart. Self-history checks also
   compare these evaluated poses, ignoring expected paused-playing differences and
   inactive hidden-drawing storage. They do not identify individual held-frame
   indices when several frames select the same drawing. Relative tolerance is 1e-5.
3. A public synthetic native -> Quill -> IMM case exercises nested offsets/timelines,
   pivots/reflections, held drawings, visibility/opacity and five interpolations.
   It exposed a native incremental-playback defect at a finite ancestor wrap:
   opacity/drawing differed from fresh evaluation and restored-clock seeking, in
   both reference and candidate. This was native history behavior, not conversion
   drift. The incorrect initial scene-graph-only loader was replaced before these
   results; its earlier provisional comparison is not acceptance evidence.
4. Layer::SetStateAt now recognizes active finite repeated ancestors and the root
   as possible wrap sources. It excludes exhausted repeat windows, and never
   retimes the root's own start from its wrap. This restarts child timelines/sound
   scheduling at real wraps without turning finite roots into infinite loops or
   continuously retriggering children at a clamped end. No seek API change was made.
5. Fresh source-matched Windows reader/player builds, full QuillToImm suite, native
   chapter/viewpoint smoke and unchanged 121/51 export checks pass. The synthetic
   pair has zero pair/history errors at 36 times across three modes (108 samples),
   including finite-loop exhaustion. A deliberately changed transform is rejected;
   the negative gate requires pair errors with zero history errors, so setup
   failures or unrelated history regressions cannot pass as expected differences.
6. Native sound scheduling metadata tests cover root single/finite/infinite and
   nested finite repeats under single/infinite outer timelines, including stable
   start times after exhaustion. No audio assets, decoder or audible output are
   involved. Corrected paired comparisons pass for Fishgym (8 supported layers)
   and Snoopy (46), each at 23 times across three modes (69 samples), with zero
   pair/history errors. Their excluded viewpoint/media counts remain explicit.
7. Added required Windows CI positive/negative evaluated-state checks after fixture
   generation. Evidence: ignored artifacts/evaluated-state-final-positive.log,
   evaluated-state-final-negative.log, evaluated-state-fishgym-final.log,
   evaluated-state-snoopy-final.log, evaluated-state-commit-suite.log,
   evaluated-state-viewpoint-policy.log, evaluated-state-exports.json and
   finite-loop-root-clock-build.log. Source and test changes received independent
   review; temporary project references remain uncommitted and are removed by the
   runner. Native draw quantization and rendered/audio/device acceptance stay open.
8. Latest CI audit: preview run 39690789395 Windows passed with no reported failures;
   Android/browser work is still running and macOS/iOS gates remain incomplete.
   Spawn run 39689819341 Windows also passed. This playback correction requires
   fresh full CI. The older Android SDK infrastructure retry remains deferred
   (6.28/6.43). Source chapter/startup mapping and the remaining roadmap are open.

### 6.45. Structural source panorama transfer (2026-10-22)

1. Project conversion now recognizes `360_Equirect_Mono` and
   `360_Equirect_Stereo`, plus the existing reader's human-readable aliases,
   mapping them to native equirectangular types 1 and 2. Stored qbin pixels are
   transferred without repacking; common layer properties, animation and viewer
   locking continue through the existing picture path. Unknown/depth/cubemap
   projections still require explicit omission permission. Explicit horizontal
   or vertical FOV metadata fails before replacing output, including when
   omission permission is enabled; no verified cropped-panorama mapping exists.
2. Synthetic checks cover both projections and aliases, distinct upper/lower RGB
   and RGBA pixels, alpha, viewer locking, native import round trips, explicit
   FOV rejection and the existing malformed/budget/cancellation/output guards.
   Full converter/native .NET self-test passes. No native ABI or binary changed.
3. The local Pano source converts without omissions. Independent native reader
   verification finds a 4096x4096 stereo, viewer-locked picture; every decoded
   RGB pixel matches its stored qbin payload and output alpha is opaque. The
   original JPEG decodes differently, so it is deliberately not reimported.
   Evidence: ignored artifacts/panorama-structural-suite.log,
   panorama-Pano-conversion.log and panorama-Pano-pixel-check.json.
4. This establishes structural transfer, not Quill visual equivalence. Native
   stereo sampling uses vertical halves, but no matching official Pano IMM was
   found in the inspected local corpus. Eye order, panorama orientation,
   application/device appearance and mono reference coverage remain open.
   Conversion reports `PanoramaLayoutUnverified` explicitly. Source chapters,
   broader navigation and the rest of the roadmap remain open.
5. Independent regression audit found no new source failure: playback correction
   run 39693358402 has Windows/Android checks in progress; preview run
   39690789395 Windows passed, full validation pending. Old run 39669262483 still
   has the Android SDK archive infrastructure failure and active independent
   iOS jobs; its single affected-job retry remains deferred until terminal.
   These are snapshots, not a claim that full CI has passed. Fresh full CI is
   required for this panorama milestone.

### 6.46. Final chapter-stop completion correction (2026-10-22)

1. Chapter/startup audit found a native completion defect independently of source
   mapping: an authored Stop stores `stopTime = startTime + keyTime - 1`, but
   `MngrPlayer::GetIsFinished` compared the uncorrected relative stop time with
   the last Stop key. Reaching that key therefore remained Waiting instead of
   Finished. The new final-stop assertion fails on the previous implementation.
2. Corrected the comparison by the same one tick used by `GetTime`. The existing
   waiting gate and last-Stop policy are unchanged. Regression checks pass for
   final completion, held completion, restart clearing completion, intermediate
   stops waiting without completion, exact Play/chapter boundaries and Continue.
   Required Windows CI now runs the CPU-only chapter/viewpoint policy harness.
   This does not prove Quill chapter semantics or application/device behavior.
3. Windows player/reader build, existing full converter/native .NET suite and
   export verification pass. Evidence: ignored artifacts/chapter-final-stop-before.log,
   chapter-final-stop-after.log, chapter-final-stop-native-build.log,
   chapter-final-stop-suite.log and chapter-final-stop-exports.json. No ABI added.
4. Source mapping remains open: native Play is a chapter marker, Loop restarts
   through the manager, and MakeDefault affects spawn areas only. The local
   Chapters source includes root MakeDefault, so equivalent enum labels are
   insufficient evidence. A further native startup issue remains unresolved:
   Stop at tick 0 or 1 cannot satisfy the current `stopTime > startTime` waiting
   condition. Chapters begins with Stop 0; startup parity must not be claimed.
   Next investigate these early stops with explicit restart/Continue/seek cases.
5. Latest panorama run 39694228185 and playback run 39693358402 reported no source
   failures at this turn's audit but remain incomplete. Old run 39669262483 has
   now passed Unity iOS; Godot iOS remains active and the Android SDK archive
   infrastructure failure still awaits its eligible targeted retry. Fresh full
   validation is required for this native correction.

### 6.47. Early root stops and nested playback clocks (2026-10-22)

1. Reproduced native root Stop failures at ticks 0 and 1. Stop 0 shares its
   stored timestamp with the initial no-stop sentinel; timestamp comparisons
   cannot identify it. Introduced internal held-clock and authored-stop state,
   with explicit clearing on restart, resume, Continue and playing seek. Real
   pause/seek holds freeze time without requesting chapter input. Timeline clock
   fields are initialized. File format and exported C ABI are unchanged.
2. The root hold now freezes nested timeline evaluation, including grandchildren.
   New child starts use the held parent clock. Continue shifts active/held nested
   clocks by the root's start-time change, preserving local positions while leaving
   dormant timelines untouched. Seek resets child playback through the existing
   jump/reset path before restoring requested clocks; paused sound state is retained.
   Pause/resume during an authored root hold leaves nested clocks aligned. Normal
   unheld elapsed calculation keeps its direct path; no per-frame allocation or
   cache was added. This is correctness evidence, not a performance measurement.
3. Required Windows chapter/viewpoint harness now covers ticks 0/1 at zero/nonzero
   epochs, no premature stop, reported position, root/nested/grandchild holds,
   Continue without jumping ahead, delayed first evaluation after restart,
   playing seeks past/onto stops, pause/resume, final completion and a future
   visibility boundary. A no-action pause/zero-seek/resume check prevents held time
   from being mistaken for chapter input. Existing paused-seek/viewpoint checks
   also pass. Independent review identified the nested/dormant clock risks and
   found no material remaining issue after correction.
4. Windows player/reader build, full converter/native .NET suite and export checks
   pass. Evaluated-state positive comparison passes 36 times across three modes
   (108 samples), zero pair/history errors; deliberate mismatch remains rejected.
   Fishgym and Snoopy pass 69 samples each, within the existing paint/group/root
   scope and media/viewpoint exclusions. Evidence: ignored artifacts/early-stop-before.log,
   early-stop-nested-before.log, early-stop-after.log, early-stop-native-build.log,
   early-stop-suite.log, early-stop-evaluated.log, early-stop-evaluated-negative.log,
   early-stop-fishgym.log, early-stop-snoopy.log and early-stop-exports.json.
5. These native state checks close the early-stop defect recorded in 6.46 within
   their tested scope. They do not establish Quill root action/startup mapping,
   nonroot action semantics, audible playback, renderer/device behavior or live
   authoring acceptance. Chapters' root MakeDefault and the missing paired official
   export remain unresolved. The rest of C2-C4/R1-R2/A1-A3 remains as recorded.
6. Panorama run 39694228185, finite-loop run 39693358402 and final-stop run
   39694746959 have passed Windows. Full validation remains incomplete. Old run
   39669262483 first attempt finished: its Android SDK archive failure prevented
   dependent packaging/device/engine evidence; no independent source failure was
   found in the evidence-report failure. macOS/iOS builds and all three iOS visual
   gates passed. The one targeted Android job/dependents retry was accepted and
   verified in progress as attempt 2, with successful independent jobs retained.
   Evidence: artifacts/old-android-targeted-retry-state.json. A repeated failure
   remains unresolved; do not retry again indefinitely. This clock correction
   requires its own fresh full CI.

### 6.48. Live publication timing infrastructure and current baseline (2026-10-22)

1. A1 audit confirmed that the shared live probe timed queue/Commit calls, then
   polled status after three frames. Added internal monotonic timestamps at native
   Document Commit entry and first Preparing/Prepared/Presented/Rejected transitions.
   The opt-in probe reports those intervals and checks monotonic ordering; its
   polling delay is excluded from publication timing. No exported C ABI changed.
   Timestamp work runs on authoring transitions, not ordinary unedited playback.
2. Rebuilt the Windows player/reader/viewer. A fresh standalone DirectX sample1
   run reaches revision 13; the initial two-element/16-point edit reports 0.175 ms
   from internal Commit entry to publication. CPU/GPU/pre-presentation injection
   each rejects with no Presented timestamp and unchanged target drawing bounds.
   Captured renderer frame traces report zero geometry mismatches. The first
   short run ended at async load completion and was inconclusive; corrected runs
   use a longer frame budget. Empty child environment flags also invalidated an
   initial injection attempt; only runs with explicit environment removal are used.
3. These are diagnostics, not M1 latency acceptance: the live edit is not matched
   to the Small/Medium/Large full-reload workload, and there is no repeated live
   median/p95 yet. Internal publication excludes prior ABI validation/copy, display
   latency, GPU completion and retirement. Prepared boundaries differ by operation,
   so its interval does not isolate CPU/GPU stages. No M1/M2 completion is claimed.
4. Fresh 30-iteration compile/reload summed stage medians: Small 1.892 ms (10
   drawings/160 points), Medium 50.209 ms (400/12,800), Large 841.438 ms
   (4,000/256,000). The slower Large historical comparison was investigated with
   isolated five-sample parent/current/parent runs using identical reader and
   generated fixture hashes: summed-stage sample medians 830.027 / 809.443 /
   782.320 ms. No slowdown due to this timing change was identified within that
   comparison; the unmatched historical difference remains unclassified. Stage
   sums are not end-to-end wall-clock observations. Benchmark CLI now selects
   exact exporter/reader binaries and records their hashes, fixture hashes, raw
   samples and nearest-rank p95, enabling future matched comparisons.
5. Full converter/native .NET suite, export verification and existing live-edit
   wiring checks pass. Independent review found no material timing/C-ABI defect.
   Evidence: ignored artifacts/live-timing-native-build.log, live-timing-viewer-build.log,
   live-timing-{normal,cpu,gpu,present}.log, live-timing-validation-summary.json,
   live-timing-baseline.json, live-timing-parent-comparison.json,
   live-timing-suite.log, live-timing-contract.log and live-timing-exports.json.
   Next build repeated matched live-edit cases with visible workloads, phase timing,
   artifact provenance and per-frame revision checks. Full A1-A3 and converter
   startup/chapter/application acceptance remain open.
6. Latest clock run 39696568808 passed Android; Windows remains in its Godot build,
   with no reported failure. Old run 39669262483 attempt 2 has now passed replacement
   Android job 119554846760: the original SDK archive failure is cleared by its
   sole targeted retry. Downstream Unity/device/engine validation remains active;
   the full run is not yet passing/complete. This instrumentation needs fresh CI.

### 6.49. Repeated matched live publication benchmark (2026-10-22)

1. Added repeated single-drawing geometry publication measurements
   against the Small/Medium/Large compile/reload corpus. Initial frame mappings
   referenced only drawing zero, narrowing player residency despite retaining
   serialized drawings; those timings are rejected as acceptance evidence.
2. Corrected fixtures reference every drawing and hold playback at frame zero.
   Large initially exposed the importer's five-second initial buffer followed by
   asynchronous remaining drawing loads. Benchmark startup now waits for every
   expected single-stroke drawing to be resident. Decoded reader inventory verifies
   every frame reference, drawing, stroke and point count. The report requires
   paused-at-zero playback, each target revision's in-frustum model/renderer
   geometry match, unchanged sampled unrelated revisions and positive render/capture
   checks. It rejects missing, invalid or inconsistent measurements.
3. Windows DirectX / RTX 4090, five warmups then 30 measured edits: publication
   median/p95 in ms is Small 0.1293/0.2066, Medium 0.08235/0.2291 and Large
   0.08705/0.1611. Resident strokes are 10/400/4,000; points are
   160/12,800/256,000. Matched compile/reload summed-stage medians are
   1.376/54.199/836.243 ms. These endpoints differ; no speedup ratio is claimed.
   Publication excludes ABI copying, display latency, GPU completion and retirement.
   This is one geometry operation/backend, not A1/M1 or full roadmap acceptance.
4. Added a required Windows correctness smoke with one warmup/three measured
   Small edits and retained provenance/log/capture artifacts. It checks correctness
   without machine-dependent latency thresholds. Native viewer build, nine report
   guard tests, existing live-validation contract, full converter suite, native
   chapter/early-stop policy, Windows declared-binding checks (121 player/51 reader) and the exact
   new CI command pass locally. Evidence: ignored artifacts/live-benchmark-v2-*
   and artifacts/live-benchmark-ci-*; failed v1/pilot timings remain inconclusive.
5. Timing run 39698167460 and clock run 39696568808 have passed Windows and
   Android but remain incomplete. Sound-import run 39669262483 attempt 2 has
   cleared its original Android SDK archive failure, but Android Vulkan job
   119565006528 failed: required Firebase markers and captures are missing.
   Upstream Firebase jobs 119561639997 and 119561639999 report success. This
   new evidence failure is unresolved; independent producer/collector investigation
   takes priority. Producer summary has passed=false despite gcloud exit zero.
   Early missing-buffer messages are followed by offscreen target creation and
   successful native paint submissions; they do not establish the final cause.
   The smoke readiness check still fails afterward. The one targeted retry has
   already been used.

### 6.50. Android Vulkan smoke readiness investigation (2026-10-22)

1. Full Firebase producer logs narrow the failure in 39669262483: early
   missing-color-buffer messages precede creation of a 2316x1080 offscreen target.
   Native Vulkan paint pipelines and draw submissions then succeed, but the smoke
   later reports native paint not ready and exits without required captures.
   This is not established as infrastructure failure or the local 1x1 display
   portability issue. Firebase exit zero alone does not establish validation success.
2. The smoke previously sampled performance counters once. Native render resets
   triangle counters before repopulating them, so asynchronous observation is a
   possible cause, not a proven diagnosis. Added a five-second bounded readiness
   wait requiring positive paint calls AND triangles in the same observation,
   with actual counters, elapsed time and sample count in ready/timeout logs.
   Timeout still exits before capture with failure; all later capture and visual
   classification gates remain required. No native performance behavior changed.
3. Added a Unity-independent policy test for strict same-observation readiness,
   delayed readiness and terminal deadline failure, required in Windows CI.
   Local policy and relevant Firebase/classifier contracts are checked separately
   from Unity compilation/device execution. The Android failure remains unresolved
   pending fresh full CI and successful replacement visual evidence.

### 6.51. Current-source C# verification and package regression audit (2026-10-22)

1. The full player regression command exposed stale Unity-generated source lists:
   newer image/audio source files existed but were absent from the generated
   runtime project. The verifier now writes an artifact-only MSBuild source overlay
   for each default runtime assembly, discovering current assembly-owned C# files
   and excluding nested asmdef/asmref directories. Unity's references/defines and
   generated project files are preserved. The overlay is project-scoped so it
   cannot replace a referenced project's sources. This does not replace Unity import.
2. Both current package runtime assemblies now compile. Two verifier tests cover
   new-source inclusion, nested assembly exclusion, referenced-project isolation
   and preservation of the generated project; required in core CI. Exporter bridge,
   SharpQuill adapter and sample content baseline also pass the full regression
   command. Evidence: artifacts/live-a1-current-csharp.log,
   live-a1-csharp-verifier-tests.log and live-a1-player-regressions-current.*.
3. The full regression command remains failed because packaged macOS binaries
   lack newer declared exports. Windows exports pass; this is not all-platform
   acceptance. Replacement run 39701694123 is live with no failures at this audit,
   but Apple builds remain queued. Old 39669262483 is completed/cancelled.
   Do not skip or weaken the all-platform export check to report a passing set.

### 6.52. Native pending-authoring cancellation regression (2026-10-22)

1. Added a separate native Document harness using a paint-free group/viewpoint
   fixture and the real player/importer code. It interrupts group creation while
   Queued and after CPU Prepared, before GPU publication. Canonical hierarchy
   stays unchanged, unload invalidates editing/status/revision queries, reload
   contains no cancelled creation, and a fresh session can publish/delete a group.
2. Invalid parents, root/self reparenting, unsupported deletion of a group with
   viewpoint descendants and deleted layer IDs are rejected without hierarchy
   mutation. This does not establish stale drawing-handle behavior, allocator
   leak freedom or GPU allocation/upload/retirement cancellation. Those A1/M1
   failure-matrix items remain open; the live benchmark alone does not close them.
3. Isolated Windows native build and execution pass, with a required Windows CI
   step added. Evidence: artifacts/authoring-failure-suite.log and
   artifacts/authoring-failure/build.log. No production player behavior changed.

### 6.53. Paint-owning subtree lifecycle work (2026-10-22)

1. Extended deletion from group-only subtrees to non-timeline static
   paint/group descendants. CPU preparation collects the complete layer/drawing
   set; GPU publication preflights all renderer slots and retirement capacity before
   hierarchy removal. Drawing ownership transfers without frame-reference filtering,
   followed by handle invalidation and empty-layer destruction. Active asynchronous
   import and instance links into the target subtree are rejected. Other resource
   types remain explicitly unsupported; full M2 lifecycle acceptance stays open.
2. Source review found that existing replacement/deletion retirement push_back
   could allocate after model mutation. Retirement capacity is now preflighted
   before mutation, including an immediate single-deletion publication recheck.
   Opt-in retirement-preparation failure injection is added.
3. Windows native build and a real DirectX fixture pass: nested groups contain
   two paint layers/five referenced drawings, with an unaffected sibling. Deletion
   removes all target layers/handles, shrinks document bounds, preserves sibling
   handles/bounds/rendering, and completes all five renderer retirements. Forced
   retirement-preparation failure preserves hierarchy, handles, bounds and presented
   revision. The same injection rejects a drawing replacement before geometry
   publication. Required Windows CI now runs these three cases and retains logs,
   settings, fixture, capture and provenance. This is not cross-backend acceptance.
4. Full converter suite, CPU authoring cancellation, existing live contracts and
   Windows declared-binding checks (121/51) pass. Player bridge/adapter/content/C# checks pass;
   all-platform export verification remains failed on stale macOS binaries.
   Repeated 30-edit publication regression passes all three fully resident corpus
   cases (p95 0.2397/0.3856/0.1760 ms); endpoints retain section 6.49's limits.
   Evidence: artifacts/paint-subtree-final-build.log, paint-subtree-final-run/,
   paint-subtree-converter-suite.log, paint-subtree-authoring-final.log,
   paint-subtree-exports.json, paint-subtree-player-regressions.* and
   paint-subtree-live-regression/. Reader rebuilds had unchanged code sections;
   timestamp/debug-metadata-only binary changes were omitted.

### 6.54. Paint layer creation lifecycle work (2026-10-22)

1. Added Paint=1 support in the existing live LayerCreate ABI
   (Group=0 unchanged). A new paint starts visible/identity/opacity 1 with one
   initialized empty drawing and frame zero mapped to it, repeat 1, version 1,
   and the document frame rate (30 when unset). This makes existing drawing edits
   usable immediately after publication without adding a second construction ABI.
2. Detached paint/geometry and renderer-slot preparation precede atomic hierarchy
   and stable drawing-handle publication. Failure/unload cleanup cancels the slot
   before destroying either drawing or the pending layer. Partial drawing buffer
   initialization and failed drawing-vector growth now release successful allocations.
   Real DirectX checks pass: create an unpublished empty paint, publish its frame
   and stable handle, populate it with visible geometry through that handle, then
   delete it. Sibling handles/bounds remain stable and document bounds restore.
   CPU/GPU/presentation injection keeps the failed creation unpublished, with
   prepared-slot cancellation verified for GPU/presentation rejection.
3. Paint subtree run 39704338933 has passed Windows and Android with no failures;
   its five Apple jobs remain queued. Readiness run 39701694123 has the same
   terminal/queued split. Neither run is full platform acceptance, and the older
   Android readiness failure remains unresolved pending replacement device results.
4. A separate opt-in hold keeps paint creation in Preparing after CPU renderer-slot
   preparation. The test unloads the document, observes slot cancellation and old
   session invalidation before reloading the fixture, then checks restored content,
   rendering and a fresh editing session. This proves pending CPU-slot cancellation,
   not cancellation during an in-flight GPU upload. Five cases are required in
   Windows CI with logs/settings/captures/fixture and binary/backend/GPU provenance.
5. Native build, exact new CI command, full converter suite, bridge, chapter/early-stop
   policy, live contracts and Windows binding checks pass locally. The player set
   passes bridge/adapter/content/C# and remains failed on stale macOS exports.
   Windows binaries export 147 player functions and 51 reader functions; the
   verifier checks 121 declared player bindings and all 51 reader bindings. No new
   function signature/export was added by Paint=1. Evidence:
   artifacts/paint-create-pending-build.log, paint-create-ci/,
   paint-create-ci-command.log, paint-create-converter-suite.log,
   paint-create-policy-regression.log, paint-create-player-regressions.* and
   paint-create-exports.json. Other lifecycle types, broader M1/M2 evidence,
   revision-selected save and managed live cutover remain open.

### 6.55. Layer/sequence ownership correction (2026-10-22)

1. Lifecycle review found two pre-existing teardown defects: Layer::Deinit did not
   end allocated full-name strings, and Sequence::Deinit ended its raw pointer
   registry without deleting the registered Layer objects. Teardown now releases
   full names, deinitializes every registered layer before deleting any, then clears
   root/default-spawn pointers and registry storage. Explicit live deletion already
   removes its layers from the registry, so ownership is not duplicated.
2. Layer initialization now unwinds failed string/child storage and publishes to
   its parent only after success. Initialization guards make failed/repeated teardown
   safe and reject initialization of an active layer. Full-name construction checks
   its 64-node stack limit and failed appends. Failed registry growth frees the new
   unregistered layer; disposed registry queries are guarded.
3. Windows native lifecycle tests pass: 64-node hierarchy accepted, next depth
   rejected without parent mutation, failed/repeated teardown and sequence reinit
   work, and pending-authoring unload/reload regressions pass. Current native build,
   full converter suite, exporter bridge, DirectX paint deletion/rollback and new
   paint creation/rejection probes pass. These are functional checks, not measured
   allocator leak counts or every allocation-failure permutation. Evidence:
   artifacts/layer-lifecycle-native-tests.log, paint-create-final-build.log,
   paint-create-converter-suite.log, paint-create-bridge.log,
   paint-create-deletion-regression/ and paint-create-renderer-run2/.
4. Commits 50efa422 and 97ee60b0 were pulled/pushed together; fresh full validation
   run 39708054202 was cancelled after review found a spawn-cache lifetime defect
   exposed by actually deleting owned layers. The correction passes local checks; no
   full-platform acceptance is claimed. Queue audit
   found Apple jobs awaiting runner assignment, no pending approvals/deployments,
   and an older macos-14 job actively running. This supports allocation backlog;
   it does not establish the account concurrency limit or justify changing OS gates.
   Evidence: artifacts/apple-queue-*.json. Regression monitoring, Android readiness
   diagnosis and the next lifecycle audit run in parallel; no required gate is weakened.

### 6.56. Regression follow-up (2026-10-22)

1. Spawn lifecycle review found cached raw Layer pointers surviving manager Exit,
   unsafe negative/out-of-range queries and stale selection across reload without
   a default viewpoint. The ownership correction makes their lifetime especially
   important. Manager Exit now clears cached pointers/selection; Enter resets the
   selected ordinal even without a new default. Invalid/unloaded document queries
   return empty defaults and invalid selections preserve the valid selection.
   Native build, unload/reload/invalid-index tests, viewpoint policy tests and all
   five DirectX paint creation cases pass locally. This does not change the borrowed
   lifetime of returned names or establish concurrent query/teardown safety.
   Evidence: artifacts/spawn-cache-final-build.log, spawn-cache-authoring-tests.log,
   spawn-cache-policy-tests.log and spawn-cache-paint-regression/.
2. Android readiness review found unsynchronized external reads of working render
   counters while GlobalRender resets triangles and display rendering updates the
   other fields. External Unity/Godot performance queries now read a completed
   numeric snapshot by value under a short dedicated mutex. Publication follows
   complete display counters; no-document rendering clears it and disabling clears
   and rejects late publication. No string allocation or exported layout changes
   are involved. The legacy reference getter remains render-thread-only.
3. Windows Unity/viewer/Godot native builds, concurrent snapshot publication/reset/
   disable tests in the required authoring harness, exporter bridge and five DirectX
   paint-creation cases pass. Evidence: artifacts/perf-snapshot-build.log,
   perf-snapshot-native-tests.log, perf-snapshot-bridge.log and
   perf-snapshot-paint-regression/. The snapshot is player-wide, from the latest
   completed display submission; it does not certify a specific document, GPU
   completion or presentation. The race is concrete, but its contribution to the
   Android readiness failure remains unproven until replacement device validation.
4. Focused commits 56521cae (spawn-cache lifetime) and 6272e87a (render-counter
   snapshot) are pushed after a clean pull. Full replacement validation run
   39709116389 targets 6272e87a and is queued; the cancelled 39708054202 does not
   validate these fixes. Windows export/binding verification also passes locally
   (artifacts/perf-snapshot-exports.*). Android readiness and stale macOS exports
   remain unresolved until successful replacement runtime/package gates.
5. Superseded own runs 39704338933 and 39701694123 were rechecked: each had eleven
   successful completed jobs, five queued Apple jobs and no running work. Their queued
   remainders were cancelled to avoid competing with replacement 39709116389;
   completed artifacts/evidence remain available. No unknown runs were changed.

### 6.57. Asset-free spawn cleanup prerequisite (2026-10-22)

1. Lifecycle review found LayerSpawnArea's no-preview path could call piImage::Free
   with uninitialized channel/ownership fields. Its screenshot member is now value
   initialized before Init/Deinit. A poisoned-storage no-preview regression was added
   to the existing required native harness. Source-matched native build and the
   poisoned-storage cleanup test pass (artifacts/spawn-lifecycle-build.log and
   spawn-lifecycle-native-tests.log). This does not assert every preview decode or
   allocator failure path is covered.

### 6.58. Spawn lifecycle implementation (2026-10-22)

1. Native creation accepts SPAWN_AREA=2 in the existing LayerCreate ABI, and static
   subtree deletion now accepts Spawn descendants alongside Group/Paint layers.
   New spawns are visible/identity/opacity 1, non-timeline, Floor tracking, a sphere
   at the origin with radius .01, no translation axes and no preview asset.
   Prospective depth-first cache lists prepare before hierarchy mutation, preserving
   surviving active/default layer identity across ordinal changes. Publication must
   clear deleted references and invalidate stale ordinal snapshots without resetting
   playback/chapter state. No implicit first-spawn default/active selection is added.
2. Native code builds and CPU integration coverage passes for create,
   reparent, active/default deletion, subtree cleanup, cache preparation/publication
   rejection and queued/CPU-prepared unload. Existing DirectX paint creation and
   deletion regressions pass. Review found piTArray reserve could discard its live
   buffer after a real realloc failure; the correction and forced allocator
   rejection regression now pass (6.59). Local checks also cover an empty cache,
   first creation without implicit selection, deletion of an earlier ordinal while
   preserving the surviving active identity, and stale ordinal-snapshot rejection.
   No new spawn renderer/backend acceptance is claimed.
3. The next paint-timeline gap is whole-table frame replacement: current frame edits
   cannot grow the initial one-frame paint. Managed append/insert/move/remove/resize
   require atomic table publication, handle/reference checks, paused frame reselection
   and a safe zero-frame path. Keyless paint playback, repeat/Loop/Offset controls,
   presented-revision save and managed cutover remain outstanding; frame growth alone
   would not close timeline authoring.
4. Final source-matched build, native lifecycle/reserve and viewpoint policy tests,
   full converter suite, bridge and Windows bindings pass. Eight DirectX paint
   creation/deletion cases pass against the rebuilt binaries. Evidence:
   artifacts/spawn-reserve-final-build.log, spawn-reserve-native-tests.log,
   spawn-reserve-policy.log, spawn-reserve-converter.log, spawn-reserve-bridge.log,
   spawn-reserve-exports.* and spawn-reserve-paint-{create,delete}/. Native exports
   remain 147 player/51 reader, with 121 declared player bindings checked. Broader
   backend lifetime, GPU/in-flight cancellation and full M2 acceptance remain open.
5. Commits 83c173c1 (preview initialization), dc611ffc (reserve ownership/zeroing)
   and df3c5a14 (spawn lifecycle) were pulled/pushed together. Fresh full validation
   run 39711536998 targets df3c5a14 and is queued. Counter run 39709116389 now has
   a running Godot iOS validation job and four Apple jobs queued; it was not cancelled.
   Neither run yet establishes complete platform/package or Android runtime acceptance.

### 6.59. Failure-atomic reserve helpers (2026-10-22)

1. Typed and untyped SetMaxLengthNoShrink now retain the original live buffer until
   realloc succeeds and reject byte-count overflow. Untyped reserve also rejects
   zero-sized elements and starts zeroing after all live bytes, correcting an offset
   that could overwrite existing values when element size exceeded one byte.
2. The existing required native harness now compiles an isolated test translation
   unit against the actual helpers with allocator rejection forced inside realloc.
   It verifies preserved pointer/length/capacity/live values after failure, overflow
   rejection before allocation, successful subsequent growth and zeroed unused
   storage. No production allocator substitution is introduced.
3. Source-matched Windows Unity/viewer/reader/Godot builds and the combined native
   lifecycle/reserve tests pass. Initial local build failed in Microsoft's PDB API
   (C1090/RPC); a serialized retry passed, followed by the final build. No source
   compiler failure remains. Evidence: artifacts/spawn-reserve-final-build.log,
   spawn-lifecycle-final-build-retry.log and spawn-reserve-native-tests.log.

### 6.60. Atomic frame-sequence editing: Windows milestone (2026-10-22)

1. Added FrameSequenceSetHandles: an owned table of up to 1,000,000 stable
   drawing handles from one paint layer. Repetition represents holds; zero frames
   explicitly selects no drawing. Supported batches are table-only,
   drawing-create/geometry/optional-table and table/drawing-delete with references
   checked against the resulting table. Managed live mapping and frame IDs remain open.
2. Detached preparation and nonallocating table publication preserve handles and
   geometry resources. Empty tables are safe in both paint renderer paths. Playback
   intent and held local time survive zero/one-frame states; growth immediately
   reselects the held frame. Native-created paint receives the visibility key that
   offline import synthesizes. Explicitly keyless paint is unchanged. No new live
   repeat/Loop/Offset controls are claimed.
3. All eleven DirectX cases pass, including pure tables, create/delete bundles,
   injected CPU/GPU/presentation failures and queued/prepared unload. New paint
   grows at held frame one without another seek, then renders A/B/A/B evaluated
   seeks and both drawings under normal Playing clock advancement without further
   seeks. Bounds, stable handles, siblings, draw calls and real triangle counts
   agree. Evidence: artifacts/frame-sequence-ci-run3/ and its summary log.
   Table-only avoids geometry replacement paths; no GPU-upload timing claim.
4. Source-matched Windows Unity/viewer/reader/Godot builds, native table/real-layer/
   ownership tests, converter round trips, exporter bridge and stronger Windows
   export verification pass. Existing five creation, three deletion and viewpoint
   policy checks pass against the final binaries. Evidence: artifacts/
   frame-sequence-final-build.log, frame-sequence-autoplay-build.log,
   frame-sequence-reviewed-native-tests.log, frame-sequence-final-converter.log,
   frame-sequence-bridge.log, frame-sequence-exports.*,
   frame-sequence-create-regression.log, frame-sequence-delete-regression.log and
   frame-sequence-viewpoint-regression.log. Review found and removed a double Init
   in the new test fixture; the native suite passes afterward.
5. Export verification now reads native authoring declarations as well as managed
   bindings. Its negative check found the new API absent from the old binary;
   rebuilt Windows has 148 actual player exports and 147 unique required exports
   (121 managed bindings plus 26 native authoring declarations). Reader checks pass.
   Full replacement CI/platform acceptance is pending; M1/M2/M4 remain open.

### 6.61. Background import/drawing ownership guard: local validation (2026-10-22)

1. The importer captures sorted drawing indices before starting its worker, which
   subsequently indexes drawing vectors and fills geometry. Live create/delete/
   geometry edits could invalidate that ownership. Those commands now return Busy
   (-7), rechecked at queueing, CPU preparation and GPU publication. Table-only,
   metadata and independent new-layer construction remain available.
2. The native test drives the actual active-worker counter with valid payloads,
   checks queue/CPU rejection and unchanged drawing/table state, and verifies
   table-only/hierarchy commands remain allowed. It removes its CPU-only fixture
   before GPU unload. This is deterministic counter coverage, not concurrent-decode
   stress. Native tests pass in artifacts/frame-sequence-reviewed-native-tests.log.
3. Viewer resource probes wait for ownership release before queueing bundles and
   report exact errors. An earlier exploratory generic-handle failure did not log
   its return code, so its cause is not retrospectively proven. Final eleven-case
   sequence and existing create/delete probes pass with the Busy protocol.
4. Generic leaf traversal now returns zero children without reading uninitialized
   group storage, which asserted in Debug. A direct piTArray include fixes standalone
   compilation. Source review found no further actionable importer/transaction
   regressions; this is not platform, save or managed-cutover acceptance.
5. CI checkpoint: 39711536998 has twelve passed and four Apple jobs queued.
   39709116389 has twelve passed, two Apple native builds queued, and both Apple
   visual jobs running. Neither has failures; neither is a completed full pass.
   The earlier Android runtime readiness failure remains unresolved. Next after
   this milestone: move the existing required Android validation chain earlier,
   using the same-run native Android artifact, while retaining every existing gate.

### 6.62. Earlier required Android runtime validation in progress (2026-10-22)

1. Hosted Android player/Firebase/strict Vulkan aggregation currently waits for
   the complete build workflow, including queued Apple builds and full UPM
   packaging. The Android player only consumes libImmUnityPlugin.so from that
   package. The same-run Android native artifact already contains that binary.
2. Extracted the three hosted Android jobs to ci-engine-android.yml and call it
   from build.yml immediately after build-android. The original Engine opt-in
   expression and evidence scope are passed explicitly; secrets propagate at both
   nested calls and permissions do not increase. Quick/build-only calls do not
   acquire Firebase work. Quest hardware remains in the existing Engine workflow.
3. Existing matrix variants, strict marker/capture/metric checks, classifiers,
   failure propagation and artifact names remain required. Late duplicate hosted
   Android jobs are removed. An Android failure fails the nested Build result and
   blocks publish/release; no retry policy or acceptance threshold is weakened.
4. A failed Build can skip late Engine aggregation, so top-level validation also
   downloads the original UnityAndroidVulkan artifact directly and writes its
   report even after a failed download. Missing evidence still fails required
   hosted scope; direct collection preserves diagnostics rather than accepting it.
5. Workflow syntax validation with actionlint 1.7.12 passes with the existing
   custom self-hosted labels registered locally. Composition, face-orientation,
   cache-boundary, sample-entrypoint, report and matrix-evidence checks pass.
   Workflow wiring verifies 53 jobs; negative mutations reject the wrong early
   dependency, a cross-run native artifact, caller opt-in drift and removal of the
   device-success gate. Semantic comparison confirms the three extracted jobs
   retain their steps/settings except the native artifact reference, and the
   other fifteen engine jobs are unchanged except the removed collector dependency.
   Evidence: artifacts/android-ci-actionlint.log, android-ci-*-contracts.log,
   android-ci-job-equivalence.log and early-android-workflow-verifier.log.
   Pushed in 765f9a05. Run 39716790347 now proves early scheduling: Android native
   and Windows builds passed, and non-XR/URP player jobs 119622318226/119622318250
   started while four Apple jobs remained queued. The URP player build has now
   passed, followed by non-XR. The non-XR Firebase job appeared green but its actual
   result rejects missing composition success; section 6.65 records the failure.
   Neither early scheduling nor job conclusions close the Android failure.

### 6.63. Canonical property publication: Windows measurements (2026-10-22)

1. Added repeated visibility/opacity/transform publication measurements against
   the 2 ms target. The geometry benchmark's overlapping
   active strokes can hide property changes, and its Small case has no visible
   unrelated paint layer. Its captures cannot establish property correctness.
2. Added a separately identified property-live-v1 workload with separated red target
   and immutable blue siblings, full CPU drawing residency and held frame zero. Retain
   10/400/4,000 drawings and 16/32/64 points per drawing, recording new fixture hashes
   and layout metadata; do not reuse geometry baseline comparisons or claim speedup.
3. Untimed setup removes the importer's synthesized target visibility key, then
   verifies no keys/overrides mask the canonical changes. Each property is measured
   independently. Final control/change captures prove target disappearance, lower
   opacity pixel energy or translated red centroid, with blue sibling pixels
   unchanged. Per-sample effective values, counts, handles/bounds and sampled
   renderer tokens/geometry revisions remain coherent. No all-GPU-upload accounting
   or all-backend acceptance is claimed.
4. Five warmups and thirty measured edits per property/size pass on Windows DirectX
   / RTX 4090, with separate control captures. Native Commit-to-Presented median/p95
   milliseconds are:

   | Size | Visibility | Opacity | Transform |
   |---|---|---|---|
   | Small | 0.0658 / 0.1266 | 0.0566 / 0.1028 | 0.0496 / 0.1155 |
   | Medium | 0.0533 / 0.1439 | 0.0589 / 0.1289 | 0.0461 / 0.1196 |
   | Large | 0.0445 / 0.1299 | 0.0429 / 0.1182 | 0.0433 / 0.1206 |

5. These are stored native publication timestamps, excluding the later two-frame
   correctness wait, prior ABI/marshalling costs, displayed-frame latency, GPU
   completion and retirement. Queue/call samples and binary/fixture/settings/GPU
   provenance are retained separately. No optimization or speedup is claimed;
   broader M1 acceptance remains open. Maximum publication across all 270 measured
   edits is 0.1758 ms.
6. Evidence: artifacts/property-benchmark-final/ retains report.json and each
   case/property/control-or-change log and capture. Build/fixture evidence is in
   property-benchmark-final-build.log and property-benchmark-files-v1/.
   Parser tests reject incomplete metadata, missing/mismatched renderer evidence,
   incorrect effects, sibling pixel changes and invalid timing. The eleven sequence
   cases and existing geometry benchmark smoke pass. An initial stale pilot
   baseline with different warmup metadata was correctly rejected; the fresh
   matched geometry baseline/run passes in property-benchmark-geometry-regression2/.
7. Windows CI now requires the Small three-property/control correctness smoke and
   uploads reports/captures. It does not enforce a workstation latency threshold
   on hosted runners. Workflow wiring and syntax checks pass. Pushed in 91eda9bb;
   replacement run 39718923123 passed the required Windows property step, but was
   cancelled with 39716790347 after that run exposed the shared Android runtime
   failure (6.65). Production mutation behavior and APIs are unchanged; full hosted
   acceptance remains pending.

### 6.64. Uploaded replacement cancellation: Windows milestone (2026-10-22)

1. Added a nonempty static drawing replacement probe on Windows DirectX/D3D11.
   Existing queued/CPU-prepared cases and empty-paint creation do not establish
   cancellation after geometry resource upload.
2. Added an opt-in hold after successful GPU preparation and Prepared revision
   assignment, before model mutation/presentation. Record that GPU preparation
   happened once: repeated Upload is not idempotent and could overwrite handles.
   Normal publication and public ABI semantics must remain unchanged.
3. Three owned cases pass: control, held capture, and unload/reload recovery. The
   old RGB image matches its control exactly across the hold; three consecutive
   held states retain the original geometry revision and renderer token while the
   new nonempty token is Prepared, not Presented. Exactly one cancellation occurs
   with no old-session swap. Session queries fail before reuse, reload restores the
   original geometry, and a healthy visible replacement moves only the red target.
   Blue sibling pixels and unchanged drawing bounds/handles remain stable.
4. This establishes cleanup of uploaded but unpublished resources. That token
   was never drawn, so it cannot establish retirement of in-flight replacement
   work, GPU completion/fence behavior, exact resource leak counts or other-backend
   acceptance. Existing D3D12 submitted-work lifetime checks remain separate.
5. Windows Unity/viewer/Godot builds, native failure/policy tests, strict exports,
   eleven sequence cases and Small property/control rendering checks pass. Evidence:
   artifacts/gpu-prepared-final-build.log, gpu-prepared-viewer-payload-bounds-build.log,
   gpu-prepared-unload-run4/, gpu-prepared-final-{native,policy}-tests.log,
   gpu-prepared-final-exports.*, gpu-prepared-{sequence,property}-regression/ and
   gpu-prepared-parser-tests.log. Parser negative checks reject empty payloads,
   duplicate/missing held frames, stale renderer traces, publication before cancel,
   missing/double cancellation and missing invalidation/recovery evidence.
6. Work was preserved during the priority Android fix (6.65). Exploratory recovery
   checks then exposed test assumptions: fully resident drawing data can still be
   import-owned, and a new raw geometry payload has different quantization from
   imported bounds. The probe now honors QueueFull, logs exact failures, and checks
   the new payload's bounds without loosening the original 1e-6 tolerance. The first
   generic queue failure did not log its return code, so its cause is not proven.
7. Windows CI now requires the three-case smoke and uploads reports/captures.
   Workflow wiring/syntax checks pass; replacement CI for this milestone is pending.
   The preceding seek-fix run now has an actual accepted non-XR producer (6.65);
   full Android acceptance still requires URP and the strict collector.

Hosted checkpoint: f50ec11d is pushed; full validation run 39723167991 includes
the new required Windows cancellation test. No hosted pass is claimed yet.

### 6.65. Android freeze failure and truthful producer status (2026-10-22)

1. Non-XR job 119627721787 in 39716790347 reports success, but artifact 12400033781
   contains passed=false: the required composition-success marker is missing.
   Its real log reports 2,530 cyan rear-probe pixels (share 0.003, limit 0.0005).
   Both render-only and composition captures lack IMM content; the latter contains
   only Unity probes. Readiness reports 37 paint calls/642,730 triangles and six
   captures exist. This is not device/runtime acceptance.
2. APK and same-run native artifact 12399692179 have identical .so SHA256
   8b1bcd8cc44737ab5cc78dcc92b3c9157829c9974665080571b8abf2ebae1893.
   The missing composition RT warning is intentional: the validation getter
   returns null because final composition occurs in OnRenderImage. Neither stale
   payload nor that warning explains the failure.
3. Actual Vulkan draws fall to zero immediately after the smoke freezes playback.
   Legacy SetTime resets keyed layers but evaluated immediately only when already
   paused. The smoke seeks while playing, then queues Pause before GlobalWork;
   Pause suppresses the next evaluation and leaves the reset layers hidden.
   A new playing-seek/immediate-pause CPU regression fails before the correction.
4. Both static and pretessellated renderers also retained stale paint counters:
   DisplayRender returned for zero visible layers before resetting draw/triangle
   counts. Move those resets ahead of the return. Coherent snapshot publication
   alone cannot make stale renderer data current.
5. Keep artifact collection despite a failed smoke, then make each Firebase
   producer explicitly fail on missing/rejected result JSON. Its existing
   continue-on-error capture step must not leave the job green. The strict Vulkan
   collector and all marker/capture/pixel thresholds remain required and unchanged.
   The new verdict rejects this actual failed artifact and a missing-file control,
   and accepts an explicit passed=true control.
6. Confirmed source-failure runs 39716790347 and 39718923123 were cancelled;
   successful independent evidence is retained. No infrastructure retry is used.
   Evidence: artifacts/ci-agent-39716790347-nonxr-*, android-composition-failure.png,
   triage-unity-android-vulkan-render.png, seek-immediate-pause-before.log and
   android-producer-verdict-controls/. The producer verdict is committed in
   0d02624b; replacement live acceptance remains pending.
7. Legacy SetTime now validates loaded/entered/root state and immediately evaluates
   the requested pose whether playing or paused. An already paused document stays
   paused. The regression's keyed-viewpoint visibility assertion fails before the
   fix and passes afterward; chapter boundaries, early stops and prior policy cases
   pass too. This is native CPU ordering evidence, not Android pixel acceptance.
8. The staged runtime fix builds Windows Unity/viewer/Godot successfully. Exporter
   bridge, strict exports, eleven sequence cases, Small three-property/control
   captures and the existing geometry benchmark smoke pass against that build.
   Evidence: artifacts/seek-pause-staged-native-build.log,
   seek-immediate-pause-after.log, seek-fix-{sequence,property,geometry}-regression/,
   seek-fix-exports.* and seek-fix-exporter-bridge.log. Unfinished GPU diagnostic
   source was temporarily backed up, excluded from this build and restored byte
   for byte; seek-staged-build-provenance.json records both hashes and restoration.
   Matching Windows plugin output is included with the staged seek/counter fix.
9. Pushed counter correction 0ff06a7a and seek fix 1d31040d. Fresh full validation
   run 39721181625 is running. Full Android acceptance still requires its strict
   collector, not only successful native/player builds or producer job conclusions.
10. Actual non-XR artifact 12401164243 reports passed=true, errors=[], six captures
   and gcloud exit zero (not ignored). Its APK .so matches same-run native artifact
   12401595945: SHA256 88c116c46e2ebbd52f689088000a084a4ab2ac7027c210caa9dc5975934a0a57.
   Readiness reports 39 paint calls/723,866 triangles; scene composition passes and
   rear occlusion has 271 matched pixels, below the unchanged limit. Render-only
   and composition captures contain actual IMM paint. Evidence:
   artifacts/ci-agent-39721181625-nonxr-* and android-seek-fix-accepted.png.
   The earlier non-XR missing-paint failure is superseded by this producer evidence;
   URP artifact 12401892293 also reports passed=true/errors=[] and gcloud exit
   zero (not ignored); required content/depth/allocation checks and rendered-frame
   validity pass. Its APK native payload matches the same native artifact/hash.
   Evidence: artifacts/ci-agent-39721181625-urp-*.
   Strict artifact 12401878020 independently passes status/manifest, capture and
   composition metrics, managed/native eye structures, stereo log contract, physical
   device video and face orientation. Evidence:
   artifacts/ci-agent-39721181625-strict-summary.json. The earlier Android failure
   is closed for this hosted scope; full multiplatform acceptance remains pending.

### 6.66. D3D11 structured-buffer ownership regression (2026-10-22)

1. Resource-lifetime audit found that CreateStructuredBuffer owns both an
   ID3D11Buffer and an ID3D11ShaderResourceView, while DestroyBuffer released only
   the buffer. The leaked view retained the buffer after cancellation, retirement
   or unload. Cancellation logs alone therefore did not prove resource release.
2. The direct WARP test first rejected the existing structured-buffer descriptor:
   DEFAULT/IMMUTABLE usage incorrectly requested CPU-write access. With that
   prerequisite corrected, the old DestroyBuffer reproduced a leaked view reference
   count of one. The final fix releases the view before the backing buffer and
   cleans wrapper/buffer ownership on failed creation. Failure unwind is reviewed,
   not allocation-fault tested.
3. Eight real WARP cases alternate Static/IMMUTABLE and Dynamic/DEFAULT. After
   unbinding, destroying the renderer buffer and releasing observer references,
   both actual COM view and resource reference counts reach zero. Ordinary buffers
   with no view also destroy successfully. No drawing commands are submitted, so
   this is ownership-release evidence, not in-flight GPU completion or all-backend
   leak accounting. The runner compiles the current production renderer directly
   and retains source/executable hashes and per-iteration results; stale pass
   reports are cleared before build/run.
4. Evidence: artifacts/d3d11-ownership-before.log (original invalid descriptor),
   d3d11-ownership-leak-before-fix.log (descriptor-only correction/old destroy),
   and d3d11-buffer-ownership/{build.log,run.log,report.json} (fixed).
   Windows Unity/viewer/Godot native build, the three real GPU-prepared
   cancellation/recovery cases, all Small property/control rendered-effect cases,
   strict Windows exports and evidence-parser guards pass. Evidence:
   artifacts/d3d11-ownership-native-build.log, d3d11-ownership-gpu-regression/,
   d3d11-ownership-property-regression/ and d3d11-ownership-exports.*.
   Windows CI now requires the ownership check and uploads its evidence; workflow
   matrix and actionlint checks pass. Full replacement CI remains pending.
5. Runs 39721181625 and 39723167991 are terminal completed/cancelled so
   the source correction can receive a fresh full validation run. The saved strict
   Android pass for 1d31040d remains scoped evidence, not full multiplatform or
   D3D11 resource-lifetime acceptance. Pushed fix b4a2c595 starts fresh full run
   39724027590; no hosted pass or full M1 claim is made.

### 6.67. Paint Loop/Offset evaluated-state correction (2026-10-22)

1. Playback audit found that paint Loop keys overwrote the stored repeat count.
   Both static and pretessellated paint now keep canonical and evaluated repeat
   counts separately. Frame selection uses evaluated state; metadata getters retain
   the authored count. Before the first Loop key or after key removal, evaluation
   restores the canonical value. The old-source regression failed on metadata
   corruption; final cases cover canonical infinite/finite counts and both Loop
   values, backward seeks and removal through actual Layer evaluation.
2. Offset now resolves its active tick value before visibility computes local
   time, with zero before the first key or when absent. This fixes first-evaluation
   lag and stale offsets after removal/backward seek. It is distinct from the
   paint-only SetOffset helper, whose argument is frames.
3. Shared Offset evaluation required sound-state checks. Cold start, backward seek
   and removed keys use the expected offsets. A decreasing midspan Offset under a
   looping ancestor reproduced a false sound retrigger. Wrap detection now compares
   visibility-span clock origins excluding content offset, resetting its sample on
   seek/restart. Final tests preserve the sound at the changed key and following
   tick while a genuine ancestor wrap still retriggers. Existing finite-window,
   root and pause/resume predicates are retained. No audio-engine or audible
   playback acceptance is claimed.
4. Final Windows Unity/viewer/Godot/reader build, native authoring/frame-table/sound
   tests, chapter/viewpoint policy, all eleven visible sequence cases, full converter
   suite and strict Windows exports pass. A prior reader/library link collision
   during parallel validation was resolved by the sequential build; final builds
   and tests run after all production edits. Native-suite run.log and verdict/hash
   report are now uploaded by the existing required Windows gate; stale pass reports
   are cleared before execution. Workflow matrix/actionlint checks pass.
5. Evidence: artifacts/playback-keys-old-source.log,
   playback-key-sound-before-fix.log, playback-key-sound-refined-before.log,
   playback-key-final-native-build.log, playback-key-final-native-tests.log,
   authoring-failure/{run.log,report.json}, playback-key-final-policy-tests.log,
   playback-key-final-sequence/, playback-key-final-converter/ and
   playback-key-final-exports.*.
6. Run 39724027590 is terminal completed/cancelled after source regressions were
   established. Fix 86fd4645 was pushed; its run 39724961901 was subsequently
   cancelled after the additional regressions in 6.68.
   No new live ABI or managed cutover is
   claimed. Group/root Loop keys also mutated canonical timeline repeat state;
   the local correction and explicit native clock rule are recorded in 6.68.
   Quill root Loop parity remains unverified. This milestone does not close M1/M2 or the plan.

### 6.68. Group/root Loop canonical state and control clock (2026-10-22)

1. Old-source actual-Layer tests reproduce canonical repeat corruption on root and
   nested groups: authored count 2 becomes 0 under a Loop key and remains 0 before
   the first key or after removal. A root probe at the same tick also differs
   between cold and sequential evaluation (opacity 0.9 versus 0.1).
2. Canonical/effective timeline repeats are now separate. Metadata getters retain
   authored counts; runtime wrapping and sound ancestor guards use evaluated state.
   Root Loop controls use unwrapped elapsed root time; other root properties and
   children use the resulting wrapped/clamped time. Nested Loop controls use parent
   timeline time. This explicit native rule removes self-referential root lookup;
   it does not change Action::Loop chapter handling.
3. Actual-Layer tests pass for cold/sequential root evaluation, controls beyond
   content duration, zero duration, backward seeks, key removal and canonical
   infinite/one/two-repeat boundaries. Keyed nested controls affect descendant
   opacity at parent-clock boundaries and root wraps, and restore finite repeat 2
   on removal. A held Stop/Continue case preserves nested clocks and checks exact
   elapsed ticks 149/150 around a later control key. The initial test expected tick
   150 one tick early; diagnostics confirmed Stop75 holds74, and the final test
   asserts both sides without changing production timing.
4. Windows Unity/viewer/Godot/reader build, native regression suite, chapter/viewpoint
   policy, full converter suite, strict Windows exports and all eleven visible
   sequence cases pass. Evaluated comparison passes 69 mode samples with zero pair
   and history errors; ordinary sound-repeat scheduling remains correct for root
   infinite/single/finite and nested finite cases. This is CPU scheduling evidence,
   not audible playback or full platform acceptance.
5. All 21 local Quill projects have zero Loop animation keys. They cannot establish
   root Loop-key timing. Quill parity for the explicit native rule remains
   unverified; another matched source/export case was requested. This requirement
   remains open independently of the native correctness evidence.
6. Evidence: artifacts/timeline-repeat-before.log,
   timeline-repeat-old-native-output.log, timeline-repeat-native-build.log,
   timeline-repeat-held-first.log, timeline-repeat-final-native-tests.log,
   authoring-failure/{run.log,report.json}, timeline-repeat-policy-tests.log,
   timeline-repeat-converter/, timeline-repeat-evaluated-compare.log,
   timeline-repeat-sequence-regression/ and timeline-repeat-exports.*.
7. Run 39724961901 is terminal completed/cancelled. Corrected source 3b8589b1 is
   pushed; its full run 39725916624 was subsequently cancelled for 6.69. No hosted pass, M1/M2
   or full-roadmap completion is claimed.

### 6.69. Android missing-override compiler regression (2026-10-22)

1. Audit of superseded run 39724961901 found a real Android native source failure,
   not an infrastructure problem: Clang rejected eighteen methods in
   LayerPaintPretessellated with -Werror,-Winconsistent-missing-override. Adding
   the evaluated-repeat override exposed the previously unannotated overrides.
   Windows compilation had not detected this Android compiler regression.
2. All eighteen existing overridden methods now explicitly declare override.
   No warning/error gate or runtime behavior is changed. NDK 26.1.10909125,
   aarch64 Android API26 reproduces all eighteen errors on the old header, while
   the corrected header passes. Both affected production translation units pass;
   all 22 CMake-listed importer translation units then compile with -Werror and
   archive successfully. This is a local importer build, not a complete Android
   player link, hosted device pass or full CI acceptance.
3. Evidence: artifacts/retry-audit-android.log,
   android-override-check/{before,after,layer,layerPaintPretessellated}.log and
   android-override-check/importer/report.json (per-source/header hashes and flags).
   Helper run 39725799794 succeeded as an eligibility-guard no-op; it performed no
   retry and supplies no validation evidence. A repeated failure is not being
   dismissed as infrastructure or hidden by a helper's green conclusion.
4. Run 39725916624 is terminal completed/cancelled. Annotation fix a0f71bd4 is
   pushed; full validation run 39726457067 now passes Android native compilation
   including importer. Required Windows native authoring/frame-table/property
   rendering checks also pass. Full CI/device acceptance remains pending.
   The group/root playback checks in 6.68 remain valid
   for their declared Windows/CPU scope; Android/platform acceptance stays open.

### 6.70. Live static-paint Loop/Offset keys (local milestone complete)

Layout follow-up: the versioned key layouts recorded at this checkpoint were
removed in 6.100. The current single 80-byte key directly carries Offset ticks;
the historical sizes/check counts below describe this earlier source only.

1. Extend the existing atomic key edit path to static-paint Loop/Offset insertion,
   replacement and removal. Both channels are stepped; other layer implementations
   remain unsupported for these live channels. This is the key-editing portion of
   M2; canonical paint-repeat property editing remains a separate open item.
2. Preserve the 80-byte version 1 key layout and reserved-zero contract. Loop uses
   its boolean payload. Offset requires an additive 96-byte version 2 key carrying
   an exact uint32 tick value, with explicit size/version/reserved validation; no
   float or reserved-field reinterpretation. No new export is required.
3. Publication reselects at the current held parent clock without another
   seek, changing playback intent, resetting root actions or retriggering unrelated
   sounds. The targeted static-paint evaluator also updates latent selection while
   hidden, so unhide cannot expose the previous key's frame. Ordinary hidden-layer
   playback policy is unchanged; inactive/keyless spans retain local-zero selection.
4. Final Windows Unity/viewer/Godot/reader rebuild and matching Windows package
   binaries pass. CPU regressions cover held add/replace/remove, hidden edit then
   unhide, keyless/inactive selection and preserved root action/playback state.
   All six real viewer cases pass: normal, CPU rejection, GPU-failure flag (no GPU
   preparation stage), presentation rejection, queued unload and prepared unload.
   They check rendered A/B selection, stable handles/mappings/selected renderer
   tokens and sibling state, rollback, session invalidation and reload.
5. ABI checks pass 16 layout/rejection cases. V1 remains 80 bytes; V2 is 96 bytes.
   Successful V2 Offset delivery through the exported C ABI is source-reviewed,
   not exercised on a loaded document by these checks; viewer edits use the native
   player API. This is not complete upload accounting or audible sound acceptance.
6. Converter, whole-frame sequence, chapter/viewpoint, strict Windows exports,
   53-job workflow matrix and changed build-workflow lint checks pass. Android
   NDK 26.1 strict arm64/Android26 compilation passes for changed importer/player
   translation units; no new Android runtime acceptance is claimed. Repo-wide
   actionlint additionally reports five custom runner-label diagnostics and one
   constant-false condition in unchanged ci-gpu/ci-ios workflows; these are not
   failures in the changed build workflow and remain outside this milestone.
7. Evidence: artifacts/paint-playback-milestone-build.log,
   artifacts/authoring-failure/report.json and run.log,
   artifacts/paint-playback-milestone-viewer/report.json,
   artifacts/authoring-key-abi/report.json,
   artifacts/paint-playback-milestone-ndk/report.json and the matching
   paint-playback-milestone converter/sequence/policy/exports/workflow/lint logs.
   CI requires the ABI and six viewer cases and uploads WindowsPaintPlaybackKeys.
   Full hosted regression run 39935868460 accepts source 97142c9a (6.77), including this milestone; operation-specific/platform breadth remains bounded above.
   Canonical paint-repeat property editing, full M2, managed cutover and broader
   platform acceptance remain open.
8. Implementation committed as 76c46f1d. The pre-push pull merged upstream
   CI binary-refresh 2316ebc9, retaining this milestone's rebuilt/validated Windows
   player and reader DLLs in the two conflicting package paths. Other upstream
   replacement binaries are retained. No source conflict or stash was required.
   Post-merge export verification passes all four platforms under the existing
   platform policy. Strict all-platform mode flags only the already documented
   iOS registration function supplied by ImmUnityPluginRegister.mm outside the
   static library; strict Windows verification passes. Evidence:
   artifacts/paint-playback-milestone-merged-exports.json.

### 6.71. Full hosted CI acceptance restored (2026-10-22)

1. The user identified the acceptance gap: individual successful jobs and cancelled
   pipelines did not deliver a completed full validation run. Feature work was
   frozen without committing/pushing its source while full CI took priority.
2. Run 39726457067 on a0f71bd4 attempt 1 had 44 successful jobs, one cancelled and
   a failed evidence collector. GitHub's annotation proves standalone iOS exceeded
   its 30-minute limit during Simulator boot. Compilation passed but application
   runtime validation never started. Actual Godot iOS artifact 12404142261 passes;
   its evidence was missing only because the preceding standalone download failure
   prevented the later Godot download step.
3. One targeted infrastructure retry of standalone iOS and dependents preserved
   successful independent jobs. Attempt 2 completed SUCCESS: 49 jobs passed,
   22 skipped, no failures. Boot, Metal visual/lifecycle, cadence, comparison and
   final evidence collection all passed. No additional retry, weaker gate, reduced
   workflow or feature push superseded this run.
4. Actual CIValidationEvidence artifact 12409062591 identifies source a0f71bd4 and
   passes all 20 supported hosted rows against that revision's exact matrix file.
   Required scope is hosted; there are zero invalid or missing required rows.
   Hardware-only unity/windows/non-vr/vulkan remains evidence_incomplete and is
   explicitly NOT accepted by this hosted pass. Uncommitted live-key work is also
   outside the tested source revision.
5. CI macOS build and Package Unity Plugins export verification pass for all four
   platforms: 147 unique required player exports (121 managed + 26 native-authoring)
   and 51 reader bindings. The existing iOS registration wrapper is provided by
   ImmUnityPluginRegister.mm, rather than the static library. This closes stale
   CI-package export evidence for a0f71bd4; the skipped binary-sync step did not
   update this checkout's tracked non-Windows binaries.
6. Final Android provenance records source a0f71bd4/run 39726457067, passing
   classification and producer/native payload hashes. Native SHA256:
   0acfe87911d566056ad6ac2d783d98923809cb887e891baa1373a00b374ad639.
   Evidence: artifacts/full-ci-39726457067-acceptance-audit.json,
   full-ci-39726457067-android-provenance.json,
   full-ci-package-export-verification.log, full-ci-retry-progress2.json and the
   downloaded final CIValidationEvidence artifact. No full-roadmap completion is
   claimed; the remaining M1-M5/converter requirements retain their own exit gates.

### 6.72. Live-key full CI: Android dependency timeout (2026-10-22)

1. Run 39748355080 at b5c43743 attempt 1 completed with 20 successful jobs,
   four failed and nine skipped. Windows, including the required static paint
   playback-key/versioned ABI step and authoring cancellation regressions, passed.
   This does not establish complete milestone/platform acceptance.
2. Unity Android URP job 119725145631 and non-XR job 119725145658 each attempted
   to download 94.1 MB for FFmpeg. Their `timeout 120 apt-get install --yes ffmpeg`
   commands exited 124 during package downloads. Device runtime steps did not run;
   missing rendered-frame/Firebase result errors are downstream consequences,
   not evidence of an observed rendering regression. The Unity Android Vulkan
   aggregate 119726432452 and final Validation Evidence 119736487908 also failed.
3. Confirmed run_attempt=1, then requested exactly one `gh run rerun --failed`
   for this isolated dependency-install infrastructure failure. Request succeeded;
   successful independent jobs are preserved. No source change, timeout increase
   or gate weakening was made. Retry acceptance remains pending. If the same
   failure repeats, it remains unresolved; do not rerun indefinitely.
4. Evidence: artifacts/live-key-ci-failure-run.json,
   artifacts/live-key-ci-failure-attempt.json,
   artifacts/live-key-ci-urp-job.log and artifacts/live-key-ci-nonxr-job.log.
   [Validation run](https://github.com/icosa-mirror/IMM/actions/runs/39748355080).
5. Attempt 2 completed with failure, confirmed by the run API in
   artifacts/live-key-ci-retry-final.json. The retry allowance was exhausted;
   this is not accepted evidence. The user then explicitly instructed a new FULL
   run using [CI VALIDATION]. A fresh marker commit starts the complete hosted
   build/engine/device/evidence pipeline, rather than another failed-job rerun.
   No source change or gate reduction accompanies this restart. The new run's
   result remains pending; do not infer it passes from successful earlier jobs.

### 6.73. Correct repeated FFmpeg setup timeout before full restart (2026-10-22)

1. Attempt 2 URP job 119826156048 again failed at FFmpeg package installation:
   94.1 MB requested, exit 124 at the 120-second bound, before device runtime.
   Non-XR passed on this attempt. Subsequent Unity Android Vulkan/evidence
   failures follow the missing URP producer. Evidence:
   artifacts/live-key-ci-attempt2-diagnosis.json and live-key-ci-attempt2-urp.log.
2. The agent incorrectly interpreted the user's full-run instruction as a blind
   restart and pushed 8c06298c before diagnosing attempt 2. Run 39795297170 is
   superseded; cancellation was requested after the user clarified the intent.
   That run is not acceptance evidence. Its cancellation request has not yet
   reached a terminal state in the API snapshot; do not call it confirmed cancelled.
3. Correct both FFmpeg setup sites in ci-engine-android.yml. Reuse an existing
   executable; otherwise retain the 120-second package-index bound, install
   without recommended packages and allow a bounded 600-second package install.
   Give the setup step 15 minutes to contain both commands and version checking.
   Increase outer infrastructure job budgets to 55 minutes for the producer and
   30 minutes for the collector so setup cannot consume the runtime/evidence
   allowance. The existing device step remains bounded at 30 minutes.
   Install failure remains fatal. Device runtime timeouts, visual acceptance,
   producer gates and required evidence are unchanged. This changes dependency
   setup bounds in response to a measured repeated failure, not runtime gates.
4. Composition contract, 53-job workflow matrix and changed-workflow actionlint
   checks pass. Six extracted-shell cases pass for existing-tool reuse, successful
   install and fatal install failure at both sites. Package commands were mocked:
   local checks do not establish a real Ubuntu mirror download or device pass.
   Evidence: artifacts/ffmpeg-install-fix-contracts.log,
   ffmpeg-install-fix-matrix.log, ffmpeg-install-fix-actionlint.log and
   artifacts/ffmpeg-install-fix-shell/report.json. Full hosted validation of this
   corrected source remains pending; start it with [CI VALIDATION] after commit.

### 6.74. Put FFmpeg only at its consumer; replace apt provisioning (2026-10-23)

1. Full run 39798342624 on bb97268a failed: 21 successful jobs, three failed and
   nine skipped. URP requested 62.8 MB for FFmpeg and timed out after 600 seconds
   during Ubuntu mirror downloads, before device execution. Non-XR device testing
   and Windows (including live Loop/Offset checks) passed. Android aggregate and
   final evidence failures are downstream; this is not full milestone acceptance.
   Actual CIValidationEvidence artifact 12435797546 was inspected. Evidence:
   artifacts/ffmpeg-fix-full-run-results.json, ffmpeg-fix-full-run-urp.log,
   ffmpeg-fix-full-run-evidence.log and artifacts/ffmpeg-fix-full-evidence/.
2. Trace confirmed the Firebase producer script does not use FFmpeg. Remove its
   unnecessary install step, allowing both URP/non-XR tests to run independently
   of video-analysis provisioning. Restore the producer job's 40-minute budget.
   The consumer is the later collector's validate_render_video.py invocation.
3. Replace collector apt-get with imageio-ffmpeg 0.6.0's bundled binary wheel.
   Pin official Linux x86_64 and Windows x86_64 wheel SHA256 values in
   tests/tools/requirements-video.txt; require binary-only, hash-verified,
   dependency-free pip installation. Setup Python 3.12 with pip caching.
   prepare_video_ffmpeg.py rejects unpinned/system fallbacks, checks the bundled
   executable, exposes it as ffmpeg through GITHUB_PATH and records its hash/version.
   Setup is bounded at five minutes; restore the collector's 15-minute job budget.
   [Upstream bundled-wheel documentation](https://github.com/imageio/imageio-ffmpeg).
4. A required real-codec smoke precedes unchanged device-video validation:
   synthesize H.264 clips and require the existing CLI to accept matching frames
   and reject blank/corrupt videos. Reports, captures and logs remain under the
   existing always-uploaded Android evidence directory. Device-test step limits,
   visual contracts, producer acceptance and final evidence gates are unchanged.
5. Both official wheels downloaded and their hashes/binary architectures were
   verified. Actual Windows wheel install/provision and all three real-codec
   cases pass locally; Linux binary execution still requires hosted CI. Workflow
   lint, 53-job matrix, composition and stable-video contracts pass. A regression
   guard forbids FFmpeg in producer jobs and apt-get in this Android workflow.
   Independent review confirms consumer placement and unchanged acceptance gates.
   Evidence: artifacts/video-ffmpeg-wheel-verification.json,
   video-ffmpeg-local-provision.json, video-ffmpeg-real-tests/report.json and
   video-ffmpeg contracts/matrix/actionlint/validator-contract logs.
6. Committed/pushed as 97142c9a with [CI VALIDATION]; fresh full hosted run
   39935868460 is in progress. Windows passed and both Android Firebase jobs
   have started at the latest check. Do not treat local codec tests as device
   rendering evidence or call full validation passing before the required final
   artifact is accepted.
7. Completed Android evidence is now independently verified in primary JSON/logs.
   Artifact 12495240655 has successful pinned Linux wheel provisioning, actual
   FFmpeg 7.0.2-static and matching/blank/corrupt codec cases. Both physical-device
   producer results pass with empty errors and gcloud exit 0; real device-video
   validation found consecutive matching frames and strict classification passes.
   Both APK native payloads match the same-run native binary SHA256
   684e67b4f09a5eaaf19be89902dccd80444a89637f8739cb62a5bf66976f2e95.
   Manifest identifies source 97142c9a and attempt 1. Evidence:
   artifacts/ffmpeg-linux-proof-39935868460-summary.json,
   ffmpeg-linux-proof-39935868460-provenance.json and the preserved collector
   artifact/logs. This closes the FFmpeg setup/device-path failure for that source;
   remaining full-run jobs/final required rows still need acceptance.

### 6.75. Standalone converter library extraction (2026-10-23)

1. Add code/tools/ImmConversion as the .NET 8 Windows-host library. Move the
   existing shared source list and UnityTypes.cs compatibility definitions out
   of the QuillToImm test executable. The graph, reader, converter, compiler and
   native declarations are linked directly from the original Unity packages;
   their conversion source and Unity compilation are unchanged. Simulated editor
   menu code stays in the test executable, outside the library.
2. QuillToImm.csproj now references the library, so the required existing CI
   converter regression command exercises the actual extracted assembly. Only
   that regression assembly receives internal access for existing native/codec
   checks; standalone callers use the public conversion entry points.
3. Independent read-only review confirms the shared source list, compatibility
   definitions and Unity/test boundaries are preserved. Build succeeds with zero
   warnings/errors. The complete real-native converter
   self-test passes through the project reference. A separate executable with no
   internal-access grant converts an animated source through the public library.
   Rebuilding the original committed harness source list in ignored artifacts
   provides an independent pre-extraction output: decoded animated paint comparison
   passes against the new public consumer's output. The self-test's animated.imm
   file is intentionally overwritten by its rejection/output-preservation check;
   it is not a valid reference output for this comparison.
4. Evidence: artifacts/conversion-library-build.log,
   conversion-library-regressions.log, conversion-library-consumer.log,
   conversion-library-original-baseline.log and conversion-library-consumer-compare.log.
   Temporary baseline/consumer projects and dependency references are ignored
   local evidence, not distributed or committed project setup.
5. This is a packaging prerequisite, not a finished application or complete
   Unity-compatible SDK. The standalone assembly uses compatibility types and
   must not be imported alongside the Unity package; Unity builds the shared
   package source against real engine types. Production CLI/desktop entry points,
   bundled native dependencies, portable publish, clean-machine validation and
   real Unity consumer integration remain open under C5. Full run 39935868460
   validates 97142c9a, not this later library extraction. Keep it running while
   progressing independent work; do not supersede it just for this local milestone.

### 6.76. Production converter CLI and atomic output publication (2026-10-23)

1. Add code/tools/ImmConvert, a .NET 8 Windows x64 command-line executable using
   only the extracted library's public full-project conversion entry point. It
   accepts project/output paths, explicit --allow-omissions and --overwrite,
   optional JSON report, input-size limit and development native-directory option.
   Native lookup defaults to the application directory, independent of working
   directory; Unity and Python are not conversion runtime dependencies.
2. The first real staged conversion exposed six missing compression/media DLLs
   that the repository's sibling-package preload had supplied. Stage/check the
   complete 11-DLL native set before conversion, producing an actionable missing-
   dependency error rather than a later native serialization exception.
3. Reports use imm-quill-conversion-report-v1 with tool version, timestamps,
   paths, exit code, published/partial flags, published byte count and issues.
   Invalid arguments return 2; rejected conversion 1; environment/publication/
   report failures 3; cooperative cancellation 130. Complete or explicitly
   allowed partial publication returns 0. Console diagnostics/progress go to
   stderr. Partial output is labelled explicitly in both human and JSON results.
4. Review found that an initial existence check alone could overwrite a file
   created by another process during conversion. Convert into a unique private
   file in the destination directory, then publish with File.Move's overwrite
   argument. No-overwrite is enforced by the final filesystem operation. Cleanup
   removes abandoned staging files. Report/source/output collisions are rejected;
   reports are written through a separate atomic temporary file. Report writing
   and IMM publication are not one multi-file transaction; a late report failure
   can return 3 while preserving the successfully published IMM.
5. Release build and 15 real CLI cases pass, including application-directory
   native lookup, explicit native directory, unrelated working directory, Unicode/
   spaced paths, missing dependencies, existing-output refusal/overwrite, argument
   errors, source/report collision rejection, malformed-input preservation and
   explicit unsupported-model omissions. Three real-filesystem publication checks
   exercise late-created destination preservation, explicit overwrite and fresh
   publication using the production helper. Independent review confirms the race
   correction, reporting/cleanup and CI failure propagation.
6. CLI native output matches the harness's decoded full-project paint/animation
   metadata in the synthetic case; no new visual/audio fidelity is claimed.
   Add required Windows CI build/publication/CLI checks and always-uploaded
   WindowsImmConvertCLI evidence. Workflow matrix and changed-workflow lint pass.
   Evidence: artifacts/imm-convert-build-release.log,
   imm-convert-cli-tests-final/report.json, imm-convert-publication/publication-report.json,
   imm-convert-cli-compare.log and imm-convert workflow-matrix/actionlint logs.
7. C5 remains incomplete: portable self-contained publish, native/codec and runtime
   dependency bundling, licensing/clean-machine acceptance, desktop UI and real
   Unity editor consumer integration are outstanding. These changes remain outside
   running CI source 97142c9a; keep its full validation alive while working and
   include these packaging milestones in the next full validation push.

### 6.77. Full hosted acceptance for live keys and FFmpeg correction (2026-10-23)

1. Run 39935868460 at 97142c9a1885d0d344fa0317b3f686d296cedaf8 completed
   SUCCESS, with 49 successful jobs, 22 skipped and no failures. No retry was
   required. The native live-key implementation and corrected Android dependency
   placement/provisioning are included in that validated source.
2. Actual CIValidationEvidence artifact 12496091571 was downloaded and audited
   against that exact source's matrix. All 20 required supported hosted rows pass;
   there are zero missing/invalid required rows. All 43 preserved manifests have
   the matching source SHA. Final producer/native-payload/FFmpeg/video proofs are
   retained under 6.74, not inferred from green orchestration alone.
3. Hardware-only unity/windows/non-vr/vulkan has hosted_gate=null and remains
   evidence_incomplete; deferred headset/hardware gates are not accepted by this
   hosted pass. Passing regression rows do not establish every live operation's
   platform breadth, successful loaded-document V2 C ABI delivery, audio acceptance,
   complete Quill fidelity or completion of the whole roadmap.
4. Evidence: artifacts/full-ci-39935868460-acceptance-audit.json and preserved
   final/producer artifacts. Later C5 library/CLI/package sources are outside
   this accepted revision and need their own new full-validation run.

### 6.78. Self-contained Windows CLI archive prototype (2026-10-23)

1. Add package-windows.ps1 to publish a self-contained, untrimmed win-x64 CLI,
   stage the complete native package DLL set and Visual Studio's app-local
   redistributable CRT files, preserve available notices and create a ZIP.
   Existing output directories are rejected; no old artifacts or user files are
   deleted. CLI/library/native file hashes, source revision and dirty-tree state
   are recorded. Do not present a dirty-tree prototype as an exact clean revision.
2. Native import inspection found VCRUNTIME140.dll required by the codec DLLs
   and Windows Media Foundation required by Audio360. The prototype includes the
   redistributable CRT files rather than depending on a global Visual Studio/VC
   runtime install; normal Windows Media Foundation availability remains a stated
   platform requirement. No Windows system DLLs are copied into the bundle.
3. Actual local archive build passes: 232 recorded files, 39,221,930-byte ZIP in
   artifacts/imm-convert-portable-run1. Extracted tests verify every file/hash and
   manifest inventory, then run help and real conversion with absent DOTNET_ROOT,
   Windows-only PATH and an unrelated working directory. Actual loaded CoreCLR,
   vcruntime140.dll and ImmUnityPlugin.dll paths are inside the extracted Unicode/
   spaced folder. These are selected module-path proofs, not an audit of every
   loaded dependency. Reports include runtime details without failing successful
   conversion if optional module inspection is unavailable.
4. The self-contained .NET runtime is 8.0.30 for this local build. Actual extracted
   conversion and all 15 normal CLI regressions pass; workflow matrix/lint pass.
   Add required Windows packaging/extracted-runtime checks with always-uploaded
   WindowsImmConvertPortable ZIP, build/report and execution evidence. No public
   GitHub release is created. Local evidence: artifacts/imm-convert-portable-run1/
   package-report.json, imm-convert-portable-tests/report.json,
   imm-convert-cli-portable-regressions.log, dependency import report and the
   portable matrix/actionlint logs. Final source must be validated in fresh full CI.
5. Preserve repository/Core/SharpQuill, exact available codec, .NET/Newtonsoft and
   supplied Audio360/Oculus notices. Trimmed codec notices recovered from exact
   upstream version tags are retained with their URLs. Audio360's third-party
   notices are not its own acquisition grant; recovery/review of original
   Audio360 1.7.12 and Oculus 32 acquisition terms remains open before external
   public release. Manifest explicitly records licensing review incomplete.
   Existing permissive code licenses do not establish third-party SDK eligibility.
6. This is a portable CLI prototype, not completion of C5's folder-selection
   desktop application. Clean Windows VM acceptance, public-dependency licence documentation,
   desktop UI, GUI cancellation/progress behavior and real Unity editor consumer
   integration remain open. Isolated local CLR/CRT proof is stronger than running
   dotnet from the checkout, but does not substitute for a clean-machine test.
7. Packaging implementation committed as 443ce76c. Pre-push pull merged CI
   binary refresh a821203c without conflicts in e3f23f60. Repeated full converter
   regressions, strict Windows exports, 15 CLI cases and extracted-archive tests
   all pass with the refreshed DLLs. The new clean-tree archive records e3f23f60,
   232 files and 39,221,454 bytes; SHA256:
   9cf7152439c626b23ed504821b087160e184bdd857ac1fca6c3fad03622a6a0b.
   Evidence: artifacts/imm-convert-portable-post-sync/package-report.json,
   imm-convert-portable-post-sync-tests/report.json, imm-convert-cli-post-sync-tests/
   report.json and post-sync converter/exports logs. Preserve that source commit
   in history; a final [CI VALIDATION] evidence commit triggers the new full run.

### 6.79. Windows archive trace decoding regression (2026-10-23)

1. Full run 39942955962 at fe5d91ed failed Windows job 120378619910 during the
   archive verifier, after package creation. Actual WindowsImmConvertPortable
   artifact 12496797547 confirms conversion succeeded, output was published with
   no omissions, and CoreCLR/vcruntime140/ImmUnityPlugin loaded from the archive.
   The failed assertion was the additional host-trace path check, not conversion
   or the typed JSON module-location checks.
2. The 400,929-byte native Windows host trace uses Windows ANSI bytes. Its Å byte
   0xC5 is valid in the runner's ACP but invalid standalone UTF-8. Python UTF-8
   mode changed read_text's implicit decoding and replaced that byte, so the
   exact Unicode-path comparison failed. Local default-mode testing had missed
   the environment difference. Independent artifact-byte review confirms cause.
3. Read the native trace explicitly with strict encoding='mbcs' and force
   python -X utf8 in the required CI invocation. Keep the exact Unicode path,
   all 232 manifest/hash checks, actual local-module checks and real conversion
   acceptance unchanged. No gate relaxation, timeout change or native fix.
4. Reproduce the old failure locally with forced UTF-8 mode on the same real ZIP;
   the corrected verifier passes in that mode. Workflow matrix and lint pass.
   Evidence: artifacts/portable-trace-utf8-red.log,
   portable-trace-utf8-green/report.json, portable-trace matrix/actionlint logs,
   imm-convert-portable-windows-job.log and portable-ci-failure-evidence/.
5. Requested force cancellation of the source-failed superseded run before
   restarting full validation. Commit/push this isolated correction with
   [CI VALIDATION]. Desktop shared-host WIP remains uncommitted and is excluded
   from this validation source; no stash or source discard is required.

### 6.80. Shared standalone application host (local milestone; e7f5c100)

1. Move standalone native configuration, cancellation, atomic IMM publication,
   reports and runtime diagnostics from the CLI into StandaloneConversion in the
   .NET-only ImmConversion project. Both CLI and desktop call the same host service.
   Original Unity package converter source is unchanged. Native configuration is
   fixed once per process; attempts to change its directory require restart. A
   per-process guard rejects overlapping native conversion work.
2. The CLI keeps its argument policy, full-project default, exit codes and JSON
   schema. Publication tests now target the shared helper. Builds pass without
   warnings; all 15 real CLI cases and three publication checks pass. Six real
   host cases pass: pre-cancel, first conversion, re-entrant rejection, repeat,
   cancellation during conversion preserving existing output, and recovery.
   Staging cleanup is checked. Independent review found no concrete blocker.
3. Evidence: artifacts/desktop-shared-host-build.log,
   desktop-shared-host-cli-tests/report.json and desktop-host-tests/host-report.json.
   Native/operation/UI scope remains bounded by those checks. This is not managed
   live scene-graph cutover or Unity consumer integration.

### 6.81. Windows desktop converter (local milestone; CI acceptance pending)

1. Add QuillToImm.exe with project/output pickers, drag/drop project selection,
   strict-by-default omission/overwrite options, input limit, report option,
   progress, cancellation and readable results. An immutable request is captured
   before background work. Inputs are disabled during conversion, closing a busy
   window requests cancellation and waits for the worker, and 100 ms coalesced
   progress prevents one UI callback per compiled stroke.
2. Eight real WinForms event-loop/native cases pass: conversion, existing-output
   refusal, repeated use, strict unsupported-model rejection, explicit labelled
   partial output, controlled pre-native cancellation, recovery and cooperative
   close. The shared-host tests separately cover cancellation during conversion.
   Minimum-size layout overlap was measured and fixed through shared table layout
   and a usable minimum height; result/action regions cannot overlap. Normal and
   minimum-size rendered forms were inspected. No live external input is claimed.
3. Combined portable archive includes the desktop/CLI entry points and matching
   .NET Core/WindowsDesktop 8.0.30 runtimes. The actual 77,110,679-byte prototype
   contains 515 verified production files. Extracted CLI checks and the desktop
   probe pass with global .NET discovery disabled. Only four test-entry files are
   added to the extracted test copy; production UI, host, WinForms, CLR and native
   files stay from the ZIP and their hashes remain unchanged after testing. Form,
   host, Forms and CLR locations are verified inside that extracted folder.
4. Live input testing is unverified: Computer Use's list_apps cannot connect to
   its native pipe (os error 2), including retry/reset/reinitialize recovery. No
   target window or external UI input was available. The renderer/component tests
   do not substitute for mouse/keyboard, all DPI/monitor, clean-VM or real Unity
   editor-consumer acceptance. Dependency licence documentation remains a separate
   unfinished check, not a known prohibition or a proposed new product licence.
5. Evidence: artifacts/desktop-form-tests-usable-minimum/report.json and PNGs,
   desktop-portable-bundle/package-report.json, desktop-portable-tests/report.json,
   desktop-probe-publish.log and desktop workflow-matrix/actionlint logs. Required
   Windows form/shared-host checks and an extracted desktop probe are wired into
   CI with always-uploaded reports/renders. Full CI source 47a0240d excludes this
   later work; keep that run alive while progressing and validate these milestones
   in the next full push. C5 remains open until its remaining acceptance is proved. No new product licence or fee is proposed; dependency documentation is simply unverified where noted.

### 6.82. Unity editor converter and refreshed hosted acceptance

1. Add **IMM > Convert Quill Project...** with source/output pickers, strict
   defaults, explicit omission/overwrite choices, input limit, cancellable progress,
   readable conversion issues and selection of the imported asset. An editor-only
   assembly calls the existing package converter/compiler on the Editor thread;
   it does not import the standalone compatibility assembly or change mappings.
2. Publish through a private same-directory staging file. Refusal, cancellation or
   conversion failure preserves existing output. Successful replacement preserves
   its Unity asset GUID. Require an IMM inside Assets and reject hidden paths that
   Unity ignores. Play mode is blocked. Import failure is reported separately from
   successful file publication. Document the menu workflow in the package README.
3. All three targeted tests pass in the actual Unity 6000.6.0f1 Windows Editor:
   path guards; refusal/pre-cancellation preservation; and native project
   conversion, synchronous AssetDatabase import, repeat refusal and successful
   replacement with a stable GUID. Initial test compilation exposed a namespace
   ambiguity, corrected before acceptance. Evidence: ignored
   artifacts/unity-quill-editor-results.xml, unity-quill-editor.log and CLI log.
   This proves Editor compilation and import, not live menu/picker input or other
   Editor platforms. The package-import CI gate now requires these three tests;
   its Linux execution remains pending the next full run.
4. Full hosted run 39945112846 on 47a0240d completed SUCCESS (49 passed,
   22 skipped). Downloaded final evidence artifact 12501160640 proves all 20
   required hosted rows passed; all 43 manifests identify that source. Windows
   archive verification and both Firebase Unity producers passed. Audit:
   artifacts/full-ci-39945112846-acceptance-audit.json. Hardware-only Windows
   Vulkan remains outside hosted acceptance. Later shared-host, desktop and
   Editor sources still require their next full-validation run.
5. Workflow matrix verification and actionlint of the changed engine workflow
   pass locally. Unity regenerated packages-lock.json to include the existing
   declared Newtonsoft dependency; this is synchronization of the real package
   manifest, not a temporary dependency reference. C5 remains open for the
   documented CI, live-input, clean-machine and dependency-notice acceptance.
6. Editor milestone committed as 8cab3bff alongside separate shared-host and
   desktop commits e7f5c100 and 3ec46758. Pulled and merged CI binary refresh
   724b538e before pushing. A new full `[CI VALIDATION]` checkpoint push includes
   that refreshed payload and all three implementation milestones. Its outcome
   is pending; local Editor proof is not full hosted acceptance of this source.

### 6.83. Checkpoint validation in progress; cross-host proof unfinished

1. Full run 39954000025 validates e19db96e (the shared-host, desktop and Editor
   milestones plus merged replacement binaries). It is still in progress, not
   accepted. Firefox browser verification was cancelled after its 15-minute job
   limit while Playwright's Ubuntu system-package downloads stalled. Its log
   contains repeated slow/retried Azure Ubuntu mirror fetches, not a browser test
   verdict. GitHub rejected a job-specific retry while the workflow was active;
   no successful retry or full-run pass is claimed. Retry only this infrastructure
   job and its dependents once when GitHub permits it, preserving independent jobs.
2. Additional local Editor/CLI cross-host proof is incomplete. A temporary owned
   Editor probe was retained under ignored artifacts and removed from Assets after
   its processes exited. Unity startup failed to allocate its 1 GiB UDS shared
   memory; the machine had roughly 400 MiB physical memory and 1.8 GiB virtual
   memory free. No Library deletion or interruption of other Editors was used.
   This does not invalidate the earlier passing Editor tests, but supplies no
   output for the intended matched native-decoded comparison. Do not relaunch
   repeatedly under memory pressure. CLI conversion with the complete refreshed
   native dependency folder passes; using just the player package folder correctly
   rejected missing reader/codec dependencies before conversion.
3. Evidence: artifacts/converter-checkpoint-ci-progress.json,
   converter-checkpoint-firefox.log, converter-checkpoint-firefox-retry.log,
   quill-editor-cli-cross-host-editor.log and quill-editor-cli-cross-host/cli-report.json.
   The cross-host comparison remains a required C5 check, alongside clean-machine,
   live-input and dependency documentation acceptance.
4. The next native M2 implementation remains canonical paint-repeat editing.
   Source review confirms live LayerSetProperties only exposes visibility,
   opacity and transform; Loop keys edit evaluated playback instead. Existing
   paint SetMaxRepeatCount updates canonical and effective counts, so publication
   must re-evaluate active Loop keys and held-time frame selection before claiming
   correctness. Keep the existing strict 64-byte property ABI intact and cover
   zero/finite counts, active-key precedence/removal, hidden paint, rollback and
   stable unrelated renderer state in the next implementation milestone.

### 6.84. Native canonical paint-repeat editing (local CPU milestone)

1. Add dedicated ImmAuthoring_PaintSetMaxRepeatCount and
   ImmAuthoring_PaintGetMaxRepeatCount exports for canonical static-paint repeat
   counts. Zero means indefinitely; finite and full uint32 counts are retained.
   Keep the existing 64-byte LayerProperties ABI and strict reserved fields
   unchanged. Other layer types/paint implementations are explicitly unsupported
   in this slice. The getter returns presented canonical metadata, not a Loop
   key's evaluated override.
2. Use the existing one-layer metadata batch path, rechecking type at preparation.
   No drawing/resource replacement or GPU upload is required. Publication updates
   the canonical repeat count, re-evaluates paint at the held parent clock, and
   increments evaluation revision. Active Loop keys take precedence; removing
   them restores the edited canonical count. Preserve playback/root actions,
   hidden-layer policy, sibling geometry and the existing presentation lifecycle.
3. Windows Unity/viewer/Godot/reader build passes. New CPU cases pass zero,
   finite and UINT32_MAX publication, unchanged state before publication,
   non-paint/second-edit rejection, discard, injected preparation/presentation
   rejection and stable drawing handles/table. Held-clock helper cases pass
   finite/infinite selection and active-key precedence/removal. Nineteen additive
   C ABI validation checks pass, including the new exports' early argument checks;
   loaded-document delivery through those C exports is still unverified.
4. Existing six rendered Loop/Offset publication, rejection and unload cases pass
   against the rebuilt native code. These are regression checks, not rendered
   acceptance of the new repeat setter. Strict Windows package exports pass.
   Full native converter regression checks also pass against the rebuilt plugin.
   Evidence: artifacts/paint-repeat-native-build.log, paint-repeat-cpu-tests.log,
   authoring-failure/report.json and run.log, authoring-key-abi/report.json,
   paint-repeat-playback-regressions-staged/report.json and
   paint-repeat-windows-exports.json.
5. This milestone is outside active full CI source e19db96e. Required existing
   CPU/ABI jobs consume the added cases in the next full push. New rendered
   repeat-specific rollback/unload cases, loaded C ABI delivery, backend breadth,
   managed transaction mapping and full M2 acceptance remain open. Do not count
   this CPU slice as completing canonical-repeat integration across all hosts.

### 6.85. Matched real Editor and standalone output proof

1. After checking that no IMM Editor owned the project and available memory had
   recovered, run the owned cross-host probe in Unity 6000.6.0f1. Actual Editor
   conversion and synchronous AssetDatabase import pass. Convert the same
   synthetic animated source with the production CLI and complete native staging
   folder. Both use the same rebuilt exporter DLL hash, now committed in native
   milestone b0055332. The earlier UDS failure in 6.83 remains a recorded failed
   attempt, not the final outcome of this local check.
2. Editor and CLI IMM outputs are byte-identical. Independent native decoding
   exactly matches one paint layer, four strokes, twelve points, hierarchy,
   transforms, pivots, frame tables, root metadata, root/layer keys and nested
   timing. This fixture has zero viewpoints, pictures or sounds, so those empty
   comparisons provide no additional content coverage. No evaluated playback,
   navigation, media, clean-machine or live mouse/keyboard acceptance is claimed.
3. Retain input/output/native hashes and decoded comparison evidence under ignored
   artifacts/quill-editor-cli-cross-host/provenance.json and decoded-comparison.log;
   the source JSON/QBIN and both output IMM files remain available locally.
   Editor evidence is quill-editor-cli-cross-host-editor-refreshed.log and its CLI
   log. The owned temporary Editor source/output assets were removed after the
   process completed, with its probe source retained in artifacts. No changes to
   other open Editors or user assets were made.
4. Canonical-repeat CPU milestone is committed as b0055332, separate from this
   acceptance record. Active full run 39954000025 still targets e19db96e and does
   not validate that later native source. Its Windows converter/form/archive gates
   pass; latest snapshot has 18 successful jobs, three active and one cancelled
   Firefox infrastructure job. Full acceptance and the single isolated retry
   remain pending. C5 and M2 retain all remaining acceptance requirements above.

### 6.86. Canonical repeat rendered acceptance (local Windows milestone)

1. Extend the existing viewer playback-validation harness with an explicit
   canonical-repeat mode, retaining the Loop/Offset mode and its existing gates.
   Exercise six held-time count edits, canonical getter values, advancing playback,
   stable drawing/renderer identities and sibling state. Reuse the failure/unload
   matrix for CPU rejection, irrelevant GPU-failure flag, presentation rejection,
   queued unload and held-prepared unload followed by recovery.
2. Wire both modes into the required Windows paint-playback step and always-upload
   the additional repeat report, logs, settings and rendered captures. The native
   presentation hold switch now also covers repeat metadata for the prepared
   unload case. Windows Unity/viewer/Godot rebuild passes. Both modes pass all six
   real rendered cases against the rebuilt native code, including unload/recovery,
   rejection preserving canonical state, count getter values 0/1/3/UINT32_MAX,
   held-clock A/B transitions, backward seek and advancing playback. The irrelevant
   GPU-failure flag does not reject this metadata-only path.
3. Native Player getter/setter delivery on a loaded document is now exercised,
   with unchanged canonical state before publication and after rejection. The
   C-export positive delivery requirement remains separate. Windows DirectX
   evidence does not establish other backends or complete upload accounting.
   Workflow matrix and actionlint of the changed build workflow pass locally.
4. Evidence: artifacts/paint-repeat-render-build.log,
   paint-repeat-rendered/report.json, paint-repeat-rendered-key-regressions/report.json,
   and their six runtime directories with actual captures, logs and hashes;
   paint-repeat-render-workflow-matrix.log and paint-repeat-render-actionlint.log.
   Earlier cross-host evidence retains its original b0055332 native payload and
   hashes; the rendered tests retain separate runtime copies of this later build.
   Full hosted acceptance of these later native milestones remains pending.

### 6.87. Positive native C-export repeat delivery (local Windows milestone)

1. Add an owned native integration host that compiles the production main.cpp
   export definitions and retains the production shared bridge source. Initialize
   that bridge with a borrowed Direct3D 11 WARP device and load a real IMM fixture;
   no Unity interfaces, cameras, private-member access or production test hooks
   are introduced. The initial host link failed because its generated project
   omitted the shared bridge implementation; fix the project source selection.
2. Positive attach/get/set/commit/status/discard/detach calls through the C export
   definitions pass on the loaded document. Count 0, finite 2 and UINT32_MAX reach
   presented revisions, while queued values remain unpublished and the sibling
   count remains unchanged. Unsupported root and unknown-layer calls reject;
   rejected and detached getters preserve the caller's output, and detached
   mutation rejects. These checks complement dynamic-DLL symbol/early-validation
   checks and the independent rendered viewer cases; they do not establish
   Unity/PInvoke marshalling, hardware rendering or another platform.
3. Wire the host into the required Windows playback step and always-upload its
   failed-or-passed report, build/run/native logs. The runner hashes the actual
   executable, fixture and fixture exporter. All final local cases, workflow
   matrix verification and actionlint of the changed build workflow pass.
   Evidence: artifacts/paint-repeat-cabi-final/report.json, build.log, run.log and
   native-runtime.log; paint-repeat-cabi-workflow-matrix.log and actionlint log.
4. This milestone remains outside active full run 39954000025/e19db96e. Latest
   snapshot: 34 successful jobs, six active, ten skipped, one cancelled Firefox
   infrastructure job. Both Unity Firebase producers and Windows converter/form/
   archive validation pass. Linux package import/new Editor tests remain active,
   so no full acceptance or successful Firefox retry is claimed.

### 6.88. Correct native Editor CI platform; preserve required coverage

1. Source inspection found a CI configuration error in 6.82: the new positive
   native Editor conversion/import test was unconditional in the Linux package-
   import harness, but the Unity packages ship no Linux native Editor plugin.
   The shipped Windows plugin explicitly excludes Linux in its importer metadata.
   The actual completed package-import job 120436042600 confirms this was its
   sole failed test, at native export with an ImmUnityPlugin load failure. Its
   complete XML is preserved in the job log: four of five EditMode cases passed;
   38 of 41 PlayMode cases passed and the other three were ignored. This was a
   test-platform error, not evidence that Linux native import passed.
2. Restrict only the native test to the supported Windows/macOS Editors. Keep the
   two platform-independent output/cancellation tests required in Linux package
   import. Require all three tests in the existing macOS native Editor after its
   same-source native-plugin staging and player/Editor build. Use the installed
   Editor directly, preserve the no-quit test-runner lifecycle, require the named
   results even if an earlier step fails, and always retain its XML/logs in the
   existing macOS artifact directory. The workflow validator now requires these
   native test steps, an execution timeout and the always-running result check.
3. Windows player-build jobs also use Linux Editors to cross-compile players;
   they cannot establish Windows Editor native conversion. Existing actual local
   Windows Editor tests and matched Editor/CLI proof remain valid. The macOS
   Editor execution is new hosted coverage, pending the corrected full run.
   Workflow-matrix, changed engine-workflow actionlint and whitespace checks pass.
4. Requested cancellation of source e19db96e run 39954000025 before pushing the
   source correction. No full acceptance is claimed for that superseded run.
   Windows converter/form/archive and both Unity Firebase producers passed before
   cancellation; Firefox had timed out during Ubuntu dependency downloads. The
   corrected full push keeps that gate and includes b0055332/4c43d0d7 repeat editing
   and rendered validation plus a360235c positive native C-export delivery. Do not
   treat local repeat acceptance as a full hosted pass of those later sources.
5. Evidence: artifacts/converter-checkpoint-source-correction-cancel.log,
   editor-native-platform-workflow-matrix.log, editor-native-platform-actionlint.log
   and editor-native-linux-failed-job.log, editor-native-linux-failure-summary.json
   plus the run snapshot.
   The owned 961 MB full-artifact download was stopped after the complete job log
   supplied the authoritative XML and sole failure; no other process was stopped.
   This correction does not add Linux Unity runtime support
   or remove native conversion/import from required hosted coverage.

### 6.89. Corrected full validation launched

1. Source correction committed/pushed as cab50dfa with `[CI VALIDATION]` at the
   start of its message. Pull before push was already up to date. Full run
   39963637687 targets cab50dfa841f3f1aa590ac1863ddce0c1058e9a0; its launch
   snapshot was queued, and acceptance is pending.
   It includes all committed desktop/Editor conversion, canonical-repeat native
   publication, rendered regression and positive linked C-export milestones.
2. Superseded run 39954000025 is terminal cancelled: 39 successful jobs, three
   failed, six cancelled and fourteen skipped. The sole failed producer was the
   confirmed Linux native Editor test in 6.88. Engine Evidence Report and
   Validation Evidence failed by reporting that failed preflight and cancelled/
   missing producer results. Their complete logs show no additional producer
   failure. Firefox's earlier dependency-install timeout remains recorded; no gate
   was weakened or successful isolated retry claimed. The new full run exercises
   that unchanged gate again with the source correction.
3. Evidence: artifacts/editor-native-platform-new-full-runs.json,
   editor-native-platform-push.log, converter-checkpoint-ci-progress.json,
   editor-platform-engine-evidence-failed.log and
   editor-platform-validation-evidence-failed.log. The accepted full hosted
   baseline remains 39945112846/47a0240d until this newer source passes and its
   actual evidence is audited. Full M2/M3/M4/M5 and C2-C5 acceptance stay open.

### 6.90. Require passing results for named Unity test gates

1. Read-only regression review found that the shared XML verifier accepted three
   required cases marked Skipped when aggregate failures were zero. Reproduce
   that exact false acceptance, then require every matching named case to have
   Passed/Success results and no ignored/skipped or explicitly unexecuted state.
   Match exact names or their parameterized variants, not an unrelated name prefix.
2. Focused checks pass for skipped/inconclusive/unexecuted cases, similar prefixes,
   partially skipped parameter sets and legacy NUnit Success results. The stronger
   verifier also accepts actual CI PlayMode required cases and the actual local
   Windows native Editor result; it preserves their successful evidence while
   rejecting skipped required coverage. Native-runner tests also reject skipped
   XML and still return the licence.
3. Evidence: artifacts/editor-required-tests-skipped-red.json and green.json,
   editor-required-tests-verifier-tests.log and
   editor-required-verifier-real-evidence.json. This strengthens the existing
   gates; it is not a replacement for the new native macOS execution.

### 6.91. Native Editor licence lifecycle and reader provenance (local checks pass)

1. Review of official unity-builder v4 at 4423ee75828dff56037d1d1cfdac6b68e3a6ba00
   shows its Personal ULF is decoded to a serial, activated for the build and
   returned before the action completes. The direct test step added in cab50dfa
   therefore needs its own licence lifecycle. Requested cancellation of that
   superseded full run before a corrected push. This is a reviewed source issue;
   no native macOS test pass or licence failure observed on that run is claimed.
2. Add a native Editor runner using the same existing licence/credentials and the
   official activate/test/finally-return flow, a temporary activation project,
   bounded processes and strict required-case XML verification. Never echo argv
   or derived credentials; sanitize phase logs and XML before upload. Both test
   and licence-return success are required. Local fake-process checks pass for
   success, activation/test/return errors, timeout, missing/skipped tests, invalid
   ULF data and credential redaction; actual macOS execution remains pending.
3. Download/sign the same-run Internal-ImmStrokeReaderPlugin-macOS artifact too:
   the native test reads the produced IMM through the reader, so player-only
   staging is insufficient provenance. Require both reader-staging steps in the
   workflow contract. Budget fifteen minutes for the three bounded four-minute
   phases and cleanup margin, with a sixty-minute macOS job. Existing visual and
   native result gates stay required and unchanged.
4. Final read-only review approved the corrected lifecycle, secret handling,
   strict result gates, reader staging and timeout budget with no remaining
   concrete blocker. Runner/verifier checks, 53-job workflow contract, actionlint
   of both changed workflows and whitespace checks pass. The skipped-case gate
   fix is separately committed as 3cd8473f. Actual activation and all three native
   macOS Editor cases remain required in the corrected full run; local fake-process
   checks do not establish that hosted acceptance.
5. Local evidence: artifacts/editor-native-license-runner-tests.log,
   editor-native-final-verifier-tests.log, editor-native-final-runner-tests.log,
   editor-native-final-matrix.log, editor-native-final-actionlint.log and official
   source copies unity-builder-v4-parameters.ts, mac-entrypoint.sh, mac-activate.sh,
   mac-return_license.sh and setup-mac.ts. Push this independently committed
   lifecycle correction with `[CI VALIDATION]` and audit its real results before
   declaring any new full hosted acceptance.

### 6.92. Reviewed licence correction in full validation

1. Commit 2b7d3671 owns the native Editor test licence lifecycle and stages the
   matching reader. Final review approved it before pushing; pull was already
   up to date. Full `[CI VALIDATION]` run 39966625591 targets
   2b7d3671a76f58171a668f52114c19012ce9ade5. Latest observed snapshot is in progress:
   thirteen jobs successful, five active, no failed/cancelled jobs. Native macOS
   activation/conversion and full hosted acceptance are not yet proved.
2. Superseded cab50dfa run 39963637687 is terminal cancelled: eleven successful,
   twelve cancelled, five skipped and one failed Validation Evidence collector.
   The complete collector log reports missing/cancelled producer evidence, not
   an additional failed producer. Preserve those limits; no full pass is claimed.
3. Evidence: artifacts/editor-native-license-full-runs.json,
   editor-native-license-full-progress.json, editor-native-license-push.log,
   editor-native-license-superseded-run.json and
   editor-native-license-superseded-validation.log. Continue source work while
   the full run proceeds; the accepted baseline remains 47a0240d.

### 6.93. Safe layer enumeration before live rename (local Windows milestone)

1. Source/lifetime audit found GetLayerCount/GetLayerInfoByIndex unprotected
   during CPU/GPU hierarchy publication. Add the existing Player mutex to these
   const getters and validate document-table bounds/usage before dereferencing.
   Each returned LayerInfo owns its name/full-path buffers. Counts and records
   remain separate snapshots, not a revision-consistent complete enumeration.
   Readers must stop before synchronous shutdown/Deinit.
2. No internal Player call path or locked callback recursively calls these two
   getters. Windows Unity/viewer/Godot build and positive linked-export WARP host
   pass. An additional reader collected 33,478 valid owned records during 200
   group creation/deletion publications. Invalid IDs reject safely. The first
   test compile used the wrong destroy-function name; correct it to the existing
   LayerDestroy export before acceptance. All six rendered canonical-repeat
   cases and all six existing rendered Loop/Offset cases pass against the rebuilt
   native code; no deadlock or rendered regression was observed.
3. Live rename is not implemented by this prerequisite. Its next slice must
   prepare the short name and all descendant full paths, reject active importer
   ownership at queue/preparation/publication, and copy spawn labels into owned
   storage under the Player lock before wrappers encode them. Notify spawn label
   consumers without rebuilding stable ordinal caches. Nonroot scope is consistent
   with the existing normalized root-label contract; never infer worker completion
   merely from Loaded state or StopLoadingAsync.
4. Evidence: artifacts/layer-query-snapshot-build.log and
   layer-query-snapshot-cabi-corrected/report.json and run.log, plus
   layer-query-snapshot-repeat-rendered/report.json and
   layer-query-snapshot-key-rendered/report.json. This later local source
   is outside active full run 39966625591/2b7d3671. Threaded decoding, synchronous
   teardown concurrency and other backend/per-operation acceptance remain open.

### 6.94. Live nonroot layer names (local Windows milestone)

1. Add dedicated UTF-8 LayerSetName/LayerGetName C exports without changing the
   existing property ABI. Nonroot group, paint and spawn names are supported;
   root labels and the other live layer types remain explicitly unsupported.
   Input is copied at queue time, malformed/empty/NUL-containing or oversized
   UTF-8 rejects, and size-query readback includes NUL. Insufficient output
   capacity reports the required size and preserves the caller's buffer.
2. Reuse/generalize the existing prepared full-name path used by reparenting:
   prepare a short name and descendant full paths, then publish them together.
   Check importer ownership at queue, preparation and publication. Notify spawn
   label consumers/evaluation revision without rebuilding stable ordinal caches.
   Unity/Godot wrappers copy spawn data under the Player lock before encoding
   owned names; Unity retains per-thread UTF-8, and Godot's fixed ABI buffer
   truncates only at a code-point boundary. Existing borrowed native queries
   retain their restricted lifetime contract.
3. Windows Unity/viewer/Godot build passes. Loaded-document C-export checks pass
   paint/group/spawn Unicode, copied input, queued/prepared old state, descendant
   paths, stable drawing handles/selection ordinals, owned spawn labels across
   rename/deletion, discard and injected CPU/presentation rejection. CPU counter
   checks pass ownership rejection at all three boundaries plus recovery; these
   do not exercise a real concurrent decoder. Dynamic-DLL early ABI checks pass
   all twenty-eight cases, including nine new name validation cases.
4. Evidence: artifacts/layer-rename-native-build.log, layer-rename-cabi/report.json
   and run.log, layer-rename-cpu-tests.log and layer-rename-abi-tests.log. Source
   review found no remaining concrete blocker. All six rendered rename cases
   pass: publication, CPU rejection, irrelevant GPU-failure flag, presentation
   rejection, queued unload and prepared unload/recovery. All twelve existing
   repeat/key rendered regressions and full converter regressions pass as well.
   The initial name-only probe revealed its old checker excluded the backward-
   seek sample from A/B renderer-token matching; include that actual sample and
   its layer ID, preserving the same required token/geometry checks.
   No full acceptance, live managed rename UI, root/media rename, positive Godot
   runtime label query or other backend coverage is claimed. This later local
   source is outside superseded full CI source 2b7d3671. Additional evidence:
   layer-rename-rendered-corrected/report.json, layer-rename-repeat-regressions/
   report.json, layer-rename-key-regressions/report.json and
   layer-rename-converter-regressions.log. Required Windows CI runs all three
   existing playback-harness modes and retains their reports/captures. The local
   milestone leaves full hosted and broader backend acceptance open.

### 6.95. Consume the retained reader package in macOS CI

1. Full run 39966625591 failed macOS job 120480879733 before Editor tests:
   Internal-ImmStrokeReaderPlugin-macOS was absent. Build packaging explicitly
   deletes that intermediate after producing the retained aggregate package.
   Requested cancellation for the source correction; no hosted licence/Editor
   acceptance is inferred from the resulting missing XML and downstream failures.
2. Download the same-run retained ImmStrokeReaderPlugin-Unity package instead,
   and copy its exact com.immersive-foundation.imm-stroke-reader/Plugins/macOS/
   libImmStrokeReader.dylib before signing. Actual artifact 12507683874 was
   downloaded and inspected: its manifest reports passed source 2b7d3671 and the
   expected 1,984,776-byte reader exists. Workflow contract now rejects the deleted
   internal reference and requires the retained package/copy. No licence or test
   acceptance gate changes. Read-only review approved producer/consumer lifecycle
   and path correctness; workflow-matrix and changed engine-workflow lint pass.
3. Evidence: artifacts/native-editor-macos-failed-job.log,
   reader-package-source-fix-proof.json, reader-package-fix-matrix.log,
   reader-package-fix-actionlint.log and reader-package-source-fix-cancel.log.
   This CI correction is committed independently of local rename source. The
   next full push must still prove actual native macOS Editor execution.

### 6.96. Rename milestone and retained-package correction in full CI

1. Commit bfa2405d adds atomic UTF-8 live rename, owned spawn label snapshots and
   the existing six-case rendered harness mode. Commit 316ace94 independently
   fixes the macOS reader-package source. Both were pushed after an up-to-date
   pull. Full `[CI VALIDATION]` run 39976407088 targets
   bfa2405d010ac5aabf614621bb851164de24156c; its launch is in progress and no full
   acceptance is claimed. The accepted hosted baseline remains 47a0240d.
2. Rename functional/ownership/ABI, six rendered rename, twelve existing playback
   and full converter regressions pass locally. No additional rename test scope
   was added after completing that six-case mode. Full hosted and other backend
   acceptance remain separate from this local implementation milestone.
3. Superseded 2b7d3671 run 39966625591 is terminal cancelled: 35 successful jobs,
   four failed, twelve cancelled and eleven skipped. The failed producer was the
   macOS artifact download in 6.95; three evidence collectors reported that failure
   and cancelled/missing results. Android Godot's smoke was cancelled at 14:57:41
   UTC and its failed verification/classification steps ran afterward, so those
   incomplete results are not treated as a separate reproduced product regression.
4. Evidence: artifacts/layer-rename-full-runs.json, layer-rename-full-push.log,
   reader-package-source-fix-old-run.json and reader-package-fix-device/engine/
   validation-collector.log. Next source work follows the current execution order
   above while preserving all remaining roadmap requirements.

### 6.97. Live pivot editing (local Windows milestone)

1. Dedicated 48-byte versioned LayerSetPivot/LayerGetPivot C exports support
   nonroot group, paint and spawn layers, including None/X/Y/Z reflection.
   Existing property and key ABIs remain unchanged. Validate finite translation,
   unit quaternion, positive uniform scale and the reflection enum. Readback is
   synchronized and preserves caller output on failure.
2. Queue copies the pivot; existing preparation/publication applies it atomically,
   with importer ownership guards and CPU/presentation rejection checks. Resample
   only the target transform at the held timeline time, including hidden parents;
   do not refire actions, restart playback or alter canonical transforms/keys.
   Invalidate affected spawn pose snapshots without changing ordinal selection.
3. Windows Unity/viewer/Godot builds, loaded production C-export group/paint/spawn
   checks with all four flips, discard/rejection and stable handles/selection pass.
   One focused CPU check proves hidden-parent/held-root transform resampling.
   Existing CPU authoring, 28 ABI guard and twelve rendered repeat/Loop/Offset
   regression cases pass. Independent source review found no blocker.
4. Evidence: artifacts/layer-pivot-native-build.log, layer-pivot-cabi/report.json,
   authoring-failure/run.log, layer-pivot-repeat-regressions/report.json,
   layer-pivot-key-regressions/report.json and layer-pivot-workflow-check.log.
   Pivot-specific rendered/backend acceptance and managed integration remain open.
   Full run 39976407088 targets the earlier rename source, so it cannot accept
   this operation. This does not complete M2 or expand the rename test scope.

### 6.98. Same-layer metadata batches (local Windows milestone)

1. Successive property, name, pivot and canonical paint-repeat setters for
   the same layer share one atomic publication. Last supplied value wins per
   field. Invalid subsequent commands preserve earlier accepted commands. Build
   the replacement edit privately before moving it into the queue, so an allocation
   failure cannot partially overwrite previously accepted fields.
2. Retain the existing prepared name/pivot/repeat validation and publication path;
   cross-layer and geometry/key/hierarchy combinations remain unsupported.
3. Windows Unity/viewer/Godot builds, CPU authoring and loaded production C-export
   checks pass. The existing host proves combined name/opacity/pivot/repeat
   publication, last-value semantics, rejection of cross-layer/invalid commands,
   queued-state isolation and whole-edit discard. This changes the old same-layer
   second-repeat-setter rejection contract deliberately; its CPU check now requires
   successful accumulation. No new test framework or rename test expansion.
4. Evidence: artifacts/metadata-batch-native-build.log, metadata-batch-cabi/report.json,
   metadata-batch-cabi/run.log and metadata-batch-cpu-check.log. Broader mixed
   batches, managed transaction mapping and hosted/backend acceptance remain open.

### 6.99. Pivot and metadata batch full validation launched

1. Atomic commits d603a220 (pivot) and ae11ee17 (same-layer metadata) are pushed
   after an up-to-date pull. The tip starts with `[CI VALIDATION]`; full run
   39979516973 targets ae11ee17a70628ee97952c3c8a996582b4adb131 and is queued.
   Evidence: artifacts/metadata-batch-full-runs.json and metadata-batch-push.log.
2. Earlier rename run 39976407088 remains unresolved. Its latest job snapshot
   records twenty successes, three active jobs and no failures; no final acceptance
   is claimed. The last fully accepted hosted baseline remains 47a0240d.
3. Source review approved the metadata accumulation and exception/publication
   behavior. Local checks are recorded in 6.97-6.98; full M2, managed cutover,
   presented-revision save and all remaining conversion tracks retain their scope.

### 6.100. Direct live property/key layouts and reflections (local Windows milestone)

1. User confirmed Open Brush is the sole consumer and uses the managed C# API.
   Update the unreleased native layouts and repository callers directly; do not
   introduce API versions without asking or evidence of real external usage.
   Remove the uncommitted V2 property wrapper and the earlier V2 key wrapper.
2. The 64-byte property structure directly carries transformFlip at byte 56.
   The 80-byte key structure directly carries uintValue at byte 64 for exact
   Offset ticks. Existing property/key exports use one layout each; managed C#
   APIs remain unchanged. Property readback uses a synchronized owned snapshot.
3. Windows Unity/viewer/Godot builds pass. The existing loaded production C-export
   host proves all four canonical reflection values, queued-state isolation,
   invalid/truncated input rejection, explicit reflection clearing, and successful
   full-width Offset key insertion/removal through the direct layout. The existing
   ABI script now checks the single layout and passes 21 applicable guard cases;
   obsolete alternate-layout cases were removed. No new test framework was added.
4. Evidence: artifacts/direct-live-layout-native-build.log,
   direct-live-layout-cabi/report.json, direct-live-layout-cabi/run.log,
   direct-live-layout-abi-check.log and direct-live-layout-whitespace.log.
   Live transform-key reflections, spawn-specific transforms, managed cutover and
   broader backend acceptance remain open. Earlier full runs 39976407088 and
   39979516973 do not cover this source. Their latest snapshots have 36 and 14
   successful jobs respectively, no failed jobs and unresolved work; the fully
   accepted hosted baseline remains 47a0240d.
5. Commit a8c23f2e is pushed after an up-to-date pull, with `[CI VALIDATION]`
   at the start of its message. Full run 39981424089 targets
   a8c23f2e692b0a33863e3a657a39b36aef728fcd and is queued; no hosted acceptance
   is claimed. Receipt: artifacts/direct-live-layout-full-runs.json.

### 6.101. Native macOS Editor scratch-project setup correction

1. Full run 39976407088 reached the new native Editor wrapper. The retained reader
   package download/signing passed. Its activation log confirms licence activation
   succeeded, then Unity exited with `Couldn't set project path to` because the
   wrapper passed an empty temporary directory as an existing project. Runner JSON
   records activation exit 1, return exit 0 and no tests phase/XML. This is a source
   setup defect, not an unproven converter test failure or a licence-service outage.
2. Activation now uses Unity's documented `-createProject` on a fresh child path;
   return uses `-projectPath` for that created scratch project. Both run from the
   owned temporary parent, including cleanup after failure. Strict three-case XML
   checks, credential redaction and unconditional licence return remain required.
3. Existing wrapper lifecycle tests now check creation/opening arguments. They,
   strict result-verifier self-tests, workflow matrix and whitespace checks pass
   locally. No actual corrected macOS acceptance is claimed before the new full run.
4. Cancelled remaining work in source-affected runs 39976407088, 39979516973 and
   39981424089; no weakened gates or repeated infrastructure retry. Evidence:
   artifacts/converter-checkpoint-ci-agent-mac-artifact/quill-editor-runner.json,
   quill-editor-activation.log, editor-project-fix-cancel-*.log,
   editor-project-fix-runner-tests.log and editor-project-fix-verifier-tests.log.
   The last fully accepted hosted baseline remains 47a0240d.
5. Final cancellation snapshots are retained as editor-project-fix-final-*.json.
   Runs 39976407088 and 39981424089 are terminal cancelled; 39979516973 is still
   resolving cancellation at that snapshot. Subsequent log audit establishes
   Android builds were cancelled before the strict collector started, whose actual
   inputs were BUILD_RESULT=cancelled and DEVICE_RESULT=cancelled. Device collectors
   lacked iOS artifacts because those producer jobs were cancelled without a runner
   or executed steps. These are cancellation consequences; no independent Android
   runtime regression was found. Evidence: artifacts/post-cancel-ci-agent-*.log.
6. Fix 8c9d9a3e is pushed after an up-to-date pull. New full `[CI VALIDATION]`
   run 39984582643 targets 8c9d9a3e32fb2bca2286e9e6c841aeca5c48a67f and is
   queued. Receipt: artifacts/editor-project-fix-full-runs.json. This source
   includes the direct-layout correction and all earlier runtime milestones.

### 6.102. Held transform-key publication correction (local Windows milestone)

1. Existing loaded C-export host reproduces a paused-playback defect: KeySet
   reaches Presented without updating the evaluated transform. Evidence:
   artifacts/held-transform-key-red/run.log. This is a confirmed live-edit bug.
2. Resample only the target transform at publication, invalidate affected spawn
   pose snapshots, and restore canonical transform after removal of the last key.
   Preserve playback clocks, actions and transient override precedence.
3. The same loaded C-export check now passes for insertion and last-key removal
   during paused playback. Windows Unity/viewer/Godot builds and existing CPU
   authoring checks pass; source review found no blocker. Evidence:
   artifacts/held-transform-key-green/report.json, held-transform-key-build.log
   and held-transform-key-cpu-check.log. No new test framework was introduced.
   Reflection-key support and hosted/backend acceptance remain separate.

### 6.103. Direct live transform-key reflections (local Windows milestone)

1. Add transformFlip directly to the single 80-byte key structure at byte 68.
   Canonical and animated transform edits then both support None/X/Y/Z reflection;
   no alternate layout, new version or managed C# API change is introduced.
2. Existing paused C-export checks cover insertion/replacement across all four
   reflection values and canonical restoration after removing the last key.
   Windows Unity/viewer/Godot builds, loaded C-export delivery and 21 ABI guard
   checks pass. The existing six-case rendered Loop/Offset playback regression
   check also passes. Independent source review found no stale caller/layout issue.
3. Evidence: artifacts/transform-key-reflection-build.log,
   transform-key-reflection-cabi/report.json, transform-key-reflection-abi-check.log
   and transform-key-reflection-playback-regressions/report.json. Reflected-key
   rendered/backend acceptance and managed cutover remain open; no broad M2
   acceptance is inferred from these local checks.
4. Separate commits 4a885990 (held-pose fix) and 38c84dae (reflection keys) are
   pushed after an up-to-date pull. Full `[CI VALIDATION]` run 39985738393 targets
   38c84daec7b547920cf24143f56291d98cf91a2f and is queued. Earlier corrected
   scratch-project run 39984582643 has ten successful jobs and no failures at its
   latest snapshot, with required work unresolved. Neither source has full hosted
   acceptance yet. Receipt: artifacts/transform-key-reflection-full-runs.json.

### 6.104. Held opacity/static-paint visibility key correction (local Windows milestone)

1. Existing loaded C-export host reproduces the same Presented-but-stale defect
   for opacity while paused (artifacts/held-key-properties-red/run.log). Refresh
   only opacity at publication, restoring canonical opacity after last-key removal.
2. Static-paint visibility key publication uses the existing held-time paint
   reselection path; removing its last visibility key restores canonical visibility.
   Loop/Offset edits retain their existing visibility policy. Group/timeline
   visibility and descendant-clock implications remain open.
3. Windows Unity/viewer/Godot builds, loaded C-export paused opacity/paint-visibility
   insertion/removal checks and existing CPU authoring checks pass. Source review
   found no blocker. Evidence: artifacts/held-key-properties-final-build.log,
   held-key-properties-green/report.json and held-key-properties-cpu-check.log.
   This source is newer than current full runs and has no hosted acceptance yet.

### 6.105. Hosted iOS runner acquisition failure (retry outstanding)

1. Corrected full run 39984582643 has an infrastructure cancellation in Unity iOS
   Metal Simulator Validation, job 120520333904: runner_id=0, zero executed steps.
   GitHub's check annotation states `The job was not acquired by Runner of type
   hosted even after multiple attempts`. It did not run the application and was
   not cancelled by an agent or superseding-run concurrency.
2. This required gate remains unverified. Preserve independent successes and retry
   only this job and its dependents once after the parent run becomes terminal;
   the hosted API rejects job reruns while the parent is active. No retry has been
   attempted. A repeated failure remains unresolved. Other current full work and
   M2 implementation continue; no full acceptance is claimed.
3. Evidence: artifacts/converter-checkpoint-ci-agent-ios-cancel-job.json,
   converter-checkpoint-ci-agent-ios-cancel-annotations.json and
   held-key-properties-ci-39984582643.json.
4. The same run subsequently has a second unstarted hosted-runner cancellation:
   Godot iOS Simulator Validation job 120520334284 on macos-26. Its runner_id is
   zero, it executed zero steps, and the check annotation gives the same acquisition
   failure. Both iOS visual gates await their one isolated retry after the active
   parent run becomes terminal; no application failure or retry outcome is inferred.
   Evidence: artifacts/converter-checkpoint-ci-agent-godot-ios-cancel-*.json and
   group-visibility-ci-39984582643.json.

### 6.106. Held static subtree visibility correction (local Windows milestone)

1. Existing loaded C-export host reproduces a stale paused group/subtree after
   visibility-key publication (artifacts/held-group-visibility-red/run.log).
   Refresh supported subtrees parent before child at the latest held CPU time;
   resample visibility, opacity, transforms and static-paint frame selection while
   bypassing actions, playback toggles and root/chapter/default-spawn state.
2. Support ordinary group/static-paint/spawn descendants, including a root whose
   clock is preserved. Validate topology and importer ownership at queue, prepare
   and publication. Nonroot timeline groups and media/instance/other-resource
   descendants explicitly return Unsupported until their clock/lifecycle refresh
   is implemented. This is an honest live-operation limit, not full M2 acceptance;
   timeline mutation remains required before full managed cutover.
3. Windows Unity/viewer/Godot builds, loaded C-export paused group hide/remove and
   descendant-world-visibility checks, existing CPU authoring and six rendered
   playback regression cases pass. Source review found no blocker. Do not reuse full
   manager seek/update for this operation: those paths can execute actions and
   restart unrelated sound playback.
4. Evidence: artifacts/held-group-visibility-build.log,
   held-group-visibility-green/report.json, held-group-visibility-cpu-check.log
   and held-group-visibility-playback-regressions/report.json. Full timeline support
   remains required by M2; no timeline/backend acceptance is claimed. This local
   source is included with the diagnostics correction/full-validation source in 6.108.

### 6.107. Interactive native live-edit lab (local Windows milestone)

1. User prioritised real-world interactive validation ahead of further batch
   expansion. Add a small IMGUI panel to the existing Unity sample, using sample-only
   direct C# bindings to the native live APIs. Keep the public managed SDK unchanged;
   do not use compile/reload preview or transient layer overrides for edits.
2. Controls cover real-file load/reload/unload, layer selection, canonical names/
   visibility/opacity/position/reflections/pivots/repeat, key set/remove at explicit
   times, playback/seek, marker drawing replacement and hierarchy lifecycle.
   Display queued/prepared/presented/rejected results and unsupported content.
3. The scripts compile against installed Unity assemblies with zero warnings/errors
   and also compile in the actual Editor. A D3D12 Play-mode session loads the real
   Fishgym export, renders scene content and attaches a live session to nine layers.
   D3D11 is unsupported for this sample's URP RenderGraph path; use D3D12 on Windows.
   Screenshot: artifacts/live-panel-fishgym.png. No manual click acceptance is claimed.
4. Real-scene diagnostics exposed the native/managed buffer mismatch in 6.108.
   After its fix and a fresh DLL/Editor, diagnostics are coherent. The actual panel
   callbacks on paused Fishgym publish eight revisions: opacity 0.35, transform-key
   insertion, group creation, paint creation, visible magenta marker geometry,
   reparent, transform-key removal and geometry-containing subtree deletion.
   Reload restores nine layers, opacity 1 and no added transform key, with playback
   running. This uses real files/rendering and the panel's paths, not a synthetic
   managed graph; it is automated Editor callback exercise, not physical click QA.
5. Rename returned Busy (-7) twice on the imported Fishgym paint at this milestone;
   the code permits that result for incompatible queued work or active importer
   ownership, and neither cause is proven by the available observations. Do not
   dismiss it as expected background activity. The later same-document recheck in
   6.115 passes through the playback-command coordinator and supersedes this open
   real-file rename result.
6. Evidence: artifacts/live-panel-real-file-report.json, live-panel-marker-visible.png,
   live-panel-after-edits.json, live-panel-reloaded-status.json and
   live-panel-reloaded-diagnostics.json. The sample opens a maximized Game view;
   Windows has an explicit D3D12 restart menu. Manual interaction, additional real
   scenes and broader backend acceptance remained open at this milestone. No save
   or general mixed transaction support is implied. Timeline undo was subsequently
   added in 6.113; source IMM files are not written by the lab.
7. Commit the matching tracked Windows plugin with this lab delivery so a fresh
   checkout has the required APIs and the 44-byte diagnostics fix immediately.
   The real-file report records the SHA-256 of the exact locally built DLL used.
   CI may replace it with its independently built binary as usual.
8. UI commit d822b9c1 and matching Windows plugin commit 15fae5aa are pushed after
   an up-to-date pull. Full `[CI VALIDATION]` run 40000145212 targets
   15fae5aa0bae2fd4819b22b5308b10155dd9e576 and is queued. The original Fishgym
   scene is restored and left playing in the open lab. Manual interaction and
   full hosted acceptance remain pending. Receipt: artifacts/live-panel-full-runs.json.

### 6.108. Managed diagnostics buffer boundary correction (local Windows milestone)

1. The real Fishgym lab displays incoherent diagnostics. Source inspection confirms
   Unity's GetLayerDiagnostics export writes the complete internal Player snapshot
   into a managed LayerDiagnosticsNative buffer of only 44 bytes. Internal field
   order differs and includes canonical state plus three double transforms. This is
   an actual interop memory-safety defect, independent of renderer compatibility.
2. Stop playback and replace the export with an explicit 44-byte scalar copy in
   the exact existing managed field order. Add size/offset assertions. Keep the
   public managed API unchanged; introduce no version or alternate layout.
3. Existing loaded C-export host adds one canary-guarded query and compares all
   eleven fields. Windows Unity/viewer/Godot builds and this check pass; the host
   reports internalBytes=280, exportedBytes=44, guarded=1. Source review approves
   the exact managed field order and failure handling. Evidence:
   artifacts/live-panel-diagnostics-native-rebuild.log and
   managed-diagnostics-cabi/report.json / run.log. Do not resume with the old DLL.
4. This source includes prior local held-opacity/static-subtree corrections. Old
   full runs are superseded because they contain the unsafe export. One targeted
   Unity iOS retry was requested on 39984582643 before discovery; further old-source
   retries are superseded by the required new full validation of this correction.
   Existing independent proofs remain scoped to their original sources.
5. Fix a5a1e9ef is pushed after an up-to-date pull. Full run 39995932133 targets
   a5a1e9efcffe106b8d9e5b90796728d25e41457b. Its Windows native build and required
   authoring/frame-table/key steps pass; full hosted acceptance is pending.
   Receipt: artifacts/managed-diagnostics-full-runs.json. The old Unity iOS isolated
   retry also failed to acquire a runner; its one permitted retry is exhausted and
   that infrastructure failure remains unresolved (converter-checkpoint-ci-agent-
   399845-retry-outcome.json). No further old-source retry is scheduled.

### 6.109. Snoopy hierarchy and camera controls (local Editor milestone)

1. User requested a middle-ground real scene and chose Snoopy over the larger Alex
   scene. Snoopy is loaded with 52 layers and three available viewpoints. The camera
   was moved to an authored view; whole-scene bounds had placed it too far out.
2. Replaced centred flat layer buttons with a left-aligned expandable tree based on
   parent IDs, explicit 18-pixel indentation and separate type/ID columns. Enlarge
   the tree pane, retain ancestors during filtering, and add expand/collapse controls.
3. Added authored-view selection arrows plus a separate Move action, Frame selected,
   and authored-view initial placement. Keep model/scene scale unchanged.
4. Stopped Play, refreshed scripts and reopened Snoopy in the owned D3D12 Editor.
   Verified the loaded assembly contains the tree and viewpoint methods, compilation
   is idle, and the real document is attached with 52 layers, sequence ready and
   playback Playing. Inspected the actual Game-view capture: left-aligned names,
   hierarchy indentation, foldout arrows, type/ID column and viewpoint controls are
   visible; the authored initial camera frames the scene closely. Evidence:
   artifacts/live-panel-tree-ready.json, artifacts/live-panel-tree-status.json and
   artifacts/live-panel-snoopy-tree.png. This is local visual inspection, not hosted
   acceptance or a manual exercise of every control.
5. User explicitly delegates restarts/reloads needed for requested UI changes.
   Apply and inspect changes in the running UI without another refresh handoff;
   preserve unsaved work and other application instances.

### 6.110. Snoopy baking comparison and workflow research (local diagnostic milestone)

1. Compared five visible `_baked` paint layers against their hidden source groups
   using temporary project variants and the same existing standalone converter.
   Original input files were untouched. Both conversions explicitly allowed the
   same unsupported Camera-layer omission; this is a comparison of the supported
   scene content, not complete Quill-project acceptance. Input/output and native
   library hashes are retained in artifacts/snoopy-baking-benchmark/identities.json.
2. Recorded the bounded experiment before sampling. Used the owned Unity 6000.6.0f1
   D3D12 Editor, RTX 4090, 1920x888, 8x MSAA, render scale 1, fixed authored camera,
   muted audio, hidden/disabled authoring panel, vsync off during sampling. Warmed
   two 1.25-second export segments, then sampled 10 seconds per run in order
   baked/original/original/baked/baked/original. Reset playback to zero every
   1.25 seconds in both variants to compare the export range, not differing loops.
3. Results: baked uses 18 paint submissions versus 58 original, with approximately
   455,600 triangles in both. Median Editor frame intervals across three runs were
   6.79/7.66/7.27 ms baked versus 80.28/86.42/77.61 ms original. A separate five-second
   control pair without ProfilerRecorders retained a large gap (8.88 versus
   93.32 ms). This does not isolate native CPU submission, GPU execution, or the
   cause of the unusually large D3D12 slowdown. Do not generalize the speedup to
   Quill, other backends, devices or release players; the cause remains unresolved.
4. Baked output is 8,505,507 bytes versus 7,270,662 original (about 17% larger).
   Warm-process load/attach observations overlap and do not establish a loading
   benefit. In-Editor process private-memory counters returned zero and are unusable;
   no memory-saving claim is accepted. Raw per-frame records and summaries are in
   artifacts/snoopy-baking-benchmark/run-*.json and summary.json.
5. Correctness limitation: matching-time captures at 0.5 and 1.0 seconds have similar
   composition/geometry but differ visibly in the reins' colour/shape. About 0.27%
   to 0.31% of pixels differ by more than 20 RGB levels. The initial zero-time
   captures did not settle at the same time and are excluded. Therefore these are
   existing-artifact comparisons, not proof of a lossless baking optimization.
   Retain this rendering/content discrepancy as unresolved; source edits after
   baking, bake semantics and converter/renderer behavior have not been separated.
6. Workflow research: the official [timeline guide](https://quill.art/tutorials_timeline.html)
   documents group flattening and the file-size advantage of transform keys.
   [Dan Franke's production tips](https://www.creativebloq.com/how-to/get-started-in-quill)
   recommend layer merging to reduce draw calls for Quest, cite a historical
   65-draw-call budget, and warn against merging static content into animation.
   His account supports a deliberate playback-optimization workflow rather than
   assuming cargo culting. The hidden editable groups plus visible flattened
   copies in Snoopy fit that workflow; the artist's actual intention is unproven.
7. Export is another plausible motive: Snoopy's local source folder also contains
   Alembic and FBX outputs. A [first-person Quill production case study](https://www.opastpublishers.com/open-access-articles/challenges-and-opportunities-of-virtual-reality-painting-animation-in-mainland-china-a-case-study-of-dark-tide.pdf)
   describes baking transform animation to cel animation for interchange. This is
   supporting workflow evidence, not evidence that Snoopy's artist followed it.
   The export-dialog Bake Transforms option is distinct from the saved flattened
   paint layers observed here; do not conflate them.
8. Decision: baking has a measured submission-count benefit for these artifacts,
   plus a larger output and reduced editability. Automatic converter baking is not
   justified by this diagnostic. Further work must first explain the local D3D12
   cost and establish equivalent source/baked content. Restored the original
   Snoopy IMM, authoring panel, and previous vsync/frame-rate settings after sampling.
   No production behavior or validation gate changed. Follow-up investigation and
   runtime optimization candidates are planned in the root-level
   [runtime rendering optimization plan](../IMM_RUNTIME_RENDER_OPTIMIZATION_PLAN.md).

### 6.111. Timeline workspace (active)

1. User requested an animation-editor layout and set the active goal to finish a
   fully functional timeline UI. Work is in the sample lab; public managed SDK
   behavior remains unchanged. Planned delivery: full-width tracks and ruler,
   playback/scrubbing, actual drawing/key data, key selection/editing/retiming,
   nested timeline context, frame navigation and usable inspector layout.
2. The first live reload exposed a native GetPlayTime crash while a document had
   not yet loaded its root. Crash stack is retained in the current Editor log;
   the owned Editor exited. UI now reads playhead state only after attachment.
   Native time queries now zero outputs and guard invalid/loading/rootless
   documents under the Player mutex. Fresh-Editor load confirmation subsequently
   passed as recorded below.
3. Added read-only layer timing, evaluated layer-to-document transform and stored
   key enumeration exports. They reuse the existing 48-byte transform and 80-byte
   key layouts; no API versions were added. Transform queries also support fixing
   Frame selected, which previously treated local bounds as world bounds.
4. Native Windows Release x64 builds pass. Fresh D3D12 Editor load/attachment
   succeeded with the guarded time path and query exports; the initial full-width
   timeline was visually inspected. Layout and direct interaction work continues.
5. Atomic same-layer/same-channel key batches now prepare an owned key vector and
   publish it once. This supports remove-old/set-new retiming without a partial
   move. Mixed layer/channel batches and invalid later removals reject atomically.
   The first focused run exposed a missed exactly-one-key seal classifier; it was
   corrected rather than weakening the assertion. The existing loaded WARP C ABI
   harness now passes safe unready time reads, key queries/layout errors, exact
   integer offset values, reflected transforms, atomic move and rollback cases,
   alongside existing metadata, rename, reflection and repeat checks. Evidence:
   artifacts/live-timeline-query-retime-cabi-fixed/report.json and run.log.
6. Playback clock publication now shares the scene-update mutex with time queries,
   preventing readers from seeing a next-tick clock with the previous pose. Native
   Release build and the loaded C ABI suite pass after this focused correction
   (artifacts/live-timeline-final-native-build.log, artifacts/live-timeline-final-cabi).
7. UI callbacks on real Snoopy published an opacity key (revision 1), moved it from
   0.5 to 0.75 seconds (revision 2), then deleted it (revision 3), restoring the
   evaluated opacity to 1 with no opacity keys. A separate fresh load inserted a
   drawing hold (30 to 31 frames, repeated handle verified) and removed it (back to
   30). These checks verify callback/PInvoke publication, not physical mouse dragging.
8. First timeline workspace implementation: full-width bottom tracks, aligned layer
   and channel rows, time ruler/playhead, frame stepping, queued paused scrubbing,
   zoom/range/Fit/Follow/Snap, key selection/add/delete/atomic drag or entered-time
   move/interpolation, and drawing/hold frame-table controls. A side inspector keeps
   existing layer edits available. Input capture releases on hide/disable/unload;
   blank track selection clears an old key target. Drawing work is bounded by
   visible rows and resolves subpixel frames as a bar. Smaller views can scroll
   toolbars rather than losing controls. Frame selected now transforms local IMM
   bounds through the evaluated ancestor transform and coordinate conversion.
9. Nested key lanes and complex drawing-clock projections are not presented as
   root-time data. Correct nested scope mapping (including finite repeats, offsets,
   visibility starts and held actions), remaining channel types and broader
   usability/interaction acceptance remain unfinished. Timeline undo was added
   subsequently in 6.113. There is no
   graph editor or save support in this milestone. The goal remains active; do not
   describe this first workspace as the completed fully functional timeline.
10. Fresh Editor native load, paused seek and optional installed-input binding pass.
    Compile checks pass without an Input System reference even with the backend
    symbol defined. Native C ABI evidence remains distinct from Unity event/visual
    checks and hosted acceptance. Source and matching Windows plugin were committed
    and pushed as 9901c28e445bd812cb8742dfd1ea9406e62a4b60 after pulling main.
    FULL [validation run 40038310055](https://github.com/icosa-mirror/IMM/actions/runs/40038310055)
    is in progress; native Windows/Android/macOS/iOS, iOS visual, Core and Web lanes
    are present. No passing conclusion is claimed. Receipt:
    artifacts/live-panel-timeline-ci-runs.json.
11. Injected actual Game-view input verified F9 hide/show and a ruler click at
    0.75 seconds (paused native time 9451 ticks). The first drag exposed command
    ownership bugs: immediate Pause could make Commit busy, and a later playback
    command could replace an authoring command before it ran. The sample now keeps
    its owned batch while waiting to commit and serializes playback behind edits.
    Repeating the Game-view mouse down/drag/up moved the opacity key from 0.5 to
    0.75 seconds and published revision 2. Clicking Delete key through Game-view
    mouse events published revision 3, removed the key and restored opacity 1.
    Evidence: artifacts/live-panel-timeline-final-drag-result.json,
    artifacts/live-panel-timeline-final-delete-result.json and the validation
    receipt. This is injected UI event delivery, not a claim of human acceptance.

### 6.112. Optional lab input dependency correction

1. Full run 40000145212 (15fae5aa) failed both Android URP and non-XR builds with
   CS0234 for UnityEngine.InputSystem. The prepared project retains active input
   backend 1 but replaces the manifest without the Input System package and removes
   its lockfile. ENABLE_INPUT_SYSTEM describes backend selection, not assembly
   availability; the guard already present in that failed source was insufficient.
2. The sample lab now resolves optional input devices at runtime and binds typed
   control delegates when devices change, removing the compile-time package
   dependency. Legacy input remains a guarded fallback. F9 uses an IMGUI key event.
   Sample-only link.xml preserves reflected Input System members when installed
   and allows that assembly to be missing. No CI gate or package policy changed.
3. Assembly-reference compilation passes with ENABLE_INPUT_SYSTEM defined and the
   Input System assembly reference removed, matching the failed dependency shape.
   Evidence: artifacts/live-panel-no-input-system-compile.log. Actual clean hosted
   Android builds and package-present IL2CPP input remain pending. Installed-package
   Editor binding was exercised against FastKeyboard/FastMouse successfully
   (artifacts/live-panel-input-adapter-check.json).
4. Previous native baseline run 39995932133 (a5a1e9ef) completed successfully with
   49 passing jobs. The later failed run also had an independent hosted iOS runner
   acquisition failure. Neither that infrastructure issue nor the Android source
   failures are described as passing; the next source push requires full validation.

### 6.113. Timeline history and stable key times (local milestone)

1. Timeline undo/redo now covers key channels, drawing frame tables and canonical
   paint repeat counts. History moves only after Presented, retains stacks after
   rejected edits, clears after incompatible document edits, and checks observed
   revisions/track contents before replay. It retains at most 64 actions and 32 MiB
   of snapshot payload; buttons and focused Ctrl/Cmd shortcuts are provided. These checks do not
   constitute atomic protection against an independent concurrent writer.
2. Review found that requested > presented is not sufficient evidence of an active
   edit: rejected commits leave the requested revision ahead permanently. History
   readiness now checks the requested commit's terminal status instead. A live
   Snoopy probe passed 20 steps, including actual Game-view Undo/Redo button events,
   exact frame-13 key restoration, a multiple-key track move/restore, redo branching,
   an asynchronously rejected missing-key removal followed by successful undo,
   frame holds, canonical repeat counts, synchronous invalid-input rejection and
   history clearing after another document edit. Evidence:
   artifacts/live-timeline-history-probe.json. These are injected UI events and
   native publication checks, not human acceptance or hosted validation.
   A second five-step paused probe passed reflected/Smoothstep transform restoration,
   Ctrl+Z/Ctrl+Shift+Z Game-view input, and observed external-edit invalidation
   without undoing that edit. The workspace refreshed after detecting the changed
   revision. Evidence: artifacts/live-timeline-history-packed-probe.json.
3. Preparing lossless replay exposed a key-time conversion bug: 24 fps frame 13
   is 6825 ticks, but converting its queried seconds back through truncation yielded
   6824. KeySet and KeyRemove now quantize to nearest native tick with finite,
   nonnegative and overflow checks. Playback/global piTick conversion is unchanged;
   no API layout or version was added. This fixes normal animation-range round
   trips, not arbitrary int64 precision beyond what the existing double ABI holds.
4. Native Windows Release build and the existing loaded WARP C ABI suite pass,
   including exact frame-13 get/set/remove and invalid-time rejection. Evidence:
   artifacts/live-timeline-history-native-build.log and
   artifacts/live-timeline-history-keytime-cabi/report.json / run.log. Source and
   matching sample Windows DLL are committed together for this correction.
5. Nested timeline mapping remains unfinished. Read-only design review confirmed
   that visibility starts, repeat cycles/clamps, mid-span Offset history and exact
   Stop boundaries prevent a general managed root-time delta. A native context
   clock/resolver with an explicit seek policy is needed; no guessed mapping has
   been implemented or accepted.
6. History source was committed and pushed as d35ae2e6 with the required
   [CI VALIDATION] marker after pulling main. FULL
   [run 40040654620](https://github.com/icosa-mirror/IMM/actions/runs/40040654620)
   is queued; local evidence does not establish hosted acceptance. Prior timeline
   run 40038310055 remains active, with Windows native delivery and Godot iOS visual
   success; both clean Android Unity builds are still running at the last confirmed
   snapshot. They are not reported as passing. Receipt:
   artifacts/live-timeline-history-validation.json.

### 6.114. Evaluated timeline clocks and nested workspace navigation (in progress)

1. The native read-only timeline clock query now exposes evaluated local time,
   elapsed time, repeat cycle, finite clamp, inactive-span and action-hold state,
   plus publication revisions under the scene mutex. The unversioned 96-byte
   output is capacity-checked; failed queries preserve caller memory.
2. Windows Release build and the loaded WARP C ABI suite pass root/nested clocks,
   finite and infinite repeats, inactive spans, authored Stop boundaries,
   no-mutation and output-capacity checks. Evidence:
   artifacts/live-timeline-clock-native-build.log and
   artifacts/live-timeline-clock-cabi/report.json / run.log. This is local native
   evidence; live managed/UI and hosted validation remain separate.
3. The workspace now provides root/Up/open-selected navigation,
   scope breadcrumbs, local key domains and evaluated playheads. Nested playback
   and scrubbing remain disabled until their playback policy is implemented.
   Root seeks now retain integer ticks after one rounding step. Live Unity
   P/Invoke checks passed: frame 13 evaluates to 6825 ticks, an injected
   Game-view next-frame click reaches 7350 ticks, and opening Snoopymove via its
   toolbar button shows local 6300 ticks/cycle 2 at root 25200 ticks. Its visible
   children and 1.5-second domain were inspected in the running UI. Evidence:
   artifacts/live-timeline-clock-frame13.json, live-timeline-clock-frame14.json,
   live-timeline-clock-nested.json and live-timeline-scope-nested.png. Camera
   framing remains available in nested scopes. The source-only compile without
   Input System also passes. No general local-to-root inverse is claimed.
4. The user preference between independent composition playback and playback
   tied to the current main-scene occurrence remains unanswered. Neither mode
   has been implemented by assumption. Other outstanding timeline features in
   earlier sections remain open.
5. Both clean Android Unity builds passed in full runs 40038310055 and
   40040654620. At the latest monitoring snapshot, neither run has failures, but
   both are still active; neither validates the new clock/scope source yet.
6. Clock commit 6d29a51d and workspace commit 3b39562b are pushed after an
   up-to-date pull. FULL [run 40043337869](https://github.com/icosa-mirror/IMM/actions/runs/40043337869)
   targets 3b39562b9b40bf60bb03041a14d863c61b5a5cd1 and is queued. Receipt:
   artifacts/live-timeline-scope-ci-runs.json. Original Snoopy is restored in the
   open Editor; the active goal remains unfinished.

### 6.115. Inspector key-time precision and real-file rename recheck

1. Root seeking previously copied a three-decimal display string into the key
   inspector. At frame 13 this gave 0.542 seconds rather than 6825 ticks, so a key
   added through the inspector could miss the playhead by four ticks. The key-time
   field now uses round-trip seconds from the quantized native tick. Fresh live
   Unity compilation/load and a frame-13 seek confirm the inspector converts
   back to exactly 6825 ticks, matching its evaluated playhead. Evidence:
   artifacts/live-timeline-keytime-inspect.json.
2. Repeated the original immediate Pause-then-rename sequence on Fishgym layer 5
   through the current panel coordinator. It retained the batch while the playback
   command was pending, then published rename revision 1 with matching name and
   fullname. Restoring the original name in the same document published revision
   2 with matching names. No reload or source-file write separated those edits.
   Evidence: artifacts/live-timeline-fishgym-rename-result.json. This closes the
   lab's earlier Busy failure; it does not establish which owner caused the old
   insufficiently instrumented attempts or fix arbitrary hosts bypassing the
   coordinator.

## 7. Consolidated documents

The following obsolete documents were removed after their useful scope and
constraints were incorporated here or retained in the detailed references:

| Removed document | Replacement |
|---|---|
| `stroke reader plan.md` | Sections 2, 4 and 5; current API/package references |
| `imm_stroke_reader_plugin_plan.md` | Section 5; current API/package references |
| `imm_stroke_export_plan.md` | Section 5's collector and playback constraints |
| `stroke reader readme.md` | Current package README and Unity plugin reference |
| `imm-stroke-reader-quill-gap-report.md` | Sections 2 and 5; consumer scope remains explicit |
| `IMM_TO_QUILL_HANDOVER.md` | Sections 4 and 5; current converter/package sources |
| `QUILL_TIMELINE_TO_IMM.md` | Sections 2-4 plus converter timing findings and native source |

Git history retains their historical proposals and evidence. Their old setup
tasks, sample API copies and unsupported-feature claims are not current requirements.
