# IMM content pipeline: shared roadmap

Baseline source review: `ee5e02f3` on 2026-10-21; subsequent implementation and
validation revisions are recorded below.
This is the planning entry point for Quill conversion, IMM content extraction and
runtime authoring. C1 static conversion and initial C2 paint animation, root defaults
and viewpoint mapping use the shared offline graph/compiler. Native root-key/comic
construction and import round trips also exist. Quill source chapter/startup mapping,
broader playback/navigation, media, reverse conversion and live-authoring acceptance
remain open; none of these initial slices closes the full roadmap.

Local native regressions and four paired paint/viewpoint metadata comparisons pass.
Taperecorder's normal default-viewpoint DirectX capture is pixel-identical to its
reference. Live GitHub status checked at 2026-10-21 20:23 UTC confirms full hosted
validation success for compiler correction ac34f4f1 (39645977224) and viewpoint
revision 740eec8a (39646549914). These runs close the earlier Unity CS1061 and
Godot iOS capacity failures with passing replacement gates, including Unity Android
URP/non-XR Firebase, Android Vulkan, Windows DirectX/Vulkan and Godot iOS Metal.
Later sound-import revision 179a9f30 is under validation in 39669262483; it has no
reported failed jobs at this check, but is incomplete. A queued macOS build has no
runner assigned. Later APIs still need their own platform/package acceptance;
hosted success does not establish skipped hardware gates or full content fidelity.
Confirmed regressions take priority over new feature work. See section 6.26.

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
| Groups and hierarchy | Existing | Authoring reader exposes hierarchy; SharpQuill adapter flattens | Existing group/paint graph | Group hierarchy operations within limits | Map local structure without flattening |
| Static properties and pivots | Existing, including flip | Local/world properties exposed | Pivots and explicit X/Y/Z flips implemented with additive offline APIs; native platform acceptance pending | Canonical properties within existing live ABI limits | Static layer/pivot flip mapping implemented and tested locally |
| Drawings, held frames and repeats | Existing | Frame buffers/rates/repeats exposed; adapter consumes them | Existing drawings/frame references; one document frame rate | Drawing/frame operations within limits | Preserve reuse; resolve mixed source rates explicitly |
| Layer animation and nested timing | Keys/timing parsed | Key query APIs exist; SharpQuill adapter does not populate key channels | Supported keys/timeline properties; offline key flip preservation implemented | Visibility/opacity/transform key subset; no new live flip contract | Map offsets, duration, repeats and interpolation against references |
| Root actions, chapters and startup | Source actions and nullable startup metadata parsed | Root keys/properties exposed and supported keys imported; Windows local round trips pass; cross-platform CI pending | Root keys and comic export with action markers implemented; stored chapter count round-trips; playback controls not yet accepted | No complete chapter authoring claim | Quill root-action/startup mapping still needs paired reference and playback evidence |
| Viewpoints / spawn areas | Tracking defaults and preview offsets parsed | Spawn metadata and encoded previews exposed; SharpQuill adapter skips spawn layers | Spawn graph/import/export/default selection and immutable preview assets | Spawn edits within limits | Stored preview conversion implemented; generated previews, exact JPEG parity and broader navigation acceptance open |
| Pictures and panoramas | Stored 2D RGB/RGBA pictures mapped by project converter; panorama/depth mappings remain unverified | Decoded pixels and original encoded picture assets exposed; optional adapter layers | Offline picture construction/import with shared immutable assets, projection and viewer locking | Outside live scope | 19 isolated 2D reference pictures match pixels and exposed metadata; stereo/panorama and playback acceptance remain open |
| Sound | Spatial metadata and observed 16-bit PCM-to-WAV input exist | Encoded WAV/Vorbis/Opus queries and metadata import | Offline PCM16 construction plus encoded sound import/passthrough | Outside live scope | Add source mapping; validate timing, gain, looping and spatial playback |
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
   static and initial animated paint mappings now use this graph/compiler;
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

| Milestone | Work and dependency | Exit evidence |
|---|---|---|
| C0: reference and reuse decision | Initial evidence and C1 scope recorded in converter findings: all 21 source inventories, four decoded pairs and one large metadata pair | Four-brush/static reuse supported; reflected-transform gap demonstrated; controlled re-export, rotational-opacity case and playback remain unverified |
| C1: static paint conversion | Implemented in d0e7ea31; exact decoded paint-subset matches for two cases; identical taperecorder DirectX capture; Windows regressions and Unity package compilation/tests pass; Unity player/device jobs pass; hosted full CI subsequently passed in 39645977224 and 39646549914 | Four brushes, hierarchy and static properties compared to Quill export; safe output and actionable diagnostics; native visual check |
| C1b: shared reflection preservation | Implemented for layer/pivot and offline transform keys, including importer/comparer; Windows reflection/converter CI, four-platform builds/exports and Unity graph tests pass; hosted full CI subsequently passed in 39645977224 and 39646549914 | Valid X/Y/Z retained without legacy ABI changes or live-scope expansion; platform exports and regression gates pass |
| C2: animation and navigation | Drawings/holds, keys, nested timing, root properties/actions in the graph, viewpoint/default mapping and stored previews implemented. Local metadata comparisons and default-camera capture pass within recorded scope. Source chapter/startup mapping, generated previews, precision differences and broader playback/navigation acceptance remain open | Matched timing/hold/navigation behavior on representative Quill reference exports and native playback |
| C3: media and coverage | Offline picture construction/import, source 2D conversion and PCM16 sound construction implemented. Encoded sound queries/import implemented. Sound source mapping, stereo/panorama conversion and playback acceptance remain open | Panorama/picture appearance and audio playback/timing compared with Quill output; unsupported data reported |
| C4: unrepresented source cases | Investigate models, depth/cubemap and other cases only when suitable evidence exists | Explicit supported/rejected mappings; no inferred completeness from enums |
| R1: reader and consumer contract | Zero-width tip import loss fixed; local and Windows CI export/import/export checks pass. Reconcile remaining authoring/legacy views and consumer coverage | Reader extraction and selected adapter output preserve intended content or report loss |
| R2: reverse conversion | Address SharpQuill adapter hierarchy/keys and Quill writer media limits when full IMM-to-Quill delivery is selected | Quill-project read-back and application checks for the declared subset; no silent media omission |
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
5. `WriteImmAsQuillProject` invokes the bundled Quill writer. That writer filters
   Picture/Sound/Model children, despite picture data being available in memory,
   and throws for nonempty Gallery pictures. Do not promise picture/media project
   export based on adapter construction alone.
6. The adapter includes file-load fallback through memory for paths that native
   file loading cannot open. Unity editor conversion commands and output location
   remain documented in the stroke-reader package README.
7. Open Brush's historical flattening policy is a consumer decision. This review
   did not inspect a current external Open Brush checkout; verify that consumer's
   actual code before declaring its current feature coverage or planning changes.

## 6. Source authority and validation

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
