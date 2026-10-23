# Runtime rendering optimization for editable IMM scenes

Status: planned; initial Snoopy diagnostic completed. No optimization prototype
has been accepted or enabled. Updated 2026-10-23.

## Objective

1. Reduce playback cost for editable Quill/IMM hierarchies so authors need less
   manual flattening or animation baking for performance.
2. Preserve the canonical document, layer identities, animation and editing
   behavior. Build any optimized representation as disposable renderer data.
3. Support the native runtime both with and without Unity. Unity is an integration
   and measurement host, not a requirement for the optimization.
4. Prefer fixing existing renderer inefficiencies first, then submission batching,
   then selective caching. Runtime generation of baked geometry is a last candidate,
   not the assumed solution.

## Evidence and unresolved questions

1. The initial Snoopy experiment replaced five visible baked paint layers with
   their hidden editable source groups. Both variants used the same converter,
   common scene content and camera. Both explicitly allowed the same unsupported
   Camera-layer omission. The original project was not modified.
2. Measurements used Unity 6000.6.0f1, D3D12, an RTX 4090, a 1920x888 Game view,
   8x MSAA and render scale 1. The authoring panel was disabled during sampling,
   audio muted, and vsync disabled. Each ten-second sample followed two warm
   1.25-second segments; playback was reset every 1.25 seconds in both variants.

| Measurement | Editable source groups | Existing baked layers |
|---|---:|---:|
| Paint draw submissions | 58 | 18 |
| Submitted triangles | Approximately 456,000 | Approximately 456,000 |
| IMM output bytes | 7,270,662 | 8,505,507 |
| Median frame interval, three runs | 80.28 / 86.42 / 77.61 ms | 6.79 / 7.66 / 7.27 ms |
| Median interval, control without ProfilerRecorders | 93.32 ms | 8.88 ms |

3. This is an existing-artifact comparison, not an accepted lossless bake
   comparison. Matched-time images differ around the reins in colour and shape.
   Source edits after baking, bake semantics and converter/renderer behavior have
   not been separated. Initial zero-time captures were not synchronized and are
   excluded from visual comparisons.
4. The unusually large D3D12 timing difference is unexplained. Forty extra draw
   submissions are not sufficient evidence to attribute the whole difference to
   ordinary submission overhead. Editor intervals and profiler counters do not
   establish isolated native CPU or GPU costs.
5. Baked output is about 17% larger. Load observations overlap; the process memory
   counters used inside Unity returned zero and are unusable. No load-time or
   memory benefit has been established.
6. Local evidence is under `artifacts/snoopy-baking-benchmark/`: experiment record,
   variant generation inputs, conversion reports, input/library hashes, captures,
   per-frame samples, control samples and `summary.json`. Private source paths stay
   in ignored artifacts. These files are local evidence, not hosted CI artifacts.
