# Godot GDExtension build review: agent handoff

Review date: 2026-10-03

## Objective and limits

Make the Godot extension build configuration reproducible and consistent with its
godot-cpp dependency, and validate the compatibility and editor behavior we claim.
Preserve the existing rendering architecture and supported platform combinations.

This handoff follows a static comparison with
[Using any C++ library in Godot](https://blog.conan.io/cpp/conan/gamedev/godot/cmake/2026/09/29/Using-Any-Cpp-Library-In-Godot.html).
It does not establish correctness of GPU synchronization, resource lifetimes, XR,
or visual composition. Do not treat the findings as explanations for unrelated
rendering failures without runtime evidence. Reinspect current files before editing:
another agent may be working on the repository and CI.

## What is already correct

1. We use GDExtension and godot-cpp with an exported C entry point, scene-level
   registration, GDCLASS declarations, and bound methods/properties.
2. The entry point in `src/register_types.cpp` matches `imm_godot_library_init` in
   the addon manifest. Debug/release library paths and runtime dependencies are
   declared in the manifest.
3. Targeting the Godot 4.5 bindings and running on a newer Godot version is a valid
   direction of compatibility. The manifest currently declares minimum version 4.5;
   inspected workflows use Godot 4.7.2 with `godot-4.5-stable` bindings.
4. The separate GDExtension wrapper and IMM native library are reasonable for a
   renderer shared by several integrations. A single shared library is not required.
   iOS uses its own static archive/XCFramework packaging and must retain that route.
5. Conan is optional. Do not migrate dependency management or upgrade to godot-cpp
   10 merely to resemble the article. Existing SCons/CMake builds can be correct.

## Priority 1: make binding and extension build settings agree

### Confirmed observations

1. `code/appImmGodotGDExtension/SConstruct` manually constructs an environment
   instead of inheriting godot-cpp's build environment. It adds `DEBUG_ENABLED`
   for debug builds, but adds `HOT_RELOAD_ENABLED` only in its macOS branch.
2. The inspected `thirdparty/godot-cpp/tools/godotcpp.py` enables hot-reload support
   by default for targets other than `template_release`, and enables debug features
   for `template_debug`. Its headers contain conditional behavior for these flags.
3. Android builds godot-cpp using the chosen `template_debug`/`template_release`
   target. However, `code/projects/android/appImmGodot/CMakeLists.txt` does not
   propagate `DEBUG_ENABLED` or the dependency's hot-reload setting to the extension.
4. The iOS workflow builds `template_debug` godot-cpp archives, while
   `ImmGodotIOSGDExtension` in `code/projects/ios/CMakeLists.txt` does not propagate
   those definitions either.
5. Android's CMake fallback searches both debug and release archives and chooses
   the first result independently of the requested variant. The normal helper
   supplies an explicit library, so this is a fallback defect, not proof that CI
   currently links the wrong variant. SCons also accepts development-library
   filename alternatives without deriving their development settings.

### Recommended implementation

1. Define one explicit dependency build contract: API revision/version, platform,
   architecture, target, precision, development mode, thread support, hot-reload
   setting, and relevant toolchain/runtime settings.
2. Derive extension definitions from that contract. Prefer reusing godot-cpp's
   supported SCons environment where practical; an imported CMake target can carry
   the matching interface definitions. Avoid maintaining divergent flag lists.
3. Keep runtime selection separate from compiler optimization. CMake `Debug` or
   `Release` alone does not establish the godot-cpp target or debug feature flags.
4. Select exactly the matching library. Fail with a useful error if the requested
   variant is missing or ambiguous; do not choose an arbitrary debug/release or
   development archive. Validate explicit library overrides too.
5. Do not enable addon hot reloading merely to fix a flag discrepancy. The manifest
   currently does not enable `reloadable`. Either preserve that behavior with
   consistent build settings, or evaluate reloading as separate lifecycle work.

These mismatches are confirmed configuration weaknesses. Their presence does not
by itself prove memory corruption or explain any existing crash.

## Priority 2: validate dependency reuse and record build provenance

1. `code/projects/windows/build-godot-extension.ps1` accepts an existing godot-cpp
   directory without checking the requested ref. It skips rebuilding when a library
   and generated headers exist. Presence alone does not prove that they match the
   requested revision, API, flags, or compiler.
2. Record the resolved commit and build contract alongside generated bindings and
   libraries. Include the settings that actually affect compatibility; avoid an
   unnecessarily elaborate cache system.
3. Reuse artifacts only when the recorded configuration matches. Rebuild managed
   outputs when safe, or fail clearly when an explicit prebuilt dependency cannot be
   verified. Do not silently reset, switch, or clean a user-supplied checkout.
4. Apply the same rule to Android and Apple paths. Inspect CI cache keys as well as
   local helper behavior. Pin immutable revisions for reproducible CI builds and
   include their identity in cache keys and evidence.
5. Report the actual resolved dependency revision and target in build output, rather
   than printing the requested ref as though it describes an existing checkout.

## Priority 3: test the declared minimum Godot version

1. Keep `compatibility_minimum="4.5"` only with evidence for that claim. The inspected
   workflows establish a newer runtime baseline, not minimum-version acceptance.
2. Add a focused Godot 4.5 check using the earliest patch release we intend to
   support: native extension loading, class/method/property availability, document
   loading, and a representative real rendering smoke. Reuse existing harnesses.
3. Retain current-version and platform CI checks. Determine whether any backend-
   specific API needs separate minimum-version coverage rather than assuming one
   desktop result proves every platform combination.
4. If a feature genuinely requires a newer Godot version, document the reason and
   reconcile the compatibility claim. Do not raise the minimum just to avoid fixing
   a build/configuration problem.

## Priority 4: decide and document editor execution

1. `src/register_types.cpp` uses ordinary class registration. In
   `src/imm_viewer_node.cpp`, `_ready()` initializes the native backend and may load
   a document; `_process()` performs native work without an editor-hint guard.
2. Ordinary extension classes can execute while editing scenes. This can be correct
   for intentional live previews, but editor Play and scene-edit previews are
   different behaviors. Establish which behavior the product intends.
3. If live previews are intended, document them and verify scene open/close,
   document replacement, Play/Stop, and native resource cleanup.
4. If previews are not intended, consider runtime-only registration or targeted
   editor-hint guards. Preserve inspector properties and serialized scenes. Assess
   the compositor resource separately rather than changing both classes blindly.
5. Do not classify the existing registration as a proven bug until that behavior
   decision is made. Hot-reload support also needs its own safe cleanup assessment.

## Validation and implementation sequence

1. Read applicable AGENTS.md instructions, inspect dirty files and concurrent work,
   and establish current CI status before changing shared code or generated outputs.
2. Fix library selection and build-setting propagation in a focused increment.
   Verify debug and release configurations on affected platforms, including both
   iOS device and simulator architectures where packaged.
3. Add narrowly targeted checks for mismatched variants and stale dependency
   metadata. They should reject configurations that could previously be accepted,
   rather than merely checking for specific source text.
4. Run the existing `code/appImmGodotGDExtension/verify_local.py` and sample API
   verifier, then affected native loading/export/rendering CI. Static checks do not
   replace a real engine load or exported-player check.
5. Test an exported addon with only its staged dependencies available. Preserve
   Windows dependency declarations, macOS loader-relative paths, and Android/iOS
   packaging. A development machine's PATH must not hide missing dependencies.
6. Implement provenance/cache validation next, then minimum-version coverage.
   Handle editor behavior after its intended semantics are established.
7. Keep independent fixes in separate commits. Monitor meaningful CI milestones,
   wait a realistic part of job duration between checks, and fix regressions before
   advancing. Do not disable existing gates or call compilation alone acceptance.
8. Update build/addon documentation and record exact dependency/runtime versions,
   target variants, tested platforms, and remaining evidence gaps.

## Acceptance checklist

1. Every extension build agrees with its linked godot-cpp library's relevant settings.
2. Library selection cannot silently substitute another target or development mode.
3. Dependency reuse is supported by matching provenance, not file existence alone.
4. The declared minimum engine version has appropriate runtime evidence.
5. Editor execution and reloadability are explicit, intentional, and tested as applicable.
6. Exported artifacts resolve their declared dependencies on supported platforms.
7. Relevant existing CI passes, and evidence distinguishes import, export, API,
   rendering, and XR coverage.

## References

1. [Conan article](https://blog.conan.io/cpp/conan/gamedev/godot/cmake/2026/09/29/Using-Any-Cpp-Library-In-Godot.html).
2. [Godot GDExtension C++ workflow, version compatibility, and build targets](https://docs.godotengine.org/en/stable/tutorials/scripting/cpp/gdextension_cpp_example.html).
3. `code/appImmGodotGDExtension/README.md` and `code/ImmGodotSampleProject/addons/imm_viewer/README.md`.
4. `code/projects/windows/build-godot-extension.ps1`, `code/projects/android/build-godot-extension-android.ps1`, and `.github/workflows/build.yml`.
5. `.github/workflows/ci-ios.yml` and the addon manifests in the flat and XR sample projects.
