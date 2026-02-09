# IMM Stroke Reader (Unity Package)

This package provides the IMM stroke reader runtime for Unity and exposes a C API with managed bindings.

## Contents

- `Plugins/`: native binaries (Windows/macOS/Android)
- `Runtime/`: managed bindings and runtime scripts
- `Runtime/ThirdParty/SharpQuill/`: vendored SharpQuill C# source (temporary, will be replaced by NuGet package)
- `Editor/ImmToQuillConverterEditor.cs`: menu commands `IMM/Convert IMM To Quill...`, `IMM/Convert Selected IMM To Quill`, and `IMM/Convert All Selected IMM To Quill` (selection is from Unity Project view; all write to `~/Documents/Quill/<imm-file-name>/`)
- `Samples~/Examples`: sample scripts and scenes
