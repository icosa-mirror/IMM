# Performance investigation procedure

## Objective and decision gate

1. Name the user interaction: opening a populated scene, starting animation,
   seeking to a later chapter while loading, or navigating between loaded chapters.
   Specify the workload and device. The Gallery integration is the primary host.
2. Choose its limiting metric before measuring. Record time to complete visible
   content, missing-content duration, frame stalls, or sustained frame budget as
   applicable. Report loader readiness and overlay dismissal separately.
3. Estimate the maximum plausible end-to-end saving from measured elapsed costs
   on the critical path. Do not sum overlapping worker and main-thread timings,
   treat bytes/counts as time, or interpret CPU submission time as GPU execution.
4. Normally invest in a prototype only when the evidence supports roughly 25%
   improvement in the limiting metric, or removal of a reproducible disruptive
   stall. CPU headroom at an existing 60 FPS is a separate outcome. A small,
   proven correctness fix need not meet a performance threshold.

## Bounded experiment

1. Write a short experiment record before coding: hypothesis, existing evidence,
   counterfactual upper bound, one proposed change, primary metric, regressions,
   maximum run duration, and decision that each possible result will trigger.
2. Start with one diagnostic run. If it cannot support the investment gate, stop
   that candidate rather than making it incrementally more elaborate. If timing
   remains unexplained, improve attribution before adding another prototype.
3. Save an identified baseline and record hashes of the library, worker, Wasm,
   host build, and input. Compare one independent change at a time. A combined
   comparison can assess a combined result but cannot identify its cause.
4. Use the shared benchmark turn agreed with the user. Read, claim and verify
   the status file; release it after owned browser processes close. Do not
   interfere with other agents' browsers or run heavy builds during their turn.
5. Use visible Chrome with an explicit fresh automation profile. Match viewport,
   Three.js version, renderer settings, camera, authored trajectory and audio
   policy. Keep loader modifications and profiling in the harness. Record any
   intentional reduction of work and limit the claim to that work.
6. For playback, warm a complete representative segment and measure at least two
   complete passes. For load/navigation latency, include the actual queue and
   prerequisites; preloading just the selected scene cannot measure queue delay.
7. Only repeat promising candidates: use at least three alternating pairs, then
   add pairs if order effects or variance prevent a decision. Keep all runs and
   report paired differences and ranges, including unfavorable outcomes.

## Correctness, attribution and acceptance

1. Require matching content, authored timing, resource identity and visible scene
   evidence. When loading order changes, validate eventual completeness as well
   as intermediate navigation. Verify audio, cancellation and disposal where
   the affected path uses them.
2. Measure profiler overhead against an uninstrumented run. Use coarse timers
   first; expensive per-object probes may establish counts but distort timing.
3. Name memory measurements precisely. Sampled main-thread JS heap is not total
   browser memory and can miss synchronous peaks. Do not attribute a memory
   change to one feature in a combined comparison.
4. Keep prototypes inactive in the production path until acceptance. Archive
   rejected experiments locally before restoring defaults. Do not accumulate
   speculative changes in a shipping bundle.
5. Record each decision with evidence and remaining limits. Revisit a rejected
   candidate only if the workload, device, implementation cost, or bottleneck
   evidence changes. Update the plan around the findings rather than continuing
   a predetermined architectural sequence.
