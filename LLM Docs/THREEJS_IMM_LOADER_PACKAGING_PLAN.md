# Three.js IMM Loader Packaging Plan

**Status:** Proposed implementation plan  
**Date:** 2026-07-14  
**Primary constraint:** Add a reusable Three.js package without changing or
destabilizing the existing standalone player and GitHub Pages deployment.

## 1. Objective

Package the existing browser-native IMM implementation behind an API that feels
consistent with established Three.js loaders while preserving the runtime
features that do not fit a conventional static asset loader:

- Wasm and Web Worker decoding;
- staged, native-order asset loading;
- paint, picture, model-placeholder, and audio resources;
- deterministic playback, authored waits, chapters, and viewpoints;
- per-frame camera-dependent rendering behavior;
- explicit GPU and audio disposal;
- integration into a host-owned Three.js renderer, scene, camera, canvas, render
  loop, and WebXR session.

The target is an ESM package tentatively referred to as `@imm/three-loader`.
The final package name is a publishing decision and does not need to be fixed
while the API is being extracted.

The standalone player at `code/projects/web/app/index.html` remains a supported
product and the reference implementation. The loader is an additional consumer
of the same decoder, runtime, audio, and Three.js view code. It must not replace
the Pages entry point, change its URLs, or share its build output directory
during the initial packaging work.

## 2. Executive design

Use two related public objects rather than forcing the complete IMM runtime into
the semantics of a static `ObjectLoader`:

1. `IMMLoader extends THREE.Loader`
   - Resolves and fetches a URL, `File`, `Blob`, `ArrayBuffer`, or typed-array
     source.
   - Runs the IMM decoder worker and reports loading progress through a
     `THREE.LoadingManager`.
   - Produces a ready-to-integrate `IMMAsset`.
2. `IMMAsset`
   - Owns a Three.js `Group`, playback controller, staged-loading session,
     optional Web Audio integration, and every resource created for the IMM.
   - Exposes explicit `update`, transport, chapter, viewpoint, and `dispose`
     methods.
   - Does not own the host renderer, scene, camera, animation loop, canvas, or XR
     session.

The intended use is:

```ts
import * as THREE from "three";
import { IMMLoader } from "@imm/three-loader";

const loader = new IMMLoader()
    .setRenderer(renderer)
    .setDecoderPath("/imm-decoder/");

const imm = await loader.loadAsync("/scenes/example.imm");
scene.add(imm.scene);

renderer.setAnimationLoop((time) => {
    imm.update(time, camera);
    renderer.render(scene, camera);
});

// Later:
scene.remove(imm.scene);
await imm.dispose();
```

This matches the broad shape of `GLTFLoader`: the loader returns a structured
result containing a scene rather than returning only a bare `Object3D`. It also
makes lifecycle responsibilities visible instead of hiding workers, audio
contexts, and GPU allocations inside an ordinary group.

## 3. Existing boundaries to retain

The current code already has most of the required lower-level boundaries:

| Component | Current responsibility | Packaging treatment |
|---|---|---|
| `ImmDecoderClient` | Module worker messaging and decoded document/delta transfer | Retain; make worker URL resolution injectable and package-safe |
| `ImmThreeView` | Three.js scene subtree, materials, geometry, pictures, updates, compatibility diagnostics, disposal | Retain as the rendering core behind `IMMAsset` |
| `ImmPlaybackController` | Deterministic time, waits, loops, chapters, seek, play/pause | Retain; expose selected operations through `IMMAsset` |
| `ImmWebAudio` | Browser decoding, source scheduling, spatial audio, A/V clock and disposal | Retain as optional runtime support |
| `createNativeLoadOrder` | Native five-second buffering order | Retain; move orchestration out of `main.ts` |
| `RandomAccessSource` implementations | Bounds-checked buffer and `File` reads | Retain and extend with a URL/range implementation later |
| `main.ts` | Standalone UI plus loading, runtime, camera, audio, diagnostics, and render-loop orchestration | Split reusable orchestration from UI without changing visible behavior |
| `embed.ts` | Host-owned scene/renderer integration fixture | Evolve into the first real consumer test for the public loader API |

