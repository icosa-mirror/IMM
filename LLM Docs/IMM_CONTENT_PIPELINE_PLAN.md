# IMM content pipeline: shared roadmap

Reviewed against local source revision `ee5e02f3` on 2026-10-21.
This is the planning entry point for Quill conversion, IMM content extraction and
runtime authoring. C0 now has read-only investigation tooling and initial reference
evidence; converter/runtime implementation has not started.

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
| Paint and four brush sections | Existing | Existing point/brush extraction and SharpQuill mapping | Existing | Geometry replacement exists | Map conventions and compare Quill output |
| Groups and hierarchy | Existing | Authoring reader exposes hierarchy; SharpQuill adapter flattens | Existing group/paint graph | Group hierarchy operations within limits | Map local structure without flattening |
| Static properties and pivots | Existing, including flip | Local/world properties exposed | Existing pivots; transform has positive uniform scale and no flip field | Canonical properties within ABI limits | Establish flip/coordinate strategy from references |
| Drawings, held frames and repeats | Existing | Frame buffers/rates/repeats exposed; adapter consumes them | Existing drawings/frame references; one document frame rate | Drawing/frame operations within limits | Preserve reuse; resolve mixed source rates explicitly |
| Layer animation and nested timing | Keys/timing parsed | Key query APIs exist; SharpQuill adapter does not populate key channels | Supported keys and timeline properties exist | Visibility/opacity/transform key subset | Map offsets, duration, repeats and interpolation against references |
| Root actions, chapters and startup | Source actions and nullable startup metadata parsed | Chapter queries exist; authoring import reports root keys unsupported | Complete root/comic/chapter authoring absent; comic compilation rejected | No complete chapter authoring claim | Determine actual Quill export behavior, then extend only required shared APIs |
| Viewpoints / spawn areas | Existing defaults and properties | Spawn metadata exposed; SharpQuill adapter skips spawn layers | Existing spawn graph/import/export/default selection | Spawn edits within limits | Map export flags, tracking, volume and defaults |
| Pictures and panoramas | Metadata and embedded picture input exist | Picture metadata/pixels exposed; optional adapter layers | No picture construction in current graph/Unity exporter ABI | Outside live scope | Add construction and map projection/viewer locking/assets |
| Sound | Spatial metadata and observed 16-bit PCM-to-WAV input exist | No sound-content query surface in stroke-reader C API | No sound construction in current graph/Unity exporter ABI | Outside live scope | Map embedded audio, timing, gain, looping and spatial properties |
| Models | Reader skips Model/Unknown | No model mesh query/conversion surface in stroke reader | No model construction in current graph/Unity exporter ABI | Outside live scope | Conditional on suitable Quill reference evidence |
| File/memory output | SharpQuill writer supports a limited Quill-project subset | IMM-to-Quill entry point exists with losses below | Existing native IMM writer/compiler | Presented-revision save outstanding | Implement safe direct conversion entry point and diagnostics |

Native IMM picture/sound structures and model mesh serialization are broader than
the managed graph and Unity `ImmExporter_*` ABI. Do not interpret the empty managed
cells as absent native format support. Model serialization is implemented in
`toImmersiveLayerModel.cpp` / `fromImmersiveLayerModel.cpp`; it does not establish
Quill model mapping or rendering parity.

## 3. Reuse assessment and implementation boundaries

The preferred first candidate is:

`Quill project -> SharpQuill source -> explicit mapping -> ImmAuthoringDocument -> ImmAuthoringCompiler -> IMM`

1. Reuse graph transactions, validation, hierarchy, drawings, frames, keys, bounds
   compilation and export diagnostics where their contracts fit. No converter or
   mapping through this graph has been implemented yet.
2. Keep Quill semantics in the mapping layer. Extend the shared graph/exporter only
   when a demonstrated source case needs a reusable capability. Do not introduce
   another full document model or route conversion through Open Brush strokes.
3. The graph's `ImmAuthoringTransform` has no flip flag and accepts positive scale.
   The reader exposes source flip state, but the current export `TransformNative`
   also lacks an explicit flip field. Establish a reference-backed mapping or a
   focused shared extension; do not silently remove a reflection.
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
| C1: static paint conversion | In progress: metadata-only source reading added for preflight/selected payload loading; entry point and mapping remain outstanding | Four brushes, hierarchy and static properties compared to Quill export; safe output and actionable diagnostics; native visual check |
| C2: animation and navigation | Extend C1 for shared drawings/holds, keys, nested timing, viewpoints, root/startup/chapter behavior | Matched timing/hold/navigation behavior on representative Quill reference exports and native playback |
| C3: media and coverage | Add required offline picture/sound graph and ABI construction; preserve assets and spatial settings | Panorama/picture appearance and audio playback/timing compared with Quill output; unsupported data reported |
| C4: unrepresented source cases | Investigate models, depth/cubemap and other cases only when suitable evidence exists | Explicit supported/rejected mappings; no inferred completeness from enums |
| R1: reader and consumer contract | Reconcile authoring-view versus legacy reader access; specify consumer coverage; extend only concrete gaps | Reader extraction and selected adapter output preserve intended content or report loss |
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
   required for that slice. Reflections need an additive shared capability extension
   before broad corpus coverage. See the converter findings for measured limits.

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
2. These are reader/adapter data results. Quill-to-IMM conversion, Quill reference
   playback comparisons, visual/audio parity and new live-edit acceptance remain
   unverified. C0 has now compared existing local candidate reference files at the
   data level; the converter findings record scope, skipped strokes and hashes.
3. Reproduction commands and reader logs are in the converter findings and
   `code/appImmStrokeReader/tests/SharpQuillReader/README.md` and
   `tests/SharpQuillAdapter/README.md` under the same directory. The adapter check
   runs in Windows build CI. C1 adds synthetic reader regressions to the same job;
   the reader corpus remains a local check, not a CI fixture. Metadata-only reading
   is implemented and its local locked-qbin, full-read and corpus regressions pass;
   CI acceptance for that change is pending.
4. Preserve existing player regression and package/export gates when code changes.
   Live work must retain the authoring document's timing/failure/platform criteria
   and the web performance investigation procedure where applicable.

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
