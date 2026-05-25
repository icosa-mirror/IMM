# appImmGodotGDExtension

This folder contains the Godot 4 GDExtension-side integration scaffold for `ImmGodotPlugin`.

## Current state

- The source files define the intended `ImmViewerNode` native class and registration flow.
- The code is not yet buildable in this repository because `godot-cpp` is not vendored.
- The sample project includes a `.gdextension` manifest that points at the future binary location.

## Expected dependency

Add `godot-cpp` and configure the build so these headers resolve:

- `godot_cpp/classes/*`
- `godot_cpp/core/*`
- `godot_cpp/variant/*`

## Intended runtime shape

- `ImmViewerNode` owns native IMM session lifecycle.
- It exposes load/unload/playback/spawn-area APIs to Godot scripts.
- It is the handoff point for render-thread camera capture and draw callbacks in Phase 2.
