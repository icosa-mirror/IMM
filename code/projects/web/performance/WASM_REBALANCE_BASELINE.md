# Wasm runtime rebalance baseline

This is the pre-migration performance record for the schema-v5 JavaScript paint
tessellator. Raw observations are in
`wasm-rebalance-before-sample1-5x.json`.

## Environment

1. Capture date: 2026-08-30.
2. Browser: Chrome 151.0.7922.174, headless, Playwright temporary isolated
   profile.
3. Machine: AMD Ryzen 7 7800X3D, 16 logical CPUs, 33,520,889,856 bytes system
   memory.
4. Viewport: 1280 by 720 CSS pixels at device scale factor 1.
5. Fixture: committed `exampleImmFiles/sample1.imm`, 5,831,101 bytes.
6. Protocol: five sequential page loads, five-second settle, ten-second frame
   sample at the heaviest scanned chapter/time.

## Five-run result

| Measurement | Median | Minimum | Maximum |
|---|---:|---:|---:|
| Time to first meaningful frame | 204.222 ms | 198.306 ms | 259.315 ms |
| Native/Wasm decode | 39.600 ms | 38.800 ms | 40.900 ms |
| Worker marshaling | 6.700 ms | 6.000 ms | 7.000 ms |
| JavaScript geometry packing | 53.900 ms | 50.900 ms | 58.900 ms |
| Three.js geometry upload/build | 11.800 ms | 11.500 ms | 12.300 ms |
| First upload render | 22.400 ms | 20.400 ms | 44.100 ms |
| Worst observed long task | 56.000 ms | 54.000 ms | 79.000 ms |
| Mean frame interval | 16.664 ms | 16.664 ms | 16.665 ms |
| 95th-percentile frame interval | 16.900 ms | 16.900 ms | 16.900 ms |
| 99th-percentile frame interval | 17.000 ms | 16.900 ms | 17.000 ms |
| JavaScript heap after settle | 53,487,450 bytes | 49,942,374 bytes | 54,036,526 bytes |

Every run rendered the same 75 layers, 42 meshes, 802,890 total triangles,
40 draw calls, and 724,826 triangles at the selected playback point. Every run
reported no page or console errors.

## Comparison rule

1. Run the post-migration build five times with the identical harness arguments
   and environment.
2. Compare medians and ranges, not only the fastest observation.
3. Treat changed layer, mesh, triangle, draw-call, or selected-playback counts as
   a correctness failure before interpreting speed results.
4. Do not accept the migration if small/normal time to first meaningful frame
   materially regresses, if a new main-thread responsiveness problem appears,
   or if the measured benefit does not justify the new packet ABI.
5. This committed fixture alone does not satisfy the medium-animation,
   paint-heavy, or practical-upper-bound corpus tiers. Those tiers were later
   captured with machine-local fixtures; see `WASM_REBALANCE_PHASE2_RESULTS.md`.
