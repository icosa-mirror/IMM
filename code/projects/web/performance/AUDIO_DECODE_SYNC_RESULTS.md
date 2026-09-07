# Audio decode completion synchronisation

1. **Failure and interaction.** The extended browser test loads `sample1.imm`,
   recovers after a failed load, and checks autoplay audio synchronisation.
   CI run `34141897094` on `d39436d4` recorded a maximum drift of 519.5 ms
   against the 50 ms limit. All three sounds decoded and played; the current
   drift later returned to approximately zero. The assertion remains unchanged.

2. **Attributed cause.** Each asynchronous decode completion reconciled sources
   against the previous visual snapshot. A sound completing between frames
   therefore started at an old offset. A local reproduction with the exact CI
   decoder artifact and sixfold Chrome CPU throttling measured the second
   source starting at audio time 0.480 s against a snapshot sampled at 0.4213 s.
   That sound lagged by 58.7 ms while the first sound remained aligned. Completing
   the initial decode batch then restarted all sources, explaining recovery
   despite a retained maximum-drift failure.

3. **Change.** Decode completion now makes the sound available to the next
   `ImmWebAudio.update()`, which starts it from that update's evaluated snapshot.
   Finishing the initial decode batch no longer restarts resident sounds.
   Existing source disposal and replacement still stop obsolete sources.

4. **Validation.** A deterministic unit regression advances the audio clock by
   half a second while another sound decodes and verifies that it starts at the
   next snapshot's offset without restarting the first sound. It fails against
   the previous implementation. All 64 web unit tests and the web production
   build pass. Three complete browser verification runs using the CI decoder,
   fresh headless Chrome profiles, a 900-by-700 control viewport, and sixfold CPU
   throttling passed with maximum drift of 10.730, 10.730, and 10.683 ms.
   Production code was uninstrumented for those three runs.

5. **Scope and evidence.** This is a playback correctness fix, with no claimed
   load-time or frame-rate improvement. The experiment record, source/decoder
   hashes, diagnostic traces, and isolated browser results are stored locally in
   `artifacts/web-audio-drift/`. A full CI run is still required to confirm the
   original hosted environment and deployment gate.
