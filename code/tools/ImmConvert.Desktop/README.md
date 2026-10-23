# Quill to IMM desktop window

`QuillToImm.exe` provides project/output folder selection, explicit partial and
overwrite options, an input-size limit, progress, cooperative cancellation and
readable conversion details. A report is saved beside the IMM when selected.
Existing output is kept unless replacement is explicitly enabled. Closing a busy
window requests cancellation and waits for the worker before closing.

The window and CLI call `ImmConversion.StandaloneConversion`; native setup,
conversion, cancellation, output publication and reports have one implementation.
The UI snapshots settings before starting a worker and disables editing during
conversion. Per-stroke compiler progress is coalesced at a 100 ms UI cadence.
The original Unity package sources and their engine types are unchanged.

Build the combined self-contained Windows archive from the repository root:

```powershell
pwsh -NoProfile -File code/tools/ImmConvert/package-windows.ps1 -OutputDirectory artifacts/imm-convert-desktop
```

Extract the ZIP and open `QuillToImm.exe`. Both the desktop and command-line
interfaces are included, with .NET/WinForms, native DLLs, app-local CRT and
dependency notices. Keep the folder together. No separate Unity/.NET installation
is needed by this bundle; Windows Media Foundation remains a platform requirement.

The component test runs real WinForms events and real native conversion. It
checks repeated use, existing-output refusal, strict/partial conversion,
cancellation/recovery, cooperative close and layout at minimum size. Cancellation
of the UI is gated before native work for determinism; separate shared-host checks
exercise cancellation during conversion. The extracted-archive probe uses the
production UI/host/WinForms/CLR/native payload and only adds its test-entry files.

These tests do not claim live mouse/keyboard, all DPI/monitor configurations,
clean Windows VM, Unity consumer or full Quill fidelity acceptance. Live Windows
input verification is pending a connected Computer Use native bridge. Dependency
licence documentation checks are tracked separately in the shared plan.
