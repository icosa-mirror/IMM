# Wasm runtime rebalance Phase 2.1 implementation results

Implementation and committed-fixture validation date: 2026-09-01.

## Implemented boundary changes

1. Content-layer IDs and drawing resource IDs are indexed when a native scene
   opens. Drawing packet construction no longer scans the content-layer
   collection, and the worker uses its captured content-layer index for staged
   asset marshaling.
2. Worker diagnostics distinguish requested and effective load modes and retain
   the error that caused an eager fallback.
3. Every staged request reports native decode, native packet build,
   Wasm-to-JavaScript copy, packet parse, worker transfer, indexed lookup,
   adapter, and payload-byte measurements as applicable.
4. Application and library loading report requested, initially loaded, deferred,
   and background-completed work counts. Library consumers can inspect the live
   telemetry through `IMMAsset.loadTelemetry`.
5. The performance harness requires an explicit `--mode eager|staged` selection
   when a non-default path is wanted and records that selection in its report.

## Committed compatibility fixture

Five staged runs used `exampleImmFiles/sample1.imm` on the same machine and
browser configuration. Raw reports remain ignored machine-local evidence.

| Measurement | Result |
|---|---:|
| Requested/effective mode | staged / staged in all runs |
| Fallbacks or browser errors | 0 |
| Requested/initial/deferred work | 38 / 38 / 0 |
| Drawing packet/resource lookups | 30 per run |
| Asset layer lookups | 8 per run |
| First meaningful frame median | 238.215 ms |
| First meaningful frame range | 205.848–339.806 ms |
| Staged native packet-build median | 10.000 ms |
| Staged drawing-decode median | 42.300 ms |
| Staged Wasm-copy median | 7.700 ms |
| Staged worker-transfer median | 2.298 ms |

A separate five-run eager check had a 197.847 ms median and a
195.157–262.551 ms range. The ranges overlap and these were not an interleaved
paired comparison, so this evidence does not establish a staged/eager
performance difference. It does establish that the effective mode and fallback
contract can distinguish the two paths.

## Private corpus validation

Private source paths and basenames are intentionally omitted. The medium
structure/animation workload used five runs; the practical upper-bound workload
used three. Every run requested staged loading, remained staged, and reported no
fallback or browser error.

| Measurement | Medium workload | Practical upper bound |
|---|---:|---:|
| Source bytes | 20,115,461 | 440,673,604 |
| Requested work | 545 | 46,033 |
| Initially loaded work | 188 | 1,534 |
| Deferred work | 357 | 44,499 |
| Background completed in measurement window | 357 | 597–676 |
| Drawing packets observed | 336 | 1,895 |
| First meaningful frame median | 674.349 ms | 3,353.121 ms |
| First meaningful frame range | 665.611–813.333 ms | 3,030.034–3,429.697 ms |
| Native drawing-decode median | 631.700 ms | 7,860.500 ms |
| Native packet-build median | 127.200 ms | 65.400 ms |
| Wasm-copy median | 96.300 ms | 51.700 ms |
| Worker-transfer median | 135.180 ms | 69,055.597 ms |
| Adapter median | 125.100 ms | 108.200 ms |
| Mean frame-time median | 16.663 ms | 129.540 ms |
| 95th-percentile frame median | 16.900 ms | 250.000 ms |
| Worst long-task median | 60 ms | 3,334 ms |
| Worst long-task range | 54–99 ms | 2,428–54,239 ms |
| Peak Wasm heap median | 393,216,000 B | 1,037,565,952 B |
| JavaScript heap median | 442,802,195 B | 507,671,152 B |

The previous Phase 2 reports did not record requested/effective mode or work
completion counts, so their upper-bound result cannot establish equivalent
background-loading state. Geometry transfer is nevertheless directly
comparable: the current median is 93,060,804 bytes, identical to the previous
native result. The responsiveness difference is therefore not evidence of
larger native geometry output.

## Gate assessment and next work

1. Drawing packet and asset requests no longer scan the content-layer
   collection.
2. Staged and eager execution, including fallback reasons, are explicit and
   validated by tests and repeated measurements.
3. The medium workload completes background loading and remains near a 60 Hz
   frame interval.
4. Avoiding full-document refresh and summary regeneration for inactive
   background drawings reduced upper-workload adapter time from a 5.54-second
   median to 108 ms. It did not resolve responsiveness: the workload reaches
   initial content in 3.35 seconds but processes only 597–676 of 44,499 deferred
   items during the measurement window, and aggregate request/transfer wait
   still dominates native build and Wasm-copy cost by orders of magnitude.
5. Phase 2.1's diagnostic exit condition is met. The first Phase 3 slice should
   reduce structure-dense request round trips and explicitly pace main-thread
   delivery/validation. Any batching must be bounded because large batches could
   worsen the remaining long tasks. Timeline migration is not justified by
   these results.