The most important extraction is not geometry or decoding. It is the document
session orchestration currently embedded in `main.ts`: load cancellation,
metadata decode, the initial five-second buffer, eager fallback, background
deltas, view refresh, audio refresh, playback state, and complete cleanup.

## 4. Proposed public API

### 4.1 `IMMLoader`

```ts
export class IMMLoader extends THREE.Loader<IMMAsset> {
    constructor(manager?: THREE.LoadingManager);

    setRenderer(renderer: THREE.WebGLRenderer): this;
    setDecoderPath(path: string): this;
    setDecoderWorkerURL(url: string | URL): this;
    setWasmURL(url: string | URL): this;
    setAudio(options: IMMAudioLoadOptions | false): this;
    setInitialBufferSeconds(seconds: number): this;
    setStagedLoading(enabled: boolean): this;

    load(
        url: string,
        onLoad: (asset: IMMAsset) => void,
        onProgress?: (event: ProgressEvent) => void,
        onError?: (error: unknown) => void,
    ): void;

    loadAsync(url: string, onProgress?: (event: ProgressEvent) => void): Promise<IMMAsset>;
    loadFile(file: File, options?: IMMLoadOptions): Promise<IMMAsset>;
    parseAsync(data: ArrayBuffer | ArrayBufferView, options?: IMMLoadOptions): Promise<IMMAsset>;
}
```

Required behavior:

- Honor inherited `path`, `resourcePath`, `requestHeader`, `withCredentials`,
  and `manager` conventions wherever they are meaningful.
- Call `LoadingManager.itemStart`, `itemEnd`, and `itemError` exactly once for
  each top-level load.
- Treat a missing renderer as an explicit degraded integration mode. Geometry
  can still be constructed, but diagnostics must report that sample count and
  host depth compatibility were not established. The recommended path is
  `setRenderer(renderer)` before loading.
- Never create a `WebGLRenderer`, canvas, camera, scene, animation loop, or XR
  session.
- Support multiple simultaneous loaded assets. Decoder state must not be shared
  in a way that makes one load, cancellation, or disposal corrupt another.
- Make cancellation available through an additional `loadWithOptions` or task
  API if the inherited Three.js signatures cannot carry an `AbortSignal`
  cleanly. Do not change the familiar `load` signature solely to add options.

### 4.2 `IMMAsset`

```ts
export interface IMMAsset {
    readonly scene: THREE.Group;
    readonly document: Readonly<IMMDocumentMetadata>;
    readonly chapters: readonly IMMChapter[];
    readonly viewpoints: readonly IMMViewpoint[];
    readonly diagnostics: IMMAssetDiagnostics;
    readonly playing: boolean;
    readonly waiting: boolean;
    readonly timeSeconds: number;
    readonly durationSeconds: number;

    play(): void;
    pause(): void;
    continue(): void;
    restart(): void;
    seek(seconds: number): void;
    selectChapter(index: number): void;
    selectViewpoint(id: number): IMMViewpointPose;
    update(animationTimeMs: number, camera: THREE.Camera): void;
    setMuted(muted: boolean): void;
    enableAudio(): Promise<void>;
    dispose(): Promise<void>;
}
```

API rules:

- `scene` is the only object the host adds to its hierarchy.
- `update` advances playback, evaluates one authoritative snapshot, applies it
  to the view and audio, and performs camera-dependent picture/brush behavior.
- The initial call establishes the animation timestamp; it must not jump by the
  time elapsed since page navigation.
- A host that wants externally controlled time can use a separate
  `setTime(seconds, camera)` method. Do not overload `update` with ambiguous
  absolute-versus-delta behavior.
- Authored waits remain observable through `waiting` and require `continue()`.
- Chapter selection preserves the current native-parity behavior, including
  authored viewpoint changes. The library reports the resulting pose; camera
  mutation follows the camera policy described below.
- `dispose` is idempotent and releases worker sessions, geometries, materials,
  textures, decoded audio buffers, active sources, event listeners, and any
  internally owned audio context.
- Methods called after disposal throw a consistent `IMMDisposedError`, except a
  repeated `dispose`, which resolves without work.

### 4.3 Camera and viewpoint policy

