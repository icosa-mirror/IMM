# Wasm paint tessellation Phase 2 results

These results compare the schema-v5 JavaScript tessellator baseline with the
native direct-packet implementation on the same machine and committed
`exampleImmFiles/sample1.imm` fixture. Both primary samples contain five runs.
Raw observations are stored beside this report.

## Eager decode comparison

| Measurement | JavaScript median | Native packet median | Change |
|---|---:|---:|---:|
| Time to first meaningful frame | 204.222 ms | 191.645 ms | -6.2% |
| Native/Wasm decode | 39.600 ms | 42.000 ms | +6.1% |
| Worker marshaling | 6.700 ms | 34.600 ms | +416.4% |
| Geometry construction | 53.900 ms | 10.400 ms | -80.7% |
| Decode + marshal + geometry | 100.200 ms | 87.000 ms | -13.2% |
| Three.js geometry creation/upload | 11.800 ms | 11.500 ms | -2.5% |
| First upload render | 22.400 ms | 20.300 ms | -9.4% |
| Worst observed long task | 56.000 ms | 54.000 ms | -3.6% |
| Mean frame interval | 16.664 ms | 16.664 ms | neutral |
| 95th-percentile frame interval | 16.900 ms | 16.900 ms | neutral |
| 99th-percentile frame interval | 17.000 ms | 17.000 ms | neutral |
| JavaScript heap after settle | 53,487,450 B | 52,672,498 B | -1.5% |

Every run retained the same 75 layers, 42 meshes, 802,890 total triangles,
40 draw calls, and 724,826 triangles at the selected playback point. No run
reported a page or console error.

The result is not a general 5x browser speedup. Native geometry arithmetic is
5.2x faster, but standard Wasm memory cannot be detached and transferred, so
the final packet must be copied to JavaScript-owned memory. That copy moves cost
from geometry construction into marshaling and limits the observed eager
first-frame improvement to 6.2%.

## Normal staged-loading comparison

A same-build A/B comparison selected either the native packet builder or the
retained JavaScript oracle while leaving all other code unchanged.

| Mode | First meaningful frame median | Range |
|---|---:|---:|
| JavaScript oracle | 211.422 ms | 208.787–261.981 ms |
| Native packet | 202.405 ms | 191.750–299.305 ms |

The staged median improved 4.3%. The ranges overlap substantially, so this is a
small user-facing gain rather than evidence of a large perceptual change.

## Memory and buffer accounting

One instrumented eager run of each path recorded identical 58,916,864-byte peak
Wasm heap capacity. On the native path, the largest individual paint packet was
2,949,520 bytes. Final paint geometry occupied 23,540,556 bytes; packet transfer
was 23,544,460 bytes, an additional 3,904 bytes (0.017%) for headers, records,
and alignment. Estimated uploaded geometry plus texture allocation was
31,540,556 bytes.

The fixture used 403.1 geometry bytes per source point and 29.3 geometry bytes
per rendered triangle. These figures describe final CPU/GPU buffer payloads;
browser and driver implementation overhead is not included.

## Gate assessment

1. Correctness passes on the committed fixture: the native packet and
   JavaScript oracle agree for every drawing and all typed geometry attributes,
   within the documented floating-point tolerance where byte equality is not
   appropriate.
2. The committed normal fixture has no load or steady-state frame regression.
3. The native packet does not grow the measured Wasm heap on this fixture and
   has negligible packet-format overhead.
4. The speed benefit is concentrated in geometry construction. End-user loading
   improves by 4–6%, which is modest but measurable.
5. The extended corpus results below satisfy the remaining scale gates. The
   JavaScript tessellator has therefore been removed from the production worker;
   its standalone deterministic implementation remains only as a test oracle.

## Extended corpus

Private fixture paths are intentionally omitted from committed project
documentation. Raw reports retain only source basenames so runs can be matched
locally.

| Tier | Scale | JavaScript first frame | Native first frame | Change |
|---|---|---:|---:|---:|
| Small static | 6 KB, 3 layers, 48 triangles | 49.611 ms | 53.792 ms | +4.2 ms |
| Medium animated / paint-heavy | 20 MB, 313 layers, 5 chapters, 4.09M triangles | 2,001.874 ms | 1,544.208 ms | -22.9% |
| Practical upper bound | 440 MB, 5,402 layers, 15 chapters | 14,412.495 ms | 12,818.681 ms | -11.1% |

The small-fixture ranges overlap (47.645–92.513 ms before and
49.808–93.148 ms after); the 4.2 ms median shift is treated as noise-scale
absolute overhead, not a material small-document regression. Its combined
decode, marshal, and geometry work decreased from 3.4 ms to 3.1 ms.

The medium/paint-heavy run reduced geometry construction from 733.1 ms to
136.7 ms and combined worker work from 1,446.0 ms to 1,159.0 ms. Peak Wasm heap
was unchanged at 406,913,024 bytes, while settled JavaScript heap decreased from
435,729,527 to 432,278,851 bytes.

The upper-bound staged run retained the same 1,037,565,952-byte peak Wasm heap.
Settled JavaScript heap decreased from 338,073,784 to 226,054,464 bytes. Its
steady sample remained demanding in both modes, but mean frame interval improved
from 22.66 ms to 18.52 ms and no correctness or browser errors were reported.
