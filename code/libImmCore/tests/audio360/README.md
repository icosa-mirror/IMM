# Audio360 seek and restart checks

These standalone Windows checks use the bundled Audio360 DLL, FBA and its default
asynchronous decoder thread. The audio device is disabled; getAudioMix provides
rendered samples silently, without Unity. Build the Windows appImmUnity target
first: the backend check links the resulting libImmCore.lib.

```powershell
cmake -S code/libImmCore/tests/audio360 -B build/audio360-seek -G "Visual Studio 18 2026" -A x64
cmake --build build/audio360-seek --config Release
ctest --test-dir build/audio360-seek -C Release --output-on-failure
```

For compressed audio, with FFmpeg available:

```powershell
build/audio360-seek/Release/imm_audio360_seek_smoke.exe --write-wav build/audio360-seek/marker.wav
ffmpeg -hide_banner -loglevel error -y -i build/audio360-seek/marker.wav -c:a libopus build/audio360-seek/marker.opus
build/audio360-seek/Release/imm_audio360_seek_smoke.exe build/audio360-seek/marker.opus
build/audio360-seek/Release/imm_audio360_backend_smoke.exe build/audio360-seek/marker.opus
```

The SDK check validates initial seeks at zero and 800 ms, a looping seek at
1900 ms across the two-second boundary, and repeated restarts at 1300, 800,
1600 and 0 ms. It checks both playback time and the rendered PCM section.
PENDING is accepted, and play() follows the requested seek immediately.
An optional --file <marker-file> uses the SDK file stream instead of the
plugin's memory stream class.

The backend check calls the actual piSoundEngine implementation, rather than
copying its transport logic. It repeats all four restart offsets twice,
both with and without looping. The first round lets stop complete; the second
calls Stop/Play consecutively without intervening processing. It verifies the
rendered audio, silence while stopped/paused, public playback states, Resume
rejection during/after stop, and stopping a paused sound. Captured backend errors
fail the check. Its marker amplitudes identify the source section; the SDK check
separately verifies the numerical playback clock.

## Findings and chosen sequence

Verified on 2026-10-17 against bundled Windows SDK v1.7.12-62c879690ee3:

1. The old sequence stop -> seek(0) -> seek(offset) -> play fails for Opus.
   After a requested 1300 ms restart, the seek returns FAIL (-1), playback time
   is 120 ms after six output blocks, and rendered RMS is 0.050 rather than 0.100:
   playback restarted at zero. WAV supports the old sequence.
2. Removing the zero reset, or calling play() before seeking, did not fix the
   post-stop Opus failure. Pausing and then seeking to zero also caused the next
   seek to fail. These alternatives were rejected.
3. The sequence pause -> seek(requested offset) -> play preserves Opus seekability.
   The backend therefore implements logical Stop using SDK pause, defers the reset
   to the next Play(offset), and maps SDK paused state to public Stopped while
   logically stopped. Ordinary Pause remains public Paused. No asset is reopened
   or decoded synchronously, and no per-frame retry loop is added.
4. An immediate play can be overtaken by a pending SDK pause, leaving silence.
   Only when the logical stop is still pending, the backend uses playScheduled
   with one device-buffer duration, allowing the pause to execute first. Completed
   stops and initial playback use ordinary play(). This adds one buffer
   of scheduled start delay to back-to-back Stop/Play (20 ms with the tested
   960-frame, 48 kHz configuration).
5. WAV and Opus passed the corrected SDK and backend checks at 480, 960 and 1920
   frames (10, 20 and 40 ms at 48 kHz), including repeated
   nonzero and zero restarts, initial pending seeks and loop wraparound.

For a reproducible old SDK sequence, set IMM_AUDIO360_RESTART_MODE=legacy before
running the SDK executable against the Opus marker. It intentionally fails the
position check. The noreset and wake modes reproduce the rejected alternatives;
default pause exercises the corrected sequence. These switches affect only the test.

These checks use generated fixtures, not Chonky Beat or a live Unity playback
session. They do not validate Android, other Audio360 versions, natural
end-of-stream restarts, or arbitrary overlapping transport requests.
