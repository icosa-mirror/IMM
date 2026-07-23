# IMM Stroke Reader (Unity Package)

This package provides the IMM stroke reader runtime for Unity and exposes a C API with managed bindings.

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

## Contents

- `Plugins/`: native binaries (Windows/macOS/Android)
- `Runtime/`: managed bindings and runtime scripts
- `Runtime/ThirdParty/SharpQuill/`: vendored SharpQuill C# source (temporary, will be replaced by NuGet package)
- `Editor/ImmToQuillConverterEditor.cs`: menu commands `IMM/Convert IMM To Quill...`, `IMM/Convert Selected IMM To Quill`, and `IMM/Convert All Selected IMM To Quill` (selection is from Unity Project view; all write to `~/Documents/Quill/<imm-file-name>/`)
- `Samples~/Examples`: sample scripts and scenes
