# IMM Stroke Reader (Unity Package)

This package provides the IMM stroke reader runtime for Unity and exposes a C API with managed bindings.

`StrokeReader_GetSpawnAreaPreviewInfo` and `StrokeReader_GetSpawnAreaPreviewData`
copy original encoded viewpoint previews using an **authoring-layer index**.
Metadata reports dimensions, channels, PNG/JPEG encoding and source version
(1 = BGR, 2 = RGB). Info returns false when no preview is present; data returns
zero for invalid indices or a buffer smaller than the complete payload. The
document wrapper provides the same queries. These bytes are encoded assets,
distinct from decoded picture-layer pixels.

`StrokeReader_GetPictureAssetInfo` / `StrokeReader_GetPictureAssetData` copy the
original PNG/JPEG picture asset using an **authoring-layer index**. Metadata includes
projection, viewer locking, dimensions, original grayscale/RGB/RGBA channels,
encoding and byte count. Picture source version is zero. The document wrapper
exposes the same queries. A missing asset or invalid index returns false/zero;
data copying requires a buffer large enough for the complete payload. Existing
`GetPictureInfo` / `GetPicturePixelData` queries keep their legacy indices and
decoded RGBA pixels; encoded asset queries do not change those APIs.

## Install in a new Unity project

1. Install the package:
   - From a GitHub release zip: unzip `ImmStrokeReaderPlugin-Unity.zip` and add it with **Window > Package Manager > + > Add package from disk...**, selecting `com.immersive-foundation.imm-stroke-reader/package.json`.
   - From the UPM branch: add the package URL shown in the release notes.
2. Copy `sample1.imm` into a readable project location, for example `Assets/StreamingAssets/sample1.imm`.
3. To inspect the sample file without rendering:
   - Open **IMM > Stroke Reader Test**.
   - Click **Run Test on sample1.imm**.
4. To use it from a new scene:
   - Import **Package Manager > Imm Stroke Reader > Samples > Stroke Reader Samples**.
   - Create an empty GameObject.
   - Add `ImmStrokeReaderTest`.
   - Set its IMM file path to `Assets/StreamingAssets/sample1.imm` or an absolute path.
   - Press Play and check the Unity Console for layer/stroke counts.

For visible IMM playback in a scene, install `ImmPlayerPlugin-Unity`; this package only reads IMM document data.

## Root animation metadata

Read `StrokeReader_GetDocumentInfo` and iterate `rootAnimationKeyCount` with
`StrokeReader_GetRootAnimationKey(docId, keyIndex, out key)`. These read-only keys
use the same layout as layer animation keys. The count/query excludes implicit
default visibility; retained keys may be offsets or other properties rather than
actions, so the count is not a chapter count. Invalid indices/documents return
false. Use native binaries built from the same source version as the bindings.

## Root layer properties

`StrokeReader_GetRootLayerProperties(docId, out info, out transform)` returns root
properties through the existing authoring-layer-info and float-transform layouts.
It does not add the root to indexed child layers. Timing/repeats, opacity, pivots,
flips and static visibility are available separately from root keys. A sole None
visibility key at tick zero supplies static visibility; other key channels retain
their separate query contract. The API returns false for invalid/unloaded documents
or null native outputs. Use a native build marked `2026-10-21-ROOT-PROPERTIES` or newer.
Windows query/lifetime and static root import/re-export checks pass locally;
other-platform package CI for this addition remains pending. Transform values use
float precision, consistent with existing layer APIs.

## Contents

- `Plugins/`: native binaries (Windows/macOS/Android)
- `Runtime/`: managed bindings and runtime scripts
- `Runtime/ThirdParty/SharpQuill/`: vendored SharpQuill C# source (temporary, will be replaced by NuGet package)
- `Editor/ImmToQuillConverterEditor.cs`: menu commands `IMM/Convert IMM To Quill...`, `IMM/Convert Selected IMM To Quill`, and `IMM/Convert All Selected IMM To Quill` (selection is from Unity Project view; all write to `~/Documents/Quill/<imm-file-name>/`)
- `Samples~/Examples`: sample scripts and scenes

Encoded sound authoring/import supports WAV, Ogg Vorbis and Ogg Opus assets.
The reader exposes authoring-indexed sound metadata and copied encoded bytes.
Import/export preserves these bytes, spatial settings and legacy duration/playing
fields. Encoded frame count is -1; Opus sample rate is its 48 kHz decode rate.
Container validation does not establish packet decoding or audible playback.
See content pipeline section 6.25 for validation scope and remaining source mapping.

`SharpQuillCompat.ReadImmAsSequenceWithReport(path, includePictures, chapterIndex)`
returns the same flattened paint/picture subset as the legacy read method, with
`Sequence`, `Succeeded`, `IsPartial` and read-only `Issues`. Each issue includes a
code, source layer ID/name/type and message. Inspect the issues before treating a
result as adequate for a consumer: sound/spawn omissions, hierarchy, keys, root
metadata and chapter selection are reported. Root reconstruction remains incomplete,
so this API does not certify lossless conversion. It requires current authoring
reader queries; missing entry points are reported as `DiagnosticUnavailable`.
Existing read/write methods retain their behavior. This report concerns adapter
reading; the bundled Quill writer now serializes decoded RGB/RGBA picture children with
2D/equirectangular mono/stereo metadata. Unsupported/depth/Gallery media is rejected
before replacing project files. Supplied mono/stereo PCM16 WAV SoundData can also
be written with Flat/Positional, gain/loop, attenuation and one None/Cone modifier;
encoded audio, ambisonic and Model writing remain unsupported. This does
not recover content already omitted by the legacy adapter. See content pipeline
sections 6.31-6.33.

`SharpQuillCompat.ReadImmAsHierarchyWithReport(path, includePictures: true)` uses
current authoring queries to retain ordered groups, local transforms/pivots/flips,
root properties, paint drawing/frame reuse and supported visibility/opacity/offset/
transform keys. It returns the same result/report types. Unsupported content and
unmapped key channels are reported. Unrepresentable integer/float timing fails
explicitly. A zero document frame rate selects a reported 24 fps sequence default;
paint rates remain unchanged. Check `Succeeded` and `Issues` before using the subset.
This mode does not change legacy flat reads. It does not certify codec identity,
full root/startup/navigation reconstruction, native audio or application acceptance;
re-encoding decoded geometry can change quantization (content pipeline section 6.34).


Offline PCM16 decoding is available through `StrokeReader_DecodeSoundPcm16`
(authoring layer index), `StrokeReader_CopyDecodedSoundPcm16` and
`StrokeReader_ReleaseDecodedSoundPcm16`. The positive 64-bit lease owns samples;
copy them and release in a `finally` block. Leases survive document unload, but
`StrokeReader_End` clears them. `StrokeSoundPcmInfo` is 16 bytes (sample rate,
channels, per-channel frame count and total byte count). Negative decode results
mean invalid data (-1), decoded PCM limit (-2), allocation (-3) or unsupported
format (-4). The required positive byte cap bounds output PCM, not codec scratch
or input memory. Copy requires capacity for the complete PCM; invalid handles or
undersized buffers return zero. Opus decodes at 48 kHz; WAV/Vorbis retain rate.
This synchronous offline API is independent of live playback. Windows standalone
reader decoding is locally verified; platform CI and reader-only iOS static linking
remain unverified. The SharpQuill adapters still omit sounds (pipeline section 6.35).
