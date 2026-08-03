#!/usr/bin/env python3
"""Guard the user-facing Godot Run and Unity Play validation contracts."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def method_body(source: str, signature: str) -> str:
    start = source.index(signature)
    next_method = source.find("\n        public static void ", start + len(signature))
    return source[start:] if next_method < 0 else source[start:next_method]


def main() -> int:
    godot_project = (ROOT / "code/ImmGodotSampleProject/project.godot").read_text(encoding="utf-8")
    godot_scene = (ROOT / "code/ImmGodotSampleProject/scenes/SampleScene.tscn").read_text(encoding="utf-8")
    godot_controller = (ROOT / "code/ImmGodotSampleProject/scripts/sample_scene_controller.gd").read_text(encoding="utf-8")
    godot_helper = (ROOT / "code/projects/windows/run-godot-sample-play-smoke.ps1").read_text(encoding="utf-8")
    gpu_workflow = (ROOT / ".github/workflows/ci-gpu.yml").read_text(encoding="utf-8")

    assert 'run/main_scene="uid://dxrq2se1fvtxw"' in godot_project
    assert "load_on_ready = true" not in godot_scene
    warmup = godot_controller.index("for _frame in range(3):")
    load = godot_controller.index("viewer.load_document()", warmup)
    assert warmup < load
    for token in [
        "IMM_GODOT_SAMPLE_PLAY_SMOKE",
        "IMM_GODOT_SAMPLE_PLAY_CAPTURE",
        'SAMPLE_PLAY_SMOKE_PREFIX := "[IMM_GODOT_SAMPLE_PLAY_20260803]"',
        '"%s passed capture=%s layers=%d camera_ids=%s"',
        "get_viewport().get_texture().get_image()",
        "save_png(capture_path)",
    ]:
        assert token in godot_controller
    godot_args = godot_helper.split("$godotArgs = @(", 1)[1].split(")", 1)[0]
    assert "--scene" not in godot_args
    assert "--script" not in godot_args
    for token in [
        "Run Godot project Run-button smoke",
        "Record Godot project Run-button render metrics",
        "Verify Godot project Run-button log contract",
        "Enforce Godot project Run-button contract",
    ]:
        assert token in gpu_workflow

    unity_automation = (ROOT / "code/ImmUnitySampleProject/Assets/Editor/BuildAutomation.cs").read_text(encoding="utf-8")
    engine_workflow = (ROOT / ".github/workflows/ci-engine.yml").read_text(encoding="utf-8")
    assert '"Assets/Scenes/SampleScene.unity"' in unity_automation
    combined = method_body(
        unity_automation,
        "public static void BuildMacOSMetalSmokePlayerAndRunEditorPlayModeSmoke()",
    )
    assert combined.index("BuildMacOSMetalSmokePlayer();") < combined.index("RunMacOSEditorPlayModeSmoke();")
    editor_play = method_body(unity_automation, "public static void RunMacOSEditorPlayModeSmoke()")
    assert 'RunEditorPlayModeSmoke("macOS", SmokeScenes[0]' in editor_play
    for token in [
        "BuildMacOSMetalSmokePlayerAndRunEditorPlayModeSmoke",
        "Record Unity project Play-button render metrics",
        "Verify Unity project Play-button log contract",
        "Enforce Unity project Play-button contract",
    ]:
        assert token in engine_workflow

    print("Sample project entrypoint contracts passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
