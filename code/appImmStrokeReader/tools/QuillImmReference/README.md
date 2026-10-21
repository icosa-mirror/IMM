# Quill / IMM reference inspection

Read-only C0 investigation tools. They do not implement conversion or certify visual
parity. Inputs and generated reports may contain private scene content: keep reports
under ignored `artifacts/`, not in public fixtures or CI.

Reports include native root animation keys and chapter count separately from
ordinary layers. Use a reader binary with `StrokeReader_GetRootAnimationKey`.
Spawn layers also report tracking level, locomotion volume and translation mask
through `StrokeReader_GetLayerSpawnAreaInfo` for viewpoint mapping comparisons.
Sound layers report source settings, native encoded asset metadata and unique-path
`soundComparisons`. Comparisons explicitly map Flat/Positional and attenuation
modes; None/Cone settings are compared at reader float precision. Missing or
ambiguous matches remain unmatched. This does not decode/compare audio samples,
prove export provenance or establish evaluated playback/timing parity. Use a
reader binary with `StrokeReader_GetSoundAssetInfo` (2026-10-21 sound assets).
The root-key count/query excludes implicit default visibility and does not imply
that each retained key is an action or chapter marker.

Inspect an IMM's root keys without a paired source project:

```powershell
dotnet run --project code/appImmStrokeReader/tools/QuillImmReference -- --inspect-root $referenceImm code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/x86_64/ImmStrokeReader.dll artifacts/quill-reference/root.json
```

This loads the entire file through the native reader; it is not a streaming or
metadata-only IMM import. The report records the input/library hashes, native build
ID, native import duration, root keys and chapter count. The import duration excludes
file reading and report serialization. Both modes reject report/log paths that
replace their input files or native library.

1. Inventory every source project's metadata and qbin stroke headers without loading
   vertices. Add reference roots to locate filename/asset-ID candidates:

   ```powershell
   python code/appImmStrokeReader/tools/QuillImmReference/inventory_sources.py $sourceRoot --reference-root $referenceRoot --output artifacts/quill-reference/source-inventory.json
   ```

2. Inspect a small paired project/reference using the production SharpQuill reader,
   managed native declarations and hierarchy-aware Windows stroke reader:

   ```powershell
   dotnet run --project code/appImmStrokeReader/tools/QuillImmReference -- $projectFolder $referenceImm code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/x86_64/ImmStrokeReader.dll artifacts/quill-reference/pair.json
   ```

3. Append `--metadata-only` for large sources. This parses source metadata using the
   production `QuillSequenceReader.ReadMetadata` API, retaining drawing declarations
   and frame references without loading vertices or media. Native IMM
   loading still decodes the reference's assets; this is not a streaming native
   metadata reader. Source hashing still reads the qbin as a stream.

4. Inspect the JSON file with targeted queries. It records input hashes, raw versus
   parsed source type counts, static visibility through ancestors, hierarchy paths,
   transforms/flip, animation keys, paint rates/repeats/frame buffers and geometry
   deltas. Its native log is beside the JSON with suffix `.native.log`. Logging uses
   `[IMM_QUILL_REFERENCE]` and `[IMM_QUILL_INVENTORY]` for console summaries.

5. Correspondence uses unique hierarchy paths excluding the source root, then
   drawing/stroke/point indices. Differing stroke/point counts are reported and
   skipped for point comparisons. Max errors describe only compared points; they
   are not a pass/fail tolerance or an exhaustive geometry comparison. Do not use
   them to certify an unmatched or skipped stroke. Transform error treats `q` and
   `-q` as equivalent but reports flip separately.

6. Color reports compare both direct source values and an experimental sRGB-to-linear
   interpretation. These are measurements, not a selected production mapping. Width
   ratios can be large for very small quantized widths; use absolute errors and codec
   bounds when assessing unit conventions. Always-visible strokes do not contribute
   direction deltas because IMM omits their stored direction.

7. Metadata-only reports have null source point/stroke/brush totals and zero compared
   points. Exact frame-buffer equality does not prove equal playback time. Neither
   co-location nor a matching filename proves export provenance: compare content and
   regenerate a controlled Quill export when necessary.

8. The inventory scanner follows the bundled qbin layout: a 4-byte drawing count,
   40-byte stroke header and 56-byte vertices. It validates counts/bounds and skips
   vertex payloads. Its stroke totals count drawing declarations, including repeated
   references, rather than deduplicated semantic assets.

The .NET tool requires .NET 8 and the bundled DLL's dependencies beside it. It reuses
the adapter regression's Unity stand-ins; no Unity Editor or player is launched.
