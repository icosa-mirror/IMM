# Quill source reader regressions

Run from the repository root without Unity or a native library:

```powershell
dotnet run --project code/appImmStrokeReader/tests/SharpQuillReader/SharpQuillReader.csproj
```

The small project fixtures exercise the production SharpQuill JSON and binary
reader. They contain no private project assets.

Optionally supply a local Quill project collection as the sole argument. This
checks real hierarchy/metadata without loading every paint point or picture, and
reads embedded sound directly to verify PCM bytes, payload boundaries and exact
qbin write/read-back. Original
projects are not modified. Audio support covers the observed 16-bit PCM layout;
unsupported encodings and truncated/oversized payloads fail explicitly.

Picture writer checks cover nested RGB/RGBA project write/read-back for 2D and
mono/stereo equirectangular metadata, raw header/payload bytes and rewritten offsets.
Malformed/unsupported media must reject before replacing existing project files.
These checks do not validate Quill application loading or stereo appearance.

Sound writer checks cover mixed picture/mono/stereo PCM16 WAV project read-back,
exact samples and spatial settings, observed headers, ancillary WAV chunks and
malformed-input output preservation. The writer does not decode native encoded
IMM sound or establish Quill application/native playback acceptance.
