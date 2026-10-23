Quill to IMM - Windows x64

1. Extract the complete folder. Keep its files together.
2. Double-click QuillToImm.exe to open the conversion window.
3. Choose your Quill project folder and an IMM output file, then click Convert.

You can also use the command line from a terminal in the extracted folder:

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

The window and command line share the same conversion, cancellation and output
protection. The window can save a report beside the IMM and displays omissions
and errors. Unity editor integration is a separate planned interface.

See licenses/ and THIRD_PARTY.md for dependency notices.