A reusable loader must not unexpectedly take over a host camera. Support an
explicit policy:

```ts
type IMMCameraPolicy =
    | "report-only"
    | "apply-authored-viewpoints";
```

- Default library policy: `report-only`. Emit or return the authored viewpoint
  pose and let the host apply it.
- Standalone player policy: `apply-authored-viewpoints`, preserving current
  behavior.
- Gallery Viewer policy: `apply-authored-viewpoints`. Its IMM adapter must apply
  the initial authored camera, explicit viewpoint selections, and any camera
  transform selected as part of a chapter change. Authored IMM cameras are a
  required Gallery feature, not an optional host-side example.
- The desktop floor-to-eye height correction remains available as a helper and
  is applied only for non-XR camera poses.
- In XR, the host owns the reference space and rig. The asset may provide a
  suggested spawn transform but must not enter/exit XR or replace the host rig.

For Gallery Viewer specifically, desktop poses update its flat camera and the
associated controls target atomically so the controls do not snap back on the
next frame. In XR, the authored pose moves/orients the Gallery-owned camera rig
while head tracking remains relative to that rig. Chapter selection must resolve
the chapter's camera/viewpoint before applying the pose; it must not independently
choose a preferred viewpoint afterward and overwrite the authored chapter view.

### 4.4 Audio ownership

Audio behavior needs an explicit host contract:

```ts
interface IMMAudioLoadOptions {
    enabled?: boolean;
    context?: AudioContext;
    destination?: AudioNode;
    closeOwnedContextOnDispose?: boolean;
}
```

- A supplied `AudioContext` is never closed by the library.
- An internally created context is closed during disposal unless configured
  otherwise.
- Autoplay rejection is reported through asset state/events; the library never
  installs UI.
- `enableAudio()` is the user-gesture entry point.
- The standalone player explicitly selects its existing audio-on-load behavior.
- The initial library default should be audio disabled until the API is tested
  in real host applications. This avoids creating unexpected audio contexts
  merely by loading an object. This default is an API decision to confirm before
  the first public release.

### 4.5 Events

Use `THREE.EventDispatcher` or a typed equivalent for state that a host UI must
observe:

- `progress`: aggregate and staged asset progress;
- `ready`: initial native five-second buffer is renderable;
- `complete`: all staged assets have loaded;
- `play`, `pause`, `wait`, `continue`, `ended`;
- `chapterchange`, `viewpointchange`;
- `audioblocked`, `audioerror`;
- `warning`: renderer/depth/codec/capability warning;
- `error`: background staged-loading failure;
- `dispose`.

Events should carry stable structured data. Do not expose strings scraped from
the standalone status text as an API.

## 5. Renderer and WebXR contract

The package renders into the host's renderer and depth buffer. It must document
and diagnose, but not silently rewrite, the following:

- perspective camera with native-compatible near/far behavior;
- depth and stencil availability;
- no logarithmic or reversed depth unless explicitly supported;
- sRGB output and no tone mapping for native color parity;
- multisampling for alpha-to-coverage or the documented alpha-hash fallback;
- transparency limitations and the future order-independent transparency work;
- WebXR framebuffer quality must be configured before session entry.

Provide an opt-in helper rather than automatic global mutation:

```ts
configureIMMRenderer(renderer, {
    outputColorSpace: true,
    toneMapping: true,
    xrQuality: "normal" | "high",
});
```

The helper's XR quality mapping initially matches the standalone player:

- Normal: runtime-recommended framebuffer scale `1.0`;
- High: framebuffer scale `1.5` per dimension;
- antialiasing remains enabled, with the current Three.js projection-layer path
  using four samples when supported.

The helper must refuse or defer framebuffer scale changes while an XR session
is presenting. An `IMMAsset` does not own this setting because multiple assets
can share one renderer and XR session.

## 6. Decoder and asset URL packaging

### 6.1 Problem

The Pages application currently resolves decoder assets with Vite-specific
`import.meta.env.BASE_URL` and a release ID. That is correct for the deployed
site but is not a portable library default. Consumers may install the package
under arbitrary application bases, CDN paths, bundlers, or asset pipelines.

### 6.2 Package layout

