Quill to IMM command line - Windows x64

1. Extract the complete folder. Keep its files together.
2. Open a terminal in that folder and run:

   .\imm-convert.exe "C:\Art\My Quill Project" "C:\Exports\My Scene.imm"

The project folder must contain Quill.json and Quill.qbin.
Unity and a separately installed .NET runtime are not needed.
Use a Windows 10/11 installation with Windows Media Foundation available.

Options:
  --report "C:\Exports\My Scene.json"   Save conversion details as JSON.
  --allow-omissions                     Explicitly permit partial conversion.
  --overwrite                           Replace existing IMM output on success.
  --input-mib 512                        Change the default 256 MiB input limit.
  --help                                Show all options and exit codes.

Unsupported content is rejected by default. Partial conversion is labelled
"Converted with omissions" and lists the omitted content. An IMM file does not
by itself prove that all Quill animation, media or navigation behavior matches.

This download currently provides a command line. A desktop conversion window
and Unity editor integration are separate planned interfaces.

See licenses/ and THIRD_PARTY.md for dependency notices.
