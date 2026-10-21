# IMM content pipeline: shared roadmap

Baseline source review: `ee5e02f3` on 2026-10-21; subsequent implementation and
validation revisions are recorded below.
This is the planning entry point for Quill conversion, IMM content extraction and
runtime authoring. C0 has reference evidence. C1 has an offline static converter,
exact native paint-subset comparisons, one identical DirectX reference capture and
passing Windows converter regressions. Unity package compilation/tests pass;
full engine/device CI remains pending.
The static slice's Unity package-import job has passed. Both standalone Android
gates now pass in fresh run 39609583383 after earlier dependency/capture failures.
Full acceptance remains unresolved. The shared reflection extension is locally verified;
native builds and packaged exports pass on all four platforms. Broader Unity/device
acceptance is limited by the unresolved full-validation result. Regression investigation takes priority
over further feature work when a source failure is confirmed. Independent roadmap
work continues while infrastructure is unavailable. Unity player/device jobs now pass; final full acceptance
is blocked by Godot iOS hosted-runner capacity: the original job and its one
permitted retry never started. Zero-tip fix workflow 39616195981 completed successfully.

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
| Root actions, chapters and startup | Source actions and nullable startup metadata parsed | Root-key value query implemented and locally verified on Windows; cross-platform CI pending. Chapter queries exist; authoring import still reports root keys unsupported | Complete root/comic/chapter authoring absent; comic compilation rejected | No complete chapter authoring claim | Inspect actual root keys and Quill export behavior, then extend required authoring APIs |
| Viewpoints / spawn areas | Existing defaults and properties | Spawn metadata exposed; SharpQuill adapter skips spawn layers | Existing spawn graph/import/export/default selection | Spawn edits within limits | Map export flags, tracking, volume and defaults |
| Pictures and panoramas | Metadata and embedded picture input exist | Picture metadata/pixels exposed; optional adapter layers | No picture construction in current graph/Unity exporter ABI | Outside live scope | Add construction and map projection/viewer locking/assets |
| Sound | Spatial metadata and observed 16-bit PCM-to-WAV input exist | No sound-content query surface in stroke-reader C API | No sound construction in current graph/Unity exporter ABI | Outside live scope | Map embedded audio, timing, gain, looping and spatial properties |
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
| C1: static paint conversion | Implemented in d0e7ea31; exact decoded paint-subset matches for two cases; identical taperecorder DirectX capture; Windows regressions and Unity package compilation/tests pass; Unity player/device jobs pass; final full CI blocked by Godot iOS capacity | Four brushes, hierarchy and static properties compared to Quill export; safe output and actionable diagnostics; native visual check |
| C1b: shared reflection preservation | Implemented for layer/pivot and offline transform keys, including importer/comparer; Windows reflection/converter CI, four-platform builds/exports and Unity graph tests pass; final full CI blocked by Godot iOS capacity | Valid X/Y/Z retained without legacy ABI changes or live-scope expansion; platform exports and regression gates pass |
| C2: animation and navigation | Initial animated-paint conversion implemented: drawings/holds, layer keys and nested timing; local native synthetic checks and exact Fishgym paint/key metadata comparison pass. Viewpoints, root/chapter behavior and evaluated playback remain outstanding | Matched timing/hold/navigation behavior on representative Quill reference exports and native playback |
| C3: media and coverage | Add required offline picture/sound graph and ABI construction; preserve assets and spatial settings | Panorama/picture appearance and audio playback/timing compared with Quill output; unsupported data reported |
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

Status checked at 2026-10-21 14:36 UTC:

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