The assembled package should contain:

```text
package/
  package.json
  README.md
  LICENSE
  dist/
    index.js
    index.d.ts
    decoder/
      imm-web-decoder-worker.mjs
      imm-web-decoder.wasm
      [other generated decoder support files]
```

Use ESM only for the first release. This matches Three.js and module workers and
avoids maintaining a CommonJS worker/Wasm path. Emit TypeScript declarations and
source maps. Declare `three` as a peer dependency so a host cannot accidentally
load two incompatible Three.js instances.

### 6.3 URL resolution order

1. Explicit `setDecoderWorkerURL` and `setWasmURL` values.
2. Explicit `setDecoderPath`, resolving known filenames beneath it.
3. Package-relative defaults based on `import.meta.url` when the consumer's
   bundler preserves or copies package assets correctly.

The worker receives the Wasm URL explicitly. It must not assume that its own URL,
the JavaScript entry, and the Wasm binary are served from the same directory.

Document these hosting requirements:

- module workers and Wasm need valid MIME types;
- cross-origin decoder assets require appropriate CORS headers;
- Content Security Policy must permit the worker and Wasm source;
- no cross-origin isolation or Wasm threads are required initially;
- hashed consumer asset filenames are supported when explicit URLs are passed.

Do not use Blob worker construction as the default. It complicates CSP, source
maps, relative Wasm resolution, and debugging.

## 7. Loading, progress, cancellation, and concurrency

### 7.1 First packaged version

Match current production behavior before adding new network complexity:

- URL loads perform a full fetch.
- Metadata and initial assets are decoded in native order after bytes arrive.
- Playback becomes ready after the native five-second window.
- Remaining assets decode in the background.
- If staged decode fails, use the existing eager fallback.

This is not true network streaming, but it isolates packaging risk from range-I/O
risk and preserves current Pages behavior.

### 7.2 Later URL range source

Add `FetchRandomAccessSource` only after measuring large-file behavior:

- probe `Content-Length` and byte-range support;
- use exact `Range` requests when supported;
- validate `206`, `Content-Range`, returned length, and stable entity identity;
- fall back to one full download for servers without ranges;
- avoid duplicate full downloads when a server ignores `Range`;
- expose downloaded bytes separately from decoded/ready asset counts;
- retain credentials, headers, and abort signals.

### 7.3 Cancellation

Cancellation must cover:

- fetch or file reads;
- pending worker requests;
- initial buffering;
- background staged decode;
- image/audio decode callbacks;
- view construction and refresh;
- replacement loads.

Late results from an aborted load must not mutate an active asset. Use a session
identity/generation check in addition to `AbortSignal`, matching the clean-state
lessons from the standalone reload path.

### 7.4 Multiple assets

The current worker holds one staged document at a time. The first public design
should therefore allocate one decoder client/session per active `IMMAsset` that
is still staging. After staging completes, terminate or release that worker.
Pooling can be considered later only with evidence that worker startup is a
material cost. Correct isolation is more important than speculative reuse.

## 8. Package/build structure

### 8.1 Initial repository layout

Keep the loader source in the existing web project while boundaries stabilize:

```text
code/projects/web/app/
  src/
    library/
      imm-loader.ts
      imm-asset.ts
      imm-load-session.ts
      renderer-contract.ts
      public-types.ts
      index.ts
    [existing decoder/runtime/render/audio modules]
  vite.config.mjs                 # existing Pages/app build
  vite.library.config.mjs         # new isolated library build
  dist/                           # existing app output only
  dist-library/                   # new library output only
```

Do not create a workspace/package split until the imports and public API are
stable. An early physical move would create large noisy diffs and increase the
risk of breaking the application build without improving the consumer API.

### 8.2 Separate commands

- `bun run build` remains the existing Pages application build.
- Add `bun run build:library` for `dist-library/`.
- Add `bun run test:library` for unit and package-consumer checks.
- Add `bun run pack:library` only when package assembly is ready.
- Do not make `build` depend on `build:library` initially.
- Do not make Pages upload `dist-library/`.

Once extraction is stable, a later change may make the standalone player import
the library public entry point. That is a final convergence step, not an initial
requirement.

