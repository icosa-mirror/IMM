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
over further feature work. Unity player/device jobs now pass; final full acceptance
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
   mapping now uses this graph/compiler; animation/navigation/media remain outstanding.
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
| C2: animation and navigation | Reference metadata audit: 35,213 transform keys in 403 layers retain count/time/value/interpolation; base/startup evaluation remains unresolved. Extend C1 for shared drawings/holds, keys, nested timing, viewpoints and root/chapter behavior | Matched timing/hold/navigation behavior on representative Quill reference exports and native playback |
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
   reference questions. This investigation adds no production animation support.
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
