# Audio360 pending seek check

This standalone Windows check uses the bundled Audio360 DLL, FBA and its default
asynchronous decoder thread. The audio device is disabled; rendered samples are
read through `getAudioMix`, so the check is silent and requires no Unity Editor.
The same memory stream class used by the plugin supplies generated marker audio.

```powershell
cmake -S code/libImmCore/tests/audio360 -B build/audio360-seek -G "Visual Studio 18 2026" -A x64
cmake --build build/audio360-seek --config Release
ctest --test-dir build/audio360-seek -C Release --output-on-failure
```

To check the compressed path as well, with FFmpeg available:

```powershell
build/audio360-seek/Release/imm_audio360_seek_smoke.exe --write-wav build/audio360-seek/marker.wav
ffmpeg -hide_banner -loglevel error -y -i build/audio360-seek/marker.wav -c:a libopus build/audio360-seek/marker.opus
build/audio360-seek/Release/imm_audio360_seek_smoke.exe build/audio360-seek/marker.opus
```

The executable verifies initial seeks at zero and 800 ms, looping from a 1900 ms
seek across the two-second boundary, and stop/reset/restart from zero. Each seek
is followed immediately by `play()`. Both the playback clock and the rendered
PCM amplitude must match the expected section. Restart follows the backend's
stop/reset sequence and allows that asynchronous stop to complete. The restart
seek result is printed separately; the backend still calls `play()` on failure.
An optional `--file <marker-file>` invocation checks the SDK file stream instead.

Verified on 2026-10-17 against bundled Windows SDK v1.7.12-62c879690ee3:

1. WAV and Opus passed initial playback, looping and zero restart. Initial seeks
   and stop/reset seeks returned `PENDING`; immediate `play()` preserved accepted
   initial seeks, including the nonzero target.
2. WAV accepted a new seek after stop/reset. Opus instead returned `FAIL` (-1),
   even for zero, although subsequent `play()` correctly restarted at zero.
3. A separate nonzero post-stop Opus probe (1300 ms) also returned `FAIL` before
   `play()`, with both memory and SDK file streams. Nonzero compressed restart
   is therefore not established by the passing zero-restart check. This is a
   distinct SDK limitation, not evidence that `play()` discards a pending seek.
4. Calling `play()` before an asynchronous stop completes produced silence in
   the probe. The passing restart check includes completion time; it does not
   claim arbitrary stop/play command interleavings work.

These checks use generated fixtures, not Chonky Beat or a live Unity playback
session. They do not validate Android or other Audio360 versions.