### 8.3 Package metadata

The eventual publishable manifest should include:

- `type: "module"`;
- an `exports` map for the JavaScript and declaration entry;
- decoder assets included through `files`;
- `three` in `peerDependencies`, with the tested range stated;
- no app HTML, CSS, sample IMM, browser-test artifacts, or Pages cache bootstrap;
- `sideEffects: false` only after verifying that generated asset imports and
  worker setup are not tree-shaken incorrectly.

## 9. GitHub Pages non-regression strategy

Avoiding a Pages regression is a release requirement, not a best-effort check.

### 9.1 Protected invariants

The following behavior must remain unchanged until an explicitly reviewed
migration phase:

- `.github/workflows/web-pages.yml` runs `bun run build` in
  `code/projects/web/app`.
- `vite.config.mjs` owns the application HTML inputs.
- application output remains `code/projects/web/app/dist`.
- Pages uploads only that `dist` directory.
- the default `sample1.imm` remains copied under the release-scoped fixture path;
- decoder worker and Wasm remain copied under the release-scoped decoder path;
- `version.json`, the release meta tag, and the cache-busting HTML bootstrap
  remain present;
- `IMM_WEB_BASE_PATH` and `VITE_IMM_RELEASE_ID` keep their current meanings;
- `index.html`, `embed.html`, and `phase3-fixture.html` remain build inputs;
- current standalone, embedded, Chrome, Firefox, and bundle-layout tests remain
  required.

### 9.2 Isolation rules

- The library config must never write to `dist/`.
- The app build must not clean or depend on `dist-library/`.
- Library tests must use a separate port and output/artifact directory.
- Package-relative decoder URL work must not replace `releaseAssetUrl` in the
  Pages app until both paths have equivalent nested-base and release-ID tests.
- Do not rename generated decoder files during extraction.
- Do not remove the sample or change its legal upload status.
- Do not add private local IMM fixtures to the package, repository, CI artifacts,
  or hosted test routes. They remain local-only test inputs.

### 9.3 CI additions

Add library checks alongside, not inside, the Pages artifact assembly:

1. Build the Wasm decoder once.
2. Run existing unit and browser smoke tests.
3. Build and verify the existing Pages bundle exactly as today.
4. Separately build `dist-library/`.
5. Assemble a package tarball in a temporary directory.
6. Install it into a minimal consumer fixture.
7. Run the consumer fixture against a nested base path.
8. Upload only `dist/` to Pages.

During initial work the library check can be a step in the existing build job,
provided it cannot modify `dist/`. If its runtime becomes significant, move it
to an independent job. Pages deployment should continue to depend only on the
existing app build artifact; experimental package publication must never be a
prerequisite for deployment.

### 9.4 Per-commit gate

For every extraction commit:

```text
bun test
bun run build
bun run test:browser:smoke
bun run build:library             # once introduced
bun run test:library              # once introduced
```

At phase boundaries also run the extended Chrome and Firefox suites and verify
the production Pages bundle layout with a non-root base path and release ID.

## 10. Implementation phases

### Phase 0 — freeze contracts and baseline Pages

Work:

- Record the current standalone and embed public behavior.
- Add a machine-readable Pages bundle manifest test covering HTML entries,
  release fixture, decoder worker/Wasm, `version.json`, and cache bootstrap.
- Add lifecycle tests for clean load, failed load, replacement load, and dispose.
- Define the public TypeScript interfaces without exporting them from a package.
- Decide the package name, supported Three.js version range, and initial audio
  default.

Exit criteria:

- The current Pages bundle is reproducibly characterized.
- No runtime behavior has moved.
- The proposed API can represent chapters, viewpoints, waits, audio-blocked
  state, progress, and disposal without exposing UI elements.

Risk: low.

### Phase 1 — extract `IMMLoadSession`

Work:

- Move load generation, cancellation, metadata decode, native five-second
  buffering, eager fallback, background work, delta application, and release
  into a UI-independent class.
- Inject status/progress callbacks rather than writing DOM text.
- Give each session explicit decoder ownership.
- Preserve the existing main-thread upload sequence initially.
- Make both `main.ts` and tests consume the extracted session internally.

Exit criteria:

