# Web runtime performance evidence

The Wasm runtime rebalance uses before/after measurements from the same harness,
browser configuration, viewport, source files, warm-up, and playback sampling
window.

## Reproducing a run

1. Build or copy the matching decoder artifacts into
   `code/projects/web/app/public/decoder`.
2. Run the harness from `code/projects/web/app` with an explicit output file:

   ```powershell
   $env:IMM_WEB_HEADLESS = "1"
   node tests/web-player-performance.mjs --output ..\performance\run.json ..\..\..\..\exampleImmFiles\sample1.imm
   ```

3. Use the same machine and close unrelated high-load applications for paired
   comparisons. The harness uses a temporary isolated Chrome profile.
4. Add medium-animation, paint-heavy, and practical-upper-bound private fixtures
   as additional arguments without committing those source assets.
5. Preserve raw reports. Compare `readyMs`, worker decode/marshal/pack times,
   geometry upload and first-render times, long tasks, frame-time percentiles,
   peak Wasm and JavaScript heap, packet/geometry/GPU byte estimates, per-frame
   timeline/adapter cost, draw calls, and rendered triangles.

The committed `sample1.imm` result is a compatibility baseline, not a substitute
for the private representative corpus. A migration is not accepted from the
committed fixture alone.

The current committed comparison and its gate assessment are in
`WASM_REBALANCE_PHASE2_RESULTS.md`.
