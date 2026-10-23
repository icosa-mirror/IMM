# Portable converter dependencies

The bundle preserves the repository Apache 2.0 license, libImmCore MIT license,
SharpQuill Apache 2.0 license, .NET and Newtonsoft.Json notices, codec notices,
and the supplied Audio360/Oculus SDK license and attribution files in `licenses/`.
This software is based in part on the work of the Independent JPEG Group.

The libogg, libpng and zlib notice files match the binary versions recorded in
the repository dependency table. Their original sources are:

1. https://raw.githubusercontent.com/xiph/ogg/v1.3.5/COPYING
2. https://raw.githubusercontent.com/pnggroup/libpng/v1.6.43/LICENSE
3. https://raw.githubusercontent.com/madler/zlib/v1.3.1/LICENSE

Microsoft Visual C++ runtime files are taken from the installed Visual Studio
redistributable CRT directory, not copied from Windows system directories.
Their use and redistribution remain subject to the applicable Microsoft terms:
https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files

Audio360's supplied `ThirdPartyNotices.txt` covers its dependencies, not the
SDK's own acquisition agreement. Recovery/review of the original Audio360 1.7.12
and Oculus 32 SDK acquisition terms remains a public-release acceptance item;
this prototype's dependency licence documentation check is unfinished. Do not
substitute terms from a different/current Meta Audio SDK release.