- Standalone output and controls are unchanged.
- Replacement and failure loads always start from a clean state.
- Session tests cover cancellation at each stage.
- Pages and extended browser workflows pass.

Risk: moderate. This is the largest behavioral extraction.

### Phase 2 — implement `IMMAsset`

Work:

- Compose `ImmThreeView`, `ImmPlaybackController`, optional `ImmWebAudio`, and
  `IMMLoadSession` behind one lifecycle object.
- Move the one-snapshot-per-frame update path out of `main.ts`.
- Add event/state reporting.
- Implement idempotent disposal and post-disposal errors.
- Separate authored viewpoint resolution from direct camera mutation.
- Keep the standalone camera policy adapter so its behavior does not change.

Exit criteria:

- `main.ts` is primarily DOM, standalone camera controls, renderer setup, and
  mapping UI actions to `IMMAsset`.
- `embed.ts` can drive the asset without importing decoder/view internals.
- Audio timing, waits, chapters, and viewpoints retain native-parity tests.

Risk: moderate.

### Phase 3 — implement the Three.js loader facade

Work:

- Add `IMMLoader extends THREE.Loader`.
- Support URL, `File`, and in-memory sources.
- Integrate `LoadingManager` and standard callbacks.
- Add explicit renderer, decoder URL, staged loading, audio, and buffer settings.
- Add renderer compatibility warnings and `configureIMMRenderer`.
- Convert `embed.ts` to the public API as the first integration fixture.

Exit criteria:

- The usage example in this document works without importing private modules.
- Two assets can load, play, and dispose independently in one scene.
- A host object intersects IMM geometry correctly in the shared depth buffer.
- The loader does not create or own global rendering objects.

Risk: low to moderate after Phases 1 and 2.

### Phase 4 — isolated library build and assets

Work:

- Add `vite.library.config.mjs` and `dist-library/`.
- Make `three` external and a peer dependency.
- Emit ESM, declarations, and source maps.
- Assemble worker/Wasm assets without using the Pages release layout.
- Implement explicit and package-relative decoder URL resolution.
- Add an `npm pack`/tarball verification script.

Exit criteria:

- No bundled second copy of Three.js.
- A clean consumer can install the tarball and load `sample1.imm`.
- Worker/Wasm loading succeeds at root and nested URL bases.
- `bun run build` produces a byte-layout-equivalent Pages structure except for
  expected hashed application bundle changes.

Risk: moderate, concentrated in asset URLs and consumer bundlers.

### Phase 5 — consumer compatibility matrix

Work:

- Add minimal Vite consumer fixture.
- Add a browser-native import-map/static-server fixture if it does not require
  duplicating build infrastructure.
- Test current Chrome, Firefox, desktop/mobile DPR, and WebXR-capable setup where
  hardware is available.
- Test CSP-friendly external module worker configuration.
- Test custom decoder CDN URLs with CORS.
- Verify tree shaking and production minification.

Exit criteria:

- The documented setup works in at least Vite and direct ESM hosting.
- Errors for missing worker, wrong Wasm MIME type, CORS failure, and incompatible
  renderer are specific and actionable.
- Existing Pages deployment remains green.

Risk: moderate.

### Phase 6 — range loading and advanced integration

Work:

- Implement `FetchRandomAccessSource` and true range-backed staged loading.
- Add download/decode/upload progress separation.
- Add host-controlled upload budgets if profiling still shows transition stalls.
- Add optional externally clocked playback.
- Add framework lifecycle examples only after the core API stabilizes.

Exit criteria:

- Large-file time-to-first-frame and peak memory improve on representative local
  fixtures without changing playback or final rendering.
- Full-download fallback is reliable.
- Cancellation never applies late deltas to another asset.

Risk: high. This phase changes I/O semantics and must not be bundled into the
initial packaging work.

### Phase 7 — convergence and optional publication

Work:

- Decide whether the standalone app should import the public package entry point
  or retain source-level imports within the same project.
- Run the complete parity and performance matrix.
- Write package README, API reference, hosting guide, and migration notes.
- Establish semantic versioning and changelog policy.
- Publish only after the package tarball and Pages artifact are independently
  reproducible.