7. The main content pipeline plan records this work in
   [section 6.110](LLM%20Docs/IMM_CONTENT_PIPELINE_PLAN.md#6110-snoopy-baking-comparison-and-workflow-research-local-diagnostic-milestone).
   This plan owns the proposed performance investigation; it does not replace the
   content pipeline or runtime authoring plans.

## Execution order

### 1. Establish equivalent workloads and reproducible baselines

1. Determine whether the source and baked reins differ in stored stroke geometry
   or colour, or acquire the difference during conversion/rendering. Compare a few
   exact frame-grid times, including settled zero time, without modifying the
   source project. Do not infer artist intent from the suffix alone.
2. Retain Snoopy as a real-world diagnostic. If its existing versions cannot be
   reconciled, create a small controlled hierarchy and its flattened equivalent
   from the same evaluated input. Match geometry, transforms, timing, visibility,
   brush settings and ordering. Keep identical resource sharing where applicable.
3. Capture baseline hashes, build configuration, backend, host, viewport, camera,
   MSAA, frame pacing, audio policy and loading state. Verify visible content and
   completed geometry upload before steady-state sampling.
4. Separate continuous playback from the repeated-seek diagnostic above. Measure
   seeks separately so reset work cannot masquerade as ordinary playback cost.
5. Exit: reproducible baseline and a controlled equivalent pair, or a documented
   reason that Snoopy can support only a narrower comparison.

### 2. Explain the D3D12 slowdown before designing an optimization

1. Measure native evaluation/submission CPU work, render-thread waits, GPU work,
   upload bytes/resource creation and presentation pacing. Start with coarse
   attribution; add narrow probes only where the elapsed cost is unexplained.
2. Inspect likely mechanisms without assuming guilt: per-layer/per-chunk constant
   updates, allocation and descriptor work, resource lifetime/fences, redundant
   uploads, shader/pipeline changes, streaming readiness and Editor interaction.
3. Compare paused presentation with advancing animation, and warmed playback with
   seeking. Distinguish a per-frame cost from first-use work or repeated loading.
4. Reconfirm a representative standalone native D3D12 workload if the evidence
   points to Unity/Editor integration. Use one existing supported second backend
   to determine whether the problem is backend-specific. Do not launch a broad
   platform matrix before attribution is useful.
5. Record the limiting cost and the maximum plausible end-to-end saving. Do not
   add overlapping CPU/GPU timings or convert draw counts into assumed time.
6. If an existing inefficiency explains the gap, fix it in its own commit and
   repeat the baseline before considering batching. A renderer defect must not
   become an undocumented requirement for authors to bake their scenes.
7. Exit: explained measured cost and a bounded candidate, or an explicit unresolved
   diagnosis. Do not call the initial speed ratio a general baking benefit.

### 3. Prototype one submission optimization, if justified

1. Prefer avoiding redundant setup or issuing compatible draws together over
   copying and transforming all stroke vertices into new frame geometry.
2. Investigate shared geometry buffers with per-layer transform/state indices or
   backend-supported multiple-draw submission only if the measured bottleneck
   warrants that work. Choose the smallest supported mechanism after phase 2.
3. Define compatibility from actual shader, brush, blend/depth, draw-in-time,
   directional-opacity and resource requirements. Preserve transparent ordering;
   do not reorder an entire scene merely to reduce calls.
4. Retain per-layer identity and visibility/culling decisions. Account for the
   cost of less selective culling when combining work. Unsupported combinations
   continue through the existing renderer path.
5. Keep the prototype opt-in and separate from the default renderer until the
   correctness and performance gates below pass.

### 4. Add selective caching only if a remaining cost warrants it

1. Establish how often the relevant state actually changes. Reuse already static
   geometry and transforms before adding a second cache of the same information.
2. Consider caching compatible submission groups or evaluated static subtrees.
   Avoid a cache whose lookup/invalidation costs exceed the measured work saved.
3. Invalidate only affected data after changes to geometry, hierarchy, properties,
   animation or resource bindings. View-dependent state and per-document-instance
   overrides must not be shared as though they were canonical document state.
4. Use the runtime's actual publication/revision boundaries: a requested edit must
   not expose a mixture of new geometry and old cached state. Preserve document
   unload, resource retirement and backend lifetime rules.
5. Measure rebuild latency during editing, not just warmed playback. Bound cache
   memory and release it on unload; do not invent persistent cache formats or new
   public API versions for unreleased implementation details.

### 5. Consider temporary runtime baking only as a separate experiment

1. Proceed only if phases 2–4 leave a material, measured problem that temporary
   combined geometry could solve.
2. Generate a bounded playback cache during loading or ahead of playback while
   retaining the source hierarchy. Preserve transforms, bounds, timing, opacity,
   reflections, brush semantics and edit identity.
3. Measure preparation latency, peak/steady memory, cache misses and edit rebuild
   cost. Do not precompute an entire animation without a measured need and budget.
4. Treat this as a distinct candidate: its geometry duplication and preparation
   tradeoffs differ from batching. It must not silently alter saved IMM content.
5. Reject it if simpler submission changes provide comparable benefit or if the
   memory/editing costs outweigh playback savings.

## Experiment and acceptance gates

1. Before each candidate, record its hypothesis, baseline, plausible upper bound,
   primary limiting metric, experiment budget, regressions and stop condition.
   Investigate one change at a time. Use the discipline in
   [the performance investigation procedure](code/projects/web/performance/INVESTIGATION_PROCEDURE.md),
   adapting host-specific steps to the runtime under test.
2. Run one diagnostic comparison first. Continue a performance prototype only
   when it plausibly improves the limiting end-to-end metric by roughly 25% or
   removes a reproducible disruptive stall. Report smaller CPU-headroom benefits
   separately; a correctness fix does not need to meet that performance threshold.
3. For promising candidates, warm a complete representative segment, measure at
   least two complete passes, and use at least three alternating pairs. Preserve
   all results, report paired changes and variability, and check profiler overhead.
4. Match visible content and authored timing at fixed sample times. Include the
   existing scene and one focused case for any newly affected semantic constraint;
   avoid a large synthetic test framework that merely repeats the implementation.
5. Required behavior includes animation/held frames, hierarchy transforms,
   visibility and opacity, reflections, transparency ordering, culling, playback
   controls, live edits and clean unload. Audio behavior remains unchanged when
   the optimization does not concern audio.
6. Report CPU evaluation/submission, GPU execution, visible frame intervals,
   loading/preparation and memory as separate measurements. Name memory counters
   precisely; use a valid external/process or allocator measurement when the
   in-Editor counter is unavailable. Sequential retained allocations do not prove
   per-variant memory consumption.
7. Reconfirm accepted native changes through the existing supported host/backend
   gates, including Unity and standalone consumption. Run full CI validation for
   implementation milestones; a queued run is not passing evidence. Fix regressions
   before extending the optimization and do not weaken existing gates.
8. Record rejected candidates and why they stopped. Revisit them only with new
   evidence or a changed target. Do not enable a cache or batching path simply
   because fewer layers or draw calls looks cleaner.

## Delivery and progress

1. Initial diagnostic: complete, with the limitations above. Equivalence and the
   cause of the D3D12 gap remain unresolved.
2. Next action: establish a controlled equivalent workload and attribute the
   original-layer slowdown. No batching, cache or runtime baking implementation
   has started.
3. Keep each diagnostic correction, renderer fix and accepted optimization in a
   focused atomic commit. Update this plan and the main content plan at milestones.
4. Preserve the managed Open Brush API unless a demonstrated requirement warrants
   a discussed change. Do not introduce API/file-format versioning on speculation.
5. Completion means measured, regression-free benefit on declared targets with
   editing semantics intact, documented fallback and resource costs. It does not
   require eliminating artist baking for creative effects or export workflows.

## Artist workflow references

1. [Quill timeline guide](https://quill.art/tutorials_timeline.html): group
   flattening, drawing frames and the storage advantage of transform animation.
2. [Dan Franke's Quill production tips](https://www.creativebloq.com/how-to/get-started-in-quill):
   merging to reduce draw calls, historical Quest constraints and the warning
   against merging static content into animated layers.
3. [Quill export guide](https://quill.art/developers_export_projects.html): the
   export-dialog Bake Transforms option. This is distinct from the flattened
   paint layers already saved in Snoopy's source project.
4. [Dark Tide production case study](https://www.opastpublishers.com/open-access-articles/challenges-and-opportunities-of-virtual-reality-painting-animation-in-mainland-china-a-case-study-of-dark-tide.pdf):
   an example of baking animation for interchange. None of these references proves
   Snoopy's artist's intention or a performance result in our runtime.