Exit criteria:

- There is one tested implementation of runtime behavior.
- The standalone app and external consumer exercise the same public lifecycle.
- Package publication can fail without blocking Pages deployment.

Risk: low if earlier phases preserve boundaries.

## 11. Verification matrix

### Unit tests

- source bounds, file reads, eventual HTTP range reads;
- URL and worker/Wasm resolution under nested bases;
- `LoadingManager` call balance on success, failure, abort, and dispose;
- native loading order and initial buffer threshold;
- session generation protection against late results;
- transport, waits, loops, chapters, viewpoint selection, and restarts;
- camera policy behavior for desktop and XR;
- audio ownership and context-close rules;
- disposal idempotence and post-disposal behavior.

### Browser integration tests

- load `sample1.imm` from URL, file, and `ArrayBuffer`;
- render nonzero strokes/triangles and compare existing visual contracts;
- host and IMM objects share one canvas and depth buffer;
- two independent IMM assets coexist;
- load failure followed by success has no stale state;
- repeated load/dispose cycles do not grow Three.js geometry/texture counts;
- standard callbacks and LoadingManager events fire correctly;
- nested base path resolves worker and Wasm;
- audio autoplay fallback and explicit enable;
- chapter change reports and applies the expected viewpoint policy;
- Normal/High renderer helper behavior before XR entry;
- mobile layout is irrelevant to the library and remains owned by the standalone
  Pages app.

### Package tests

- tarball contains only declared runtime files and legal public fixture assets;
- package contains no private local paths or IMM files;
- `three` is external;
- declaration imports resolve in a clean TypeScript consumer;
- production bundling does not inline an invalid worker URL;
- source maps do not embed private filesystem paths;
- package can be served from a subdirectory and a separate decoder asset origin.

### Existing regression tests retained

- Wasm decoder CTest suite;
- 29+ web unit tests;
- standalone Chrome smoke;
- extended Chrome visual/audio/lifecycle suite;
- Firefox suite;
- Pages cache-versioning verification;
- release-scoped decoder and sample bundle checks;
- native render/parity comparisons where applicable.

## 12. Performance considerations

Packaging must not introduce a second evaluation or rendering layer.

- `IMMAsset.update` reuses one playback snapshot for view, viewpoint, and audio.
- The loader facade adds no work per frame after loading.
- Avoid copying decoded geometry merely to present a public API.
- Transfer source buffers to the worker where ownership allows it.
- Keep staged asset upload behavior measurable and compatible with the existing
  performance optimization plan.
- Do not pool workers, cache documents, or add resource registries until real
  multi-asset usage shows that startup or duplication is material.
- Benchmark the package consumer against the standalone player at identical CSS
  size, render scale, chapter, timestamp, and resident asset count.

Performance acceptance:

- packaged integration stays within measurement noise of the standalone
  adapter for steady-state frame time;
- time-to-ready does not regress by more than 5% on `sample1.imm` absent a known
  measurement variance;
- no extra Three.js copy appears in the consumer bundle;
- disposal returns geometry/texture counts to the pre-load baseline.

## 13. Error model

Export typed errors with causal details:

- `IMMFetchError` — HTTP status, URL, and range context;
- `IMMDecoderAssetError` — worker/Wasm URL or initialization failure;
- `IMMFormatError` — decoder status and byte offset;
- `IMMCapabilityError` — required browser feature unavailable;
- `IMMRendererCompatibilityError` — strict host contract failure;
- `IMMAbortError` — explicit cancellation;
- `IMMDisposedError` — invalid operation after disposal.

Background staged-load errors should not necessarily destroy already playable
content. Mark the asset incomplete, emit an error with the affected layer/work
item, and allow the host to choose whether to continue or dispose.

## 14. Documentation deliverables

- five-minute Vite setup;
- direct ESM/static hosting setup;
- decoder worker/Wasm hosting and CORS guide;
- host renderer and transparency contract;
- render-loop and external-clock examples;
- chapters and viewpoint integration;
- audio/autoplay ownership;
- WebXR ownership and quality configuration;
- progress, cancellation, replacement load, and disposal;
- large-file/range-server requirements when Phase 6 lands;
- differences between the standalone player and library defaults.

## 15. Decisions required before public release

1. Final npm package name and publishing scope.
2. Supported `three` peer dependency range.
3. Whether library audio defaults to disabled or creates an owned context.
4. Whether missing renderer is allowed with warnings or rejected in strict mode.
5. Whether the initial public release promises URL range loading or documents
   full-download behavior.
6. Whether `IMMAsset` exposes raw decoded document structures or only stable
   metadata and queries. Prefer the latter until the format model stabilizes.
7. Whether camera policy is fixed per asset or overridable per chapter/viewpoint
   operation.

None of these decisions requires changing the current Pages deployment.

## 16. Recommended first implementation slice

Begin with Phase 0 and the smallest part of Phase 1:

1. Strengthen the Pages bundle manifest test.
2. Define internal `IMMLoadSession` callbacks and state without moving code.
3. Extract replacement-load cancellation and decoder release behavior.
4. Make `main.ts` consume that behavior with no public API or build changes.
5. Run the full Pages workflow before extracting background staged loading.

This sequence exercises the highest-risk lifecycle boundary while leaving Vite,
release asset paths, HTML inputs, cache busting, and deployment artifact assembly
untouched.

## 17. Gallery Viewer integration findings

The initial target consumer is Gallery Viewer. Its current architecture adds
constraints that the package API and consumer fixture must cover:

- Gallery owns one `WebGLRenderer`, render loop, canvas, desktop camera, XR
  session, and XR camera rig. `IMMAsset` must plug into those objects and never
  create competing global rendering state.
- Gallery replaces content by clearing scene children. Renderer services that
  must survive content replacement belong under a persistent services root;
  loaded models and assets belong under a separate content root.
- Gallery uses a shared `THREE.LoadingManager`. The IMM adapter must balance its
  item lifecycle and forward staged progress without independently manipulating
  the loading-screen DOM.
- Gallery currently supports an optional Spark dependency. Spark 2.1 requires
  Three.js 0.180 or newer and an explicit persistent `SparkRenderer` added to the
  scene. This is a useful precedent for IMM: a static-looking scene object can
  still require a host-owned, persistent renderer service and explicit disposal.
- Spark's `SplatMesh` accepts URL, bytes, and stream sources, exposes progress
  during download/initialization, and owns an `initialized` promise and
  `dispose()` method. The IMM API should preserve the same useful flexibility,
  while returning an `IMMAsset` because playback, chapters, viewpoints, audio,
  and staged decode exceed a mesh's lifecycle.
- Spark recommends creating its renderer with antialiasing disabled for splat
  performance. IMM relies on multisampling/alpha-to-coverage for native brush
  parity. Gallery must keep a stable shared renderer configuration while mixed
  content is present; it must not recreate or silently reconfigure the renderer
  when switching between Spark and IMM content. Any quality compromise must be
  an explicit Gallery setting and benchmark.
- Gallery's existing camera far plane is shorter than the standalone IMM
  player's native-parity range. The IMM adapter must apply or validate the IMM
  camera projection requirements when authored cameras become active without
  permanently corrupting settings for later non-IMM content.
- Gallery must expose authored IMM cameras in its viewer UI. Initial load,
  explicit viewpoint selection, and chapter-associated viewpoint changes must
  all use the same pose-resolution path and update desktop controls or the XR rig
  as described in section 4.3.

The companion Gallery implementation should therefore proceed in two layers:

1. Upgrade and stabilize Gallery's persistent service/content lifecycle without
   changing its deployed `dist/` artifact.
2. Add an IMM adapter after `IMMAsset` exists. The adapter adds `asset.scene` to
   the content root, calls `asset.update` once per Gallery render frame, maps
   loading/progress events, applies authored camera results, and calls
   `asset.dispose()` during replacement or viewer teardown.

Gallery's package currently externalizes Three.js, Spark, three-icosa, and
three-tiltloader. The consumer test must use exactly one Three.js 0.180 instance
across Gallery, Spark 2.1, and the IMM package. The existing Gallery `dist/`
output is intentionally not regenerated during this migration; deployment can
be updated in a separately reviewed release commit after source verification.
