#!/bin/sh
set -eu

usage() {
    echo "usage: $0 [--cli-contract|--native-frame-failure-contract|--bundle-contract|--content-override-contract|--audio-contract|--interactive-audio-contract|--repeat-contract|--reload-contract] [appImmViewerMetal-path] [content-path]" >&2
    echo "" >&2
    echo "Audio contract environment:" >&2
    echo "  IMM_METAL_VALIDATE_AUDIO_EXPECTED_OPUS_DECODED  Exact distinct decoded Opus sounds expected (default: 3)" >&2
    echo "  IMM_METAL_VALIDATE_AUDIO_MIN_WAV_ADDED          Minimum unique WAV sound objects expected (default: 0)" >&2
    echo "  IMM_METAL_VALIDATE_AUDIO_MIN_TOTAL_ADDED        Minimum decoded/added sound objects expected (default: expected Opus count)" >&2
    echo "  IMM_METAL_VALIDATE_AUDIO_MIN_PLAY_CALLS         Minimum accepted AVFoundation Play calls expected (default: 1)" >&2
    echo "  IMM_METAL_VALIDATE_AUDIO_MIN_PLAYING_STATES     Minimum observed AVFoundation playing-state transitions expected (default: 1)" >&2
    echo "  IMM_METAL_VALIDATE_AUDIO_MIN_PROGRESS_MARKERS   Minimum observed AVFoundation playback-progress markers expected (default: 1)" >&2
    echo "  IMM_METAL_VALIDATE_AUDIO_PROGRESS_THRESHOLD_SEC Playback duration required for each progress marker (default: 1.0)" >&2
    echo "  IMM_METAL_VALIDATE_VOLUME_CONTROLS              Set to 1 to require standalone volume/mute smoke logs (default: 0)" >&2
    echo "  IMM_METAL_VALIDATE_PLAYBACK_CONTROLS            Set to 1 to require standalone playback-control smoke logs (default: 0)" >&2
    echo "  IMM_METAL_VALIDATE_OPEN_FAILURE_RESTORE         Set to 1 to require failed-open restore smoke logs (default: 0)" >&2
    echo "  IMM_METAL_VALIDATE_RECENT_DOCUMENTS            Set to 1 to require standalone Open Recent smoke logs (default: 0)" >&2
}

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
check_cli_contract=0
check_native_frame_failure_contract=0
check_bundle_contract=0
check_content_override_contract=0
check_audio_contract=0
check_interactive_audio_contract=0
check_repeat_contract=0
check_reload_contract=0
if [ "${1:-}" = "--cli-contract" ]; then
    check_cli_contract=1
    shift
elif [ "${1:-}" = "--native-frame-failure-contract" ]; then
    check_native_frame_failure_contract=1
    shift
elif [ "${1:-}" = "--bundle-contract" ]; then
    check_bundle_contract=1
    shift
elif [ "${1:-}" = "--content-override-contract" ]; then
    check_content_override_contract=1
    shift
elif [ "${1:-}" = "--audio-contract" ]; then
    check_audio_contract=1
    shift
elif [ "${1:-}" = "--interactive-audio-contract" ]; then
    check_interactive_audio_contract=1
    shift
elif [ "${1:-}" = "--repeat-contract" ]; then
    check_repeat_contract=1
    shift
elif [ "${1:-}" = "--reload-contract" ]; then
    check_reload_contract=1
    shift
fi

app_path=${1:-"$repo_root/build/macos/viewer/appImmViewerMetal.app/Contents/MacOS/appImmViewerMetal"}
content_path=${2:-"$repo_root/exampleImmFiles/sample1.imm"}

canonical_file_path() {
    path=$1
    dir=$(dirname -- "$path")
    base=$(basename -- "$path")
    printf '%s/%s\n' "$(CDPATH= cd -- "$dir" && pwd)" "$base"
}

if [ "${1:-}" = "--help" ] || [ "${1:-}" = "-h" ]; then
    usage
    exit 0
fi

if [ ! -x "$app_path" ]; then
    echo "appImmViewerMetal is not executable: $app_path" >&2
    exit 1
fi

if [ ! -f "$content_path" ]; then
    echo "IMM content file not found: $content_path" >&2
    exit 1
fi

app_path=$(canonical_file_path "$app_path")
content_path=$(canonical_file_path "$content_path")

capture_dir=${IMM_METAL_VALIDATE_CAPTURE_DIR:-}
capture_format=${IMM_METAL_VALIDATE_CAPTURE_FORMAT:-ppm}
case "$capture_format" in
    ppm|png) ;;
    *)
        echo "unsupported IMM_METAL_VALIDATE_CAPTURE_FORMAT: $capture_format (expected ppm or png)" >&2
        exit 1
        ;;
esac
if [ -n "$capture_dir" ]; then
    mkdir -p "$capture_dir"
    capture_dir=$(CDPATH= cd -- "$capture_dir" && pwd)
fi

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/imm-metal-validate.XXXXXX")
cleanup() {
    rm -rf "$tmp_dir"
}
trap cleanup EXIT INT TERM

validate_avfoundation_audio_teardown() {
    log_path=$1
    label=$2
    min_temp_files_removed=$3

    teardown_lines=$(grep "AVFoundation audio Deinit complete:" "$log_path" || true)
    if [ -z "$teardown_lines" ]; then
        echo "$label did not report parseable AVFoundation audio teardown" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi
    sounds_destroyed=$(printf '%s\n' "$teardown_lines" | sed -n 's/.*soundsDestroyed=\([0-9][0-9]*\).*/\1/p' | awk '{s += $1} END {print s + 0}')
    temp_files_removed=$(printf '%s\n' "$teardown_lines" | sed -n 's/.*tempFilesRemoved=\([0-9][0-9]*\).*/\1/p' | awk '{s += $1} END {print s + 0}')
    temp_file_remove_failures=$(printf '%s\n' "$teardown_lines" | sed -n 's/.*tempFileRemoveFailures=\([0-9][0-9]*\).*/\1/p' | awk '{s += $1} END {print s + 0}')

    if [ "$temp_file_remove_failures" -ne 0 ]; then
        echo "$label reported AVFoundation temp-file removal failures: $temp_file_remove_failures" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if [ "$sounds_destroyed" -lt "$min_temp_files_removed" ]; then
        echo "$label destroyed $sounds_destroyed AVFoundation sounds, expected at least $min_temp_files_removed" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if [ "$temp_files_removed" -lt "$min_temp_files_removed" ]; then
        echo "$label removed $temp_files_removed AVFoundation temp files, expected at least $min_temp_files_removed" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi
}

validate_metal_renderer_cleanup() {
    log_path=$1
    label=$2

    cleanup_lines=$(grep "Metal renderer resource cleanup:" "$log_path" || true)
    if [ -z "$cleanup_lines" ]; then
        echo "$label did not report Metal renderer resource cleanup" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    nonzero_cleanup=$(printf '%s\n' "$cleanup_lines" | grep -Ev "Metal renderer resource cleanup: renderTargets=0 rasterStates=0 blendStates=0 depthStates=0 textures=0 samplers=0 shaders=0 buffers=0 vertexArrays=0 queries=0 retainedBuffers=0" || true)
    if [ -n "$nonzero_cleanup" ]; then
        echo "$label reported nonzero Metal renderer resources at cleanup" >&2
        printf '%s\n' "$nonzero_cleanup" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi
}

write_settings() {
    settings_path=$1
    rendering_technique=$2
    cat > "$settings_path" <<EOF
{
  "Rendering": {
    "EnableVR": false,
    "RenderingAPI": "Metal",
    "RenderingTechnique": "$rendering_technique",
    "PixelDensity": 1.0,
    "Supersampling": 1
  },
  "File": {
    "Load": [ "$content_path" ]
  },
  "Window": {
    "FullScreen": false,
    "PositionX": 0,
    "PositionY": 0,
    "Width": 1280,
    "Height": 720
  },
  "Playback": {
    "Location": {
      "Rotation": [ 0, 0, 0, 1 ],
      "Scale": 1,
      "Flip": "N",
      "Translation": [ 0, 0, 0 ]
    },
    "PlayerSpawn": {
      "Custom": {
        "Rotation": [ 0, 0, 0, 1 ],
        "Scale": 1,
        "Flip": "N",
        "Translation": [ 0, 0, 0 ]
      },
      "Location": "Default"
    }
  },
  "Sound": {
    "Device": "Default"
  },
  "UI": {
    "EnableHaptics": true,
    "LeftHanded": false,
    "UISoundVolume": 0.5
  }
}
EOF
}

static_settings_path="$tmp_dir/static_settings.json"
pretessellated_settings_path="$tmp_dir/pretessellated_settings.json"
write_settings "$static_settings_path" "Static"
write_settings "$pretessellated_settings_path" "Pretessellated"

run_cli_contract_check() {
    log_path="$tmp_dir/cli_contract.log"
    set +e
    "$app_path" "$content_path" "$content_path" >"$log_path" 2>&1
    app_status=$?
    set -e

    if [ "$app_status" -ne 1 ]; then
        echo "command-line contract check exited with status $app_status, expected 1" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if ! grep -q "appImmViewerMetal accepts at most one command-line IMM content path" "$log_path"; then
        echo "command-line contract check did not report the expected error" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    echo "Standalone Metal command-line contract validation passed."
}

run_native_frame_failure_contract_check() {
    log_path="$tmp_dir/native_frame_failure_contract.log"
    player_log_path="$tmp_dir/native_frame_failure_contract.player.log"
    rm -f "$repo_root/metal_player_debug.txt"
    rm -f "$player_log_path"

    set +e
    (
        cd "$repo_root"
        IMM_METAL_VALIDATE_FRAME=1 \
        IMM_METAL_VALIDATE_MAX_FRAME=32 \
        IMM_METAL_VALIDATE_MIN_NONZERO=1 \
        IMM_METAL_VALIDATE_FORCE_NATIVE_FRAME_FAILURE=1 \
        IMM_METAL_EXIT_AFTER_VALIDATE=1 \
        IMM_METAL_LOG_PATH="$player_log_path" \
        "$app_path" "$static_settings_path" "$content_path"
    ) >"$log_path" 2>&1
    app_status=$?
    set -e

    if [ -f "$player_log_path" ]; then
        cat "$player_log_path" >> "$log_path"
        rm -f "$player_log_path"
    fi

    if [ -f "$repo_root/metal_player_debug.txt" ]; then
        cat "$repo_root/metal_player_debug.txt" >> "$log_path" 2>/dev/null || true
        rm -f "$repo_root/metal_player_debug.txt"
    fi

    if [ "$app_status" -ne 2 ]; then
        echo "native-frame failure contract check exited with status $app_status, expected 2" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if ! grep -q "IMM Metal validation failed: native frame setup failed repeatedly: forced native frame setup failure" "$log_path"; then
        echo "native-frame failure contract check did not report the expected validation failure" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if ! grep -q "IMM Metal validation cleanup: done=1 exitCode=2" "$log_path"; then
        echo "native-frame failure contract check did not report clean failure teardown" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    echo "Standalone Metal native-frame failure validation passed."
}

run_bundle_contract_check() {
    executable_name=$(basename -- "$app_path")
    app_contents=$(dirname -- "$(dirname -- "$app_path")")
    app_bundle=$(dirname -- "$app_contents")
    info_plist="$app_contents/Info.plist"
    bundled_settings="$app_contents/Resources/appImmViewerMetal-settings.json"

    if [ "$(basename -- "$app_bundle")" != "appImmViewerMetal.app" ] ||
       [ "$(basename -- "$app_contents")" != "Contents" ]; then
        echo "appImmViewerMetal is not inside the expected .app bundle layout: $app_path" >&2
        exit 1
    fi

    if [ ! -f "$info_plist" ]; then
        echo "appImmViewerMetal bundle is missing Info.plist: $info_plist" >&2
        exit 1
    fi

    if [ ! -f "$bundled_settings" ]; then
        echo "appImmViewerMetal bundle is missing default Metal settings: $bundled_settings" >&2
        exit 1
    fi

    if ! grep -q '"RenderingAPI": "Metal"' "$bundled_settings"; then
        echo "appImmViewerMetal bundled settings do not select Metal: $bundled_settings" >&2
        exit 1
    fi

    if ! grep -q '"RenderingTechnique": "Static"' "$bundled_settings"; then
        echo "appImmViewerMetal bundled settings do not select the visually validated Static renderer path: $bundled_settings" >&2
        exit 1
    fi

    plist_executable=$(/usr/libexec/PlistBuddy -c "Print :CFBundleExecutable" "$info_plist")
    plist_identifier=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$info_plist")
    plist_package_type=$(/usr/libexec/PlistBuddy -c "Print :CFBundlePackageType" "$info_plist")
    plist_name=$(/usr/libexec/PlistBuddy -c "Print :CFBundleName" "$info_plist")
    plist_document_extension=$(/usr/libexec/PlistBuddy -c "Print :CFBundleDocumentTypes:0:CFBundleTypeExtensions:0" "$info_plist")
    plist_document_role=$(/usr/libexec/PlistBuddy -c "Print :CFBundleDocumentTypes:0:CFBundleTypeRole" "$info_plist")
    plist_document_uti=$(/usr/libexec/PlistBuddy -c "Print :CFBundleDocumentTypes:0:LSItemContentTypes:0" "$info_plist")
    plist_exported_uti=$(/usr/libexec/PlistBuddy -c "Print :UTExportedTypeDeclarations:0:UTTypeIdentifier" "$info_plist")
    plist_exported_extension=$(/usr/libexec/PlistBuddy -c "Print :UTExportedTypeDeclarations:0:UTTypeTagSpecification:public.filename-extension:0" "$info_plist")

    if [ "$plist_executable" != "$executable_name" ] ||
       [ "$plist_identifier" != "com.immersivefoundation.imm.metalviewer" ] ||
       [ "$plist_package_type" != "APPL" ] ||
       [ "$plist_name" != "IMM Metal Viewer" ] ||
       [ "$plist_document_extension" != "imm" ] ||
       [ "$plist_document_role" != "Viewer" ] ||
       [ "$plist_document_uti" != "com.immersivefoundation.imm" ] ||
       [ "$plist_exported_uti" != "com.immersivefoundation.imm" ] ||
       [ "$plist_exported_extension" != "imm" ]; then
        echo "appImmViewerMetal bundle metadata is unexpected" >&2
        echo "CFBundleExecutable=$plist_executable" >&2
        echo "CFBundleIdentifier=$plist_identifier" >&2
        echo "CFBundlePackageType=$plist_package_type" >&2
        echo "CFBundleName=$plist_name" >&2
        echo "CFBundleDocumentTypes[0].CFBundleTypeExtensions[0]=$plist_document_extension" >&2
        echo "CFBundleDocumentTypes[0].CFBundleTypeRole=$plist_document_role" >&2
        echo "CFBundleDocumentTypes[0].LSItemContentTypes[0]=$plist_document_uti" >&2
        echo "UTExportedTypeDeclarations[0].UTTypeIdentifier=$plist_exported_uti" >&2
        echo "UTExportedTypeDeclarations[0].UTTypeTagSpecification.public.filename-extension[0]=$plist_exported_extension" >&2
        exit 1
    fi

    if ! plutil -lint "$info_plist" >/dev/null; then
        echo "appImmViewerMetal Info.plist is not valid" >&2
        exit 1
    fi

    echo "Standalone Metal app bundle validation passed."
}

run_content_override_contract_check() {
    log_path="$tmp_dir/content_override_contract.log"
    player_log_path="$tmp_dir/content_override_contract.player.log"
    rm -f "$repo_root/metal_player_debug.txt"
    rm -f "$player_log_path"

    set +e
    (
        cd "$tmp_dir"
        IMM_METAL_VALIDATE_FRAME=1 \
        IMM_METAL_VALIDATE_MAX_FRAME=240 \
        IMM_METAL_VALIDATE_MIN_NONZERO=16 \
        IMM_METAL_VALIDATE_MIN_DRAWCALLS=1 \
        IMM_METAL_VALIDATE_MIN_PICTURE_DRAWCALLS=1 \
        IMM_METAL_VALIDATE_MIN_PICTURE360_DRAWCALLS=1 \
        IMM_METAL_VALIDATE_MIN_TRIANGLES=1 \
        IMM_METAL_VALIDATE_HELPER_DRAWS=1 \
        IMM_METAL_EXIT_AFTER_VALIDATE=1 \
        IMM_METAL_LOG_PATH="$player_log_path" \
        "$app_path" "$content_path"
    ) >"$log_path" 2>&1
    app_status=$?
    set -e

    if [ -f "$player_log_path" ]; then
        cat "$player_log_path" >> "$log_path"
        rm -f "$player_log_path"
    fi

    if [ -f "$repo_root/metal_player_debug.txt" ]; then
        cat "$repo_root/metal_player_debug.txt" >> "$log_path" 2>/dev/null || true
        rm -f "$repo_root/metal_player_debug.txt"
    fi

    if [ "$app_status" -ne 0 ]; then
        echo "content override contract check exited with status $app_status" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if ! grep -q "IMM Metal validation:" "$log_path"; then
        echo "content override contract check did not report successful Metal validation" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if ! grep -q "picture360DrawCalls=1" "$log_path"; then
        echo "content override contract check did not render the expected 360 picture path" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if ! grep -q "IMM Metal validation window title: sample1.imm - IMM Metal Player" "$log_path"; then
        echo "content override contract check did not report the expected window title" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if ! grep -q "IMM Metal validation cleanup: done=1 exitCode=0" "$log_path"; then
        echo "content override contract check did not report clean teardown" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    echo "Standalone Metal content override validation passed."
}

run_audio_contract_check() {
    log_path="$tmp_dir/audio_contract.log"
    player_log_path="$tmp_dir/audio_contract.player.log"
    rm -f "$repo_root/metal_player_debug.txt"
    rm -f "$player_log_path"

    audio_validation_frame=${IMM_METAL_VALIDATE_FRAME:-120}
    audio_max_frame=${IMM_METAL_VALIDATE_MAX_FRAME:-180}
    audio_min_nonzero=${IMM_METAL_VALIDATE_MIN_NONZERO:-16}
    audio_min_drawcalls=${IMM_METAL_VALIDATE_MIN_DRAWCALLS:-1}
    audio_min_picture_drawcalls=${IMM_METAL_VALIDATE_MIN_PICTURE_DRAWCALLS:-1}
    audio_min_picture360_drawcalls=${IMM_METAL_VALIDATE_MIN_PICTURE360_DRAWCALLS:-1}
    audio_min_triangles=${IMM_METAL_VALIDATE_MIN_TRIANGLES:-1}
    audio_helper_draws=${IMM_METAL_VALIDATE_HELPER_DRAWS:-1}
    audio_expected_opus=${IMM_METAL_VALIDATE_AUDIO_EXPECTED_OPUS_DECODED:-3}
    audio_min_wav=${IMM_METAL_VALIDATE_AUDIO_MIN_WAV_ADDED:-0}
    audio_min_total=${IMM_METAL_VALIDATE_AUDIO_MIN_TOTAL_ADDED:-$audio_expected_opus}
    audio_min_play_calls=${IMM_METAL_VALIDATE_AUDIO_MIN_PLAY_CALLS:-1}
    audio_min_playing_states=${IMM_METAL_VALIDATE_AUDIO_MIN_PLAYING_STATES:-1}
    audio_min_progress_markers=${IMM_METAL_VALIDATE_AUDIO_MIN_PROGRESS_MARKERS:-1}
    audio_progress_threshold_sec=${IMM_METAL_VALIDATE_AUDIO_PROGRESS_THRESHOLD_SEC:-1.0}

    set +e
    (
        cd "$repo_root"
        IMM_VIEWER_VALIDATE_DISABLE_AUDIO=0 \
        IMM_METAL_VALIDATE_FRAME="$audio_validation_frame" \
        IMM_METAL_VALIDATE_MAX_FRAME="$audio_max_frame" \
        IMM_METAL_VALIDATE_MIN_NONZERO="$audio_min_nonzero" \
        IMM_METAL_VALIDATE_MIN_DRAWCALLS="$audio_min_drawcalls" \
        IMM_METAL_VALIDATE_MIN_PICTURE_DRAWCALLS="$audio_min_picture_drawcalls" \
        IMM_METAL_VALIDATE_MIN_PICTURE360_DRAWCALLS="$audio_min_picture360_drawcalls" \
        IMM_METAL_VALIDATE_MIN_TRIANGLES="$audio_min_triangles" \
        IMM_METAL_VALIDATE_HELPER_DRAWS="$audio_helper_draws" \
        IMM_AVFOUNDATION_AUDIO_PROGRESS_THRESHOLD_SEC="$audio_progress_threshold_sec" \
        IMM_METAL_EXIT_AFTER_VALIDATE=1 \
        IMM_METAL_LOG_PATH="$player_log_path" \
        "$app_path" "$static_settings_path" "$content_path"
    ) >"$log_path" 2>&1
    app_status=$?
    set -e

    if [ -f "$player_log_path" ]; then
        cat "$player_log_path" >> "$log_path"
        rm -f "$player_log_path"
    fi

    if [ -f "$repo_root/metal_player_debug.txt" ]; then
        cat "$repo_root/metal_player_debug.txt" >> "$log_path" 2>/dev/null || true
        rm -f "$repo_root/metal_player_debug.txt"
    fi

    if [ "$app_status" -ne 0 ]; then
        echo "audio contract check exited with status $app_status" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if ! grep -q "IMM Metal validation:" "$log_path"; then
        echo "audio contract check did not report successful Metal validation" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    decoded_lines=$(grep "Decoded Ogg Opus sound to PCM temp WAV for AVFoundation" "$log_path" || true)
    decoded_count=$(printf '%s\n' "$decoded_lines" | sed -n 's/.*id=\([0-9][0-9]*\).*/\1/p' | sort -u | wc -l | tr -d ' ')
    if [ "$decoded_count" -ne "$audio_expected_opus" ]; then
        echo "audio contract check decoded $decoded_count distinct Ogg Opus sounds, expected $audio_expected_opus" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    wav_count=$(grep "Add WAV sound object .* ID=[0-9][0-9]*" "$log_path" | sort -u | wc -l | tr -d ' ' || true)
    if [ "$wav_count" -lt "$audio_min_wav" ]; then
        echo "audio contract check added $wav_count unique WAV sound objects, expected at least $audio_min_wav" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    total_count=$((decoded_count + wav_count))
    if [ "$total_count" -lt "$audio_min_total" ]; then
        echo "audio contract check saw $total_count total decoded/added sound objects, expected at least $audio_min_total" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    play_count=$(grep "AVFoundation audio Play accepted: id=[0-9][0-9]*" "$log_path" | wc -l | tr -d ' ' || true)
    if [ "$play_count" -lt "$audio_min_play_calls" ]; then
        echo "audio contract check saw $play_count accepted AVFoundation Play calls, expected at least $audio_min_play_calls" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    playing_state_count=$(grep "AVFoundation audio Playback state: id=[0-9][0-9]* state=playing" "$log_path" | wc -l | tr -d ' ' || true)
    if [ "$playing_state_count" -lt "$audio_min_playing_states" ]; then
        echo "audio contract check saw $playing_state_count AVFoundation playing-state transitions, expected at least $audio_min_playing_states" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    progress_count=$(grep "AVFoundation audio Playback progress: id=[0-9][0-9]*" "$log_path" | wc -l | tr -d ' ' || true)
    if [ "$progress_count" -lt "$audio_min_progress_markers" ]; then
        echo "audio contract check saw $progress_count AVFoundation playback-progress markers, expected at least $audio_min_progress_markers" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if grep -q "AVFoundation audio Play rejected" "$log_path"; then
        echo "audio contract check saw rejected AVFoundation Play calls" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if grep -Eq "macOS audio backend could not decode Ogg Opus audio blob|AVFoundation audio Ogg Opus decode failed" "$log_path"; then
        echo "audio contract check hit an Ogg Opus decode failure" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if ! grep -q "Loaded in SPU!" "$log_path"; then
        echo "audio contract check did not complete SPU audio loading" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if ! grep -q "IMM Metal validation cleanup: done=1 exitCode=0" "$log_path"; then
        echo "audio contract check did not report clean teardown" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi
    validate_metal_renderer_cleanup "$log_path" "audio contract check"

    validate_avfoundation_audio_teardown "$log_path" "audio contract check" "$total_count"

    echo "Standalone Metal audio validation passed: opusDecoded=$decoded_count wavAdded=$wav_count total=$total_count playCalls=$play_count playingStates=$playing_state_count progressMarkers=$progress_count progressThresholdSec=$audio_progress_threshold_sec."
}

run_interactive_audio_contract_check() {
    log_path="$tmp_dir/interactive_audio_contract.log"
    player_log_path="$tmp_dir/interactive_audio_contract.player.log"
    rm -f "$repo_root/metal_player_debug.txt"
    rm -f "$player_log_path"

    audio_expected_opus=${IMM_METAL_VALIDATE_AUDIO_EXPECTED_OPUS_DECODED:-3}
    audio_min_wav=${IMM_METAL_VALIDATE_AUDIO_MIN_WAV_ADDED:-0}
    audio_min_total=${IMM_METAL_VALIDATE_AUDIO_MIN_TOTAL_ADDED:-$audio_expected_opus}
    audio_min_play_calls=${IMM_METAL_VALIDATE_AUDIO_MIN_PLAY_CALLS:-1}
    audio_min_playing_states=${IMM_METAL_VALIDATE_AUDIO_MIN_PLAYING_STATES:-1}
    audio_min_progress_markers=${IMM_METAL_VALIDATE_AUDIO_MIN_PROGRESS_MARKERS:-1}
    audio_progress_threshold_sec=${IMM_METAL_VALIDATE_AUDIO_PROGRESS_THRESHOLD_SEC:-1.0}
    validate_volume_controls=${IMM_METAL_VALIDATE_VOLUME_CONTROLS:-0}
    validate_playback_controls=${IMM_METAL_VALIDATE_PLAYBACK_CONTROLS:-0}
    validate_open_failure_restore=${IMM_METAL_VALIDATE_OPEN_FAILURE_RESTORE:-0}
    validate_recent_documents=${IMM_METAL_VALIDATE_RECENT_DOCUMENTS:-0}
    smoke_exit_after_sec=${IMM_METAL_INTERACTIVE_SMOKE_EXIT_AFTER_SECONDS:-3.0}
    smoke_timeout_sec=${IMM_METAL_INTERACTIVE_SMOKE_TIMEOUT_SECONDS:-20}

    set +e
    (
        cd "$repo_root"
        IMM_METAL_INTERACTIVE_SMOKE_EXIT_AFTER_SECONDS="$smoke_exit_after_sec" \
        IMM_METAL_VALIDATE_VOLUME_CONTROLS="$validate_volume_controls" \
        IMM_METAL_VALIDATE_PLAYBACK_CONTROLS="$validate_playback_controls" \
        IMM_METAL_VALIDATE_OPEN_FAILURE_RESTORE="$validate_open_failure_restore" \
        IMM_METAL_SUPPRESS_OPEN_FAILURE_ALERT="$validate_open_failure_restore" \
        IMM_METAL_VALIDATE_RECENT_DOCUMENTS="$validate_recent_documents" \
        IMM_AVFOUNDATION_AUDIO_PROGRESS_THRESHOLD_SEC="$audio_progress_threshold_sec" \
        IMM_METAL_LOG_PATH="$player_log_path" \
        "$app_path" "$static_settings_path" "$content_path"
    ) >"$log_path" 2>&1 &
    app_pid=$!

    start_time=$(date +%s)
    app_status=0
    while kill -0 "$app_pid" 2>/dev/null; do
        now=$(date +%s)
        if [ $((now - start_time)) -ge "$smoke_timeout_sec" ]; then
            kill "$app_pid" 2>/dev/null || true
            wait "$app_pid" 2>/dev/null || true
            app_status=124
            break
        fi
        sleep 1
    done
    if [ "$app_status" -eq 0 ]; then
        wait "$app_pid"
        app_status=$?
    fi
    set -e

    if [ -f "$player_log_path" ]; then
        cat "$player_log_path" >> "$log_path"
        rm -f "$player_log_path"
    fi

    if [ -f "$repo_root/metal_player_debug.txt" ]; then
        cat "$repo_root/metal_player_debug.txt" >> "$log_path" 2>/dev/null || true
        rm -f "$repo_root/metal_player_debug.txt"
    fi

    if [ "$app_status" -ne 0 ]; then
        echo "interactive audio contract check exited with status $app_status" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if grep -q "IMM Metal validation:" "$log_path"; then
        echo "interactive audio contract unexpectedly used validation rendering" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if ! grep -q "IMM Metal player sound backend: AVFoundation" "$log_path"; then
        echo "interactive audio contract did not select AVFoundation" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    decoded_lines=$(grep "Decoded Ogg Opus sound to PCM temp WAV for AVFoundation" "$log_path" || true)
    decoded_count=$(printf '%s\n' "$decoded_lines" | sed -n 's/.*id=\([0-9][0-9]*\).*/\1/p' | sort -u | wc -l | tr -d ' ')
    if [ "$decoded_count" -ne "$audio_expected_opus" ]; then
        echo "interactive audio contract decoded $decoded_count distinct Ogg Opus sounds, expected $audio_expected_opus" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    wav_count=$(grep "Add WAV sound object .* ID=[0-9][0-9]*" "$log_path" | sort -u | wc -l | tr -d ' ' || true)
    if [ "$wav_count" -lt "$audio_min_wav" ]; then
        echo "interactive audio contract added $wav_count unique WAV sound objects, expected at least $audio_min_wav" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    total_count=$((decoded_count + wav_count))
    if [ "$total_count" -lt "$audio_min_total" ]; then
        echo "interactive audio contract saw $total_count total decoded/added sound objects, expected at least $audio_min_total" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    play_count=$(grep "AVFoundation audio Play accepted: id=[0-9][0-9]*" "$log_path" | wc -l | tr -d ' ' || true)
    if [ "$play_count" -lt "$audio_min_play_calls" ]; then
        echo "interactive audio contract saw $play_count accepted AVFoundation Play calls, expected at least $audio_min_play_calls" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    playing_state_count=$(grep "AVFoundation audio Playback state: id=[0-9][0-9]* state=playing" "$log_path" | wc -l | tr -d ' ' || true)
    if [ "$playing_state_count" -lt "$audio_min_playing_states" ]; then
        echo "interactive audio contract saw $playing_state_count AVFoundation playing-state transitions, expected at least $audio_min_playing_states" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    progress_count=$(grep "AVFoundation audio Playback progress: id=[0-9][0-9]*" "$log_path" | wc -l | tr -d ' ' || true)
    if [ "$progress_count" -lt "$audio_min_progress_markers" ]; then
        echo "interactive audio contract saw $progress_count AVFoundation playback-progress markers, expected at least $audio_min_progress_markers" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if grep -q "AVFoundation audio Play rejected" "$log_path"; then
        echo "interactive audio contract saw rejected AVFoundation Play calls" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if grep -Eq "macOS audio backend could not decode Ogg Opus audio blob|AVFoundation audio Ogg Opus decode failed" "$log_path"; then
        echo "interactive audio contract hit an Ogg Opus decode failure" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if ! grep -q "Loaded in SPU!" "$log_path"; then
        echo "interactive audio contract did not complete SPU audio loading" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if ! grep -q "IMM Metal interactive smoke:" "$log_path"; then
        echo "interactive audio contract did not reach the normal-run smoke exit" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if ! grep -q "IMM Metal player cleanup: exitCode=0" "$log_path"; then
        echo "interactive audio contract did not report clean normal-run teardown" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi
    validate_metal_renderer_cleanup "$log_path" "interactive audio contract"

    validate_avfoundation_audio_teardown "$log_path" "interactive audio contract" "$total_count"

    if [ "$validate_volume_controls" != "0" ]; then
        if ! grep -q "IMM Metal player audio volume smoke: done=1 final=0.75 muted=0" "$log_path"; then
            echo "interactive audio contract did not complete volume/mute smoke validation" >&2
            tail -n 80 "$log_path" >&2
            exit 1
        fi
        if ! grep -q "IMM Metal player audio volume: reason=volume-smoke-mute volume=0.50 muted=1 applied=0.00" "$log_path"; then
            echo "interactive audio contract did not observe mute applying zero document volume" >&2
            tail -n 80 "$log_path" >&2
            exit 1
        fi
        if ! grep -q "IMM Metal player audio volume: reason=volume-smoke-restore volume=0.75 muted=0 applied=0.75" "$log_path"; then
            echo "interactive audio contract did not observe volume restore" >&2
            tail -n 80 "$log_path" >&2
            exit 1
        fi
    fi

    if [ "$validate_playback_controls" != "0" ]; then
        if ! grep -q "IMM Metal player playback control smoke: done=1 pauseResume=1 restart=1" "$log_path"; then
            echo "interactive audio contract did not complete playback-control smoke validation" >&2
            tail -n 80 "$log_path" >&2
            exit 1
        fi
    fi

    if [ "$validate_open_failure_restore" != "0" ]; then
        if ! grep -q "IMM Metal player open failure restore: restored=1" "$log_path"; then
            echo "interactive audio contract did not restore the previous document after a failed open" >&2
            tail -n 120 "$log_path" >&2
            exit 1
        fi
        if ! grep -q "IMM Metal player open failure restore smoke: done=1 viewerReady=1" "$log_path"; then
            echo "interactive audio contract did not complete failed-open restore smoke validation" >&2
            tail -n 120 "$log_path" >&2
            exit 1
        fi
    fi

    if [ "$validate_recent_documents" != "0" ]; then
        if ! grep -Eq "IMM Metal player recent document smoke: done=1 recentImmURLs=[1-9][0-9]* menuItems=[1-9][0-9]*" "$log_path"; then
            echo "interactive audio contract did not complete recent-document smoke validation" >&2
            tail -n 80 "$log_path" >&2
            exit 1
        fi
    fi

    echo "Standalone Metal interactive audio validation passed: opusDecoded=$decoded_count wavAdded=$wav_count total=$total_count playCalls=$play_count playingStates=$playing_state_count progressMarkers=$progress_count progressThresholdSec=$audio_progress_threshold_sec."
}

validate_ppm_capture() {
    name=$1
    capture_path=$2
    expected_width=$3
    expected_height=$4
    log_path=$5

    if [ ! -s "$capture_path" ]; then
        echo "$name Metal validation did not write capture: $capture_path" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    magic=$(sed -n '1p' "$capture_path")
    dimensions=$(sed -n '2p' "$capture_path")
    max_value=$(sed -n '3p' "$capture_path")

    check_dimensions=1
    if [ "${IMM_METAL_VALIDATE_EXPECTED_VALUES:-1}" = "0" ]; then
        check_dimensions=0
    fi

    dimensions_unexpected=0
    if [ "$check_dimensions" -eq 1 ] && [ "$dimensions" != "${expected_width} ${expected_height}" ]; then
        dimensions_unexpected=1
    fi

    if [ "$magic" != "P6" ] ||
       [ "$max_value" != "255" ] ||
       [ "$dimensions_unexpected" -eq 1 ]; then
        echo "$name Metal validation capture has an unexpected PPM header: $capture_path" >&2
        echo "expected: P6 / ${expected_width} ${expected_height} / 255" >&2
        echo "actual:   $magic / $dimensions / $max_value" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi
}

validate_png_capture() {
    name=$1
    capture_path=$2
    log_path=$3

    if [ ! -s "$capture_path" ]; then
        echo "$name Metal validation did not write capture: $capture_path" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    signature=$(dd if="$capture_path" bs=8 count=1 2>/dev/null | od -An -tx1 | tr -d ' \n')
    if [ "$signature" != "89504e470d0a1a0a" ]; then
        echo "$name Metal validation capture has an unexpected PNG signature: $capture_path" >&2
        echo "actual: $signature" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi
}

if [ "$check_cli_contract" -eq 1 ]; then
    run_cli_contract_check
    exit 0
fi

if [ "$check_native_frame_failure_contract" -eq 1 ]; then
    run_native_frame_failure_contract_check
    exit 0
fi

if [ "$check_bundle_contract" -eq 1 ]; then
    run_bundle_contract_check
    exit 0
fi

if [ "$check_content_override_contract" -eq 1 ]; then
    run_content_override_contract_check
    exit 0
fi

if [ "$check_audio_contract" -eq 1 ]; then
    run_audio_contract_check
    exit 0
fi

if [ "$check_interactive_audio_contract" -eq 1 ]; then
    run_interactive_audio_contract_check
    exit 0
fi

run_case() {
    name=$1
    expected_hash=$2
    expected_draw_calls=$3
    expected_triangles=$4
    expected_pixels=$5
    expected_nonzero=$6
    resize_width=$7
    resize_height=$8
    shift 8
    log_path="$tmp_dir/$name.log"
    player_log_path="$tmp_dir/$name.player.log"

    rm -f "$repo_root/metal_player_debug.txt"
    rm -f "$player_log_path"

    echo "Running $name Metal validation..."
    timeout_seconds=${IMM_METAL_VALIDATE_TIMEOUT:-30}
    timeout_flag="$tmp_dir/$name.timeout"
    capture_path=
    if [ -n "$capture_dir" ]; then
        capture_path="$capture_dir/$name.$capture_format"
        rm -f "$capture_path"
    fi
    (
        cd "$repo_root"
        IMM_METAL_VALIDATE_FRAME="${IMM_METAL_VALIDATE_FRAME:-1}" \
        IMM_METAL_VALIDATE_MAX_FRAME="${IMM_METAL_VALIDATE_MAX_FRAME:-240}" \
        IMM_METAL_VALIDATE_MIN_NONZERO="${IMM_METAL_VALIDATE_MIN_NONZERO:-16}" \
        IMM_METAL_VALIDATE_MIN_DRAWCALLS="${IMM_METAL_VALIDATE_MIN_DRAWCALLS:-1}" \
        IMM_METAL_VALIDATE_MIN_PICTURE_DRAWCALLS="${IMM_METAL_VALIDATE_MIN_PICTURE_DRAWCALLS:-1}" \
        IMM_METAL_VALIDATE_MIN_PICTURE2D_DRAWCALLS="${IMM_METAL_VALIDATE_MIN_PICTURE2D_DRAWCALLS:-0}" \
        IMM_METAL_VALIDATE_MIN_PICTURE360_DRAWCALLS="${IMM_METAL_VALIDATE_MIN_PICTURE360_DRAWCALLS:-1}" \
        IMM_METAL_VALIDATE_MIN_PICTURE360_EQUIRECT_DRAWCALLS="${IMM_METAL_VALIDATE_MIN_PICTURE360_EQUIRECT_DRAWCALLS:-0}" \
        IMM_METAL_VALIDATE_MIN_PICTURE360_CUBEMAP_DRAWCALLS="${IMM_METAL_VALIDATE_MIN_PICTURE360_CUBEMAP_DRAWCALLS:-0}" \
        IMM_METAL_VALIDATE_MIN_TRIANGLES="${IMM_METAL_VALIDATE_MIN_TRIANGLES:-1}" \
        IMM_METAL_VALIDATE_RESIZE_FRAME="${IMM_METAL_VALIDATE_RESIZE_FRAME:-}" \
        IMM_METAL_VALIDATE_RELOAD_FRAME="${IMM_METAL_VALIDATE_RELOAD_FRAME:-}" \
        IMM_METAL_VALIDATE_RELOAD_PATH="${IMM_METAL_VALIDATE_RELOAD_PATH:-}" \
        IMM_METAL_VALIDATE_RESIZE_WIDTH="$resize_width" \
        IMM_METAL_VALIDATE_RESIZE_HEIGHT="$resize_height" \
        IMM_METAL_VALIDATE_CAPTURE_PATH="$capture_path" \
        IMM_METAL_VALIDATE_HELPER_DRAWS="${IMM_METAL_VALIDATE_HELPER_DRAWS:-1}" \
        IMM_METAL_EXIT_AFTER_VALIDATE="${IMM_METAL_EXIT_AFTER_VALIDATE:-1}" \
        IMM_METAL_LOG_PATH="$player_log_path" \
        "$app_path" "$@"
    ) >"$log_path" 2>&1 &
    app_pid=$!

    elapsed=0
    while kill -0 "$app_pid" 2>/dev/null; do
        if [ "$elapsed" -ge "$timeout_seconds" ]; then
            echo "timed out after ${timeout_seconds}s" > "$timeout_flag"
            kill "$app_pid" 2>/dev/null || true
            break
        fi
        sleep 1
        elapsed=$((elapsed + 1))
    done

    set +e
    wait "$app_pid"
    app_status=$?
    set -e

    if [ -f "$player_log_path" ]; then
        cat "$player_log_path" >> "$log_path"
        rm -f "$player_log_path"
    fi

    if [ -f "$repo_root/metal_player_debug.txt" ]; then
        cat "$repo_root/metal_player_debug.txt" >> "$log_path" 2>/dev/null || true
        rm -f "$repo_root/metal_player_debug.txt"
    fi

    if [ -f "$timeout_flag" ]; then
        echo "$name Metal validation timed out after ${timeout_seconds}s" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if [ "$app_status" -ne 0 ]; then
        echo "$name Metal validation exited with status $app_status" >&2
        tail -n 80 "$log_path" >&2
        exit "$app_status"
    fi

    if ! grep -q "IMM Metal validation:" "$log_path"; then
        echo "$name did not report successful Metal validation" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if grep -q "IMM Metal validation failed" "$log_path"; then
        echo "$name reported failed Metal validation" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if grep -Ei "shader.*failed|pipeline.*failed|assert|segmentation fault|abort" "$log_path" >/dev/null; then
        echo "$name log contains a failure signature" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if grep -Ei "IMM Metal error: .*does not support|IMM Metal error: .*not implemented" "$log_path" >/dev/null; then
        echo "$name hit an unsupported Metal renderer path" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if ! grep -q "IMM Metal validation cleanup: done=1 exitCode=0" "$log_path"; then
        echo "$name did not report clean Metal validation teardown" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi
    validate_metal_renderer_cleanup "$log_path" "$name"

    if [ "${IMM_METAL_VALIDATE_HELPER_DRAWS:-1}" != "0" ]; then
        if ! grep -q "IMM Metal pipeline sanity: singleTriangle=1" "$log_path"; then
            echo "$name did not pass the Metal single-triangle pipeline sanity check" >&2
            tail -n 80 "$log_path" >&2
            exit 1
        fi
        if ! grep -q "IMM Metal pipeline sanity: indexedBaseVertexTriangle=1" "$log_path"; then
            echo "$name did not pass the Metal indexed base-vertex triangle pipeline sanity check" >&2
            tail -n 80 "$log_path" >&2
            exit 1
        fi
        if ! grep -q "IMM Metal pipeline sanity: picture2DShader=1" "$log_path"; then
            echo "$name did not pass the Metal 2D-picture shader sanity check" >&2
            tail -n 80 "$log_path" >&2
            exit 1
        fi
        if ! grep -q "IMM Metal pipeline sanity: picture360CubemapShader=1" "$log_path"; then
            echo "$name did not pass the Metal 360-cubemap shader sanity check" >&2
            tail -n 80 "$log_path" >&2
            exit 1
        fi
        if ! grep -q "IMM Metal helper validation: unitQuad=1 blendQuad=1 indirectQuad=1 indirectIndexedTriangle=1 unitCubeXYZ=1 unitCubeXYZNOR=1 fixedStateHints=1 memoryBarrier=1 drawable=1280x720" "$log_path"; then
            echo "$name did not exercise Metal helper draws" >&2
            tail -n 80 "$log_path" >&2
            exit 1
        fi
        helper_line=$(grep "IMM Metal helper validation:" "$log_path" | tail -n 1)
        helper_texture_handle=$(printf '%s\n' "$helper_line" | sed -n 's/.* textureHandle=\([0-9][0-9]*\).*/\1/p')
        if [ -z "$helper_texture_handle" ] || [ "$helper_texture_handle" = "0" ]; then
            echo "$name Metal helper texture handle was not reported" >&2
            tail -n 80 "$log_path" >&2
            exit 1
        fi
        helper_timing_ns=$(printf '%s\n' "$helper_line" | sed -n 's/.* timingNs=\([0-9][0-9]*\).*/\1/p')
        if [ -z "$helper_timing_ns" ] || [ "$helper_timing_ns" = "0" ]; then
            echo "$name Metal helper draw timing was not reported" >&2
            tail -n 80 "$log_path" >&2
            exit 1
        fi
        if ! grep -q "Metal renderer report: standaloneProven=.*nativeFrame.*staticPaint.*picture360CubemapShaderSmoke.*cpuTiming" "$log_path"; then
            echo "$name did not report the proven Metal renderer feature surface" >&2
            tail -n 80 "$log_path" >&2
            exit 1
        fi
        if ! grep -q "Metal renderer report: standaloneUnsupported=.*externalTextureWrapping.*computeShaders.*gpuTimestampQueries" "$log_path"; then
            echo "$name did not report the unsupported Metal renderer feature surface" >&2
            tail -n 80 "$log_path" >&2
            exit 1
        fi
    fi

    if [ -n "$resize_width" ]; then
        if ! grep -q "IMM Metal validation resize: .* width=$resize_width height=$resize_height" "$log_path"; then
            echo "$name did not report the expected validation resize to ${resize_width}x${resize_height}" >&2
            tail -n 80 "$log_path" >&2
            exit 1
        fi
    fi

    if [ -n "${IMM_METAL_VALIDATE_RELOAD_FRAME:-}" ]; then
        if ! grep -q "IMM Metal validation reload: frame=${IMM_METAL_VALIDATE_RELOAD_FRAME}" "$log_path"; then
            echo "$name did not report the expected in-process reload at frame ${IMM_METAL_VALIDATE_RELOAD_FRAME}" >&2
            tail -n 80 "$log_path" >&2
            exit 1
        fi
        if ! grep -q "IMM Metal validation window title: sample1.imm - IMM Metal Player" "$log_path"; then
            echo "$name did not report the expected window title after in-process reload" >&2
            tail -n 80 "$log_path" >&2
            exit 1
        fi
    fi

    validation_line=$(grep "IMM Metal validation:" "$log_path" | tail -n 1)

    actual_hash=$(printf '%s\n' "$validation_line" | sed -n 's/.* hash=\([0-9][0-9]*\) .*/\1/p')
    actual_draw_calls=$(printf '%s\n' "$validation_line" | sed -n 's/.* drawCalls=\([0-9][0-9]*\) .*/\1/p')
    actual_picture_draw_calls=$(printf '%s\n' "$validation_line" | sed -n 's/.* pictureDrawCalls=\([0-9][0-9]*\) .*/\1/p')
    actual_picture2d_draw_calls=$(printf '%s\n' "$validation_line" | sed -n 's/.* picture2DDrawCalls=\([0-9][0-9]*\) .*/\1/p')
    actual_picture360_draw_calls=$(printf '%s\n' "$validation_line" | sed -n 's/.* picture360DrawCalls=\([0-9][0-9]*\) .*/\1/p')
    actual_picture360_equirect_draw_calls=$(printf '%s\n' "$validation_line" | sed -n 's/.* picture360EquirectDrawCalls=\([0-9][0-9]*\) .*/\1/p')
    actual_picture360_cubemap_draw_calls=$(printf '%s\n' "$validation_line" | sed -n 's/.* picture360CubemapDrawCalls=\([0-9][0-9]*\) .*/\1/p')
    actual_triangles=$(printf '%s\n' "$validation_line" | sed -n 's/.* triangles=\([0-9][0-9]*\) .*/\1/p')
    actual_pixels=$(printf '%s\n' "$validation_line" | sed -n 's/.* pixels=\([0-9][0-9]*\) .*/\1/p')
    actual_nonzero=$(printf '%s\n' "$validation_line" | sed -n 's/.* nonZero=\([0-9][0-9]*\) .*/\1/p')

    if [ -z "$actual_hash" ] ||
       [ -z "$actual_draw_calls" ] ||
       [ -z "$actual_picture_draw_calls" ] ||
       [ -z "$actual_picture2d_draw_calls" ] ||
       [ -z "$actual_picture360_draw_calls" ] ||
       [ -z "$actual_picture360_equirect_draw_calls" ] ||
       [ -z "$actual_picture360_cubemap_draw_calls" ] ||
       [ -z "$actual_triangles" ] ||
       [ -z "$actual_pixels" ] ||
       [ -z "$actual_nonzero" ]; then
        echo "$name Metal validation line could not be parsed" >&2
        echo "$validation_line" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if [ "$actual_picture_draw_calls" -lt "${IMM_METAL_VALIDATE_MIN_PICTURE_DRAWCALLS:-1}" ] ||
       [ "$actual_picture2d_draw_calls" -lt "${IMM_METAL_VALIDATE_MIN_PICTURE2D_DRAWCALLS:-0}" ] ||
       [ "$actual_picture360_draw_calls" -lt "${IMM_METAL_VALIDATE_MIN_PICTURE360_DRAWCALLS:-1}" ] ||
       [ "$actual_picture360_equirect_draw_calls" -lt "${IMM_METAL_VALIDATE_MIN_PICTURE360_EQUIRECT_DRAWCALLS:-0}" ] ||
       [ "$actual_picture360_cubemap_draw_calls" -lt "${IMM_METAL_VALIDATE_MIN_PICTURE360_CUBEMAP_DRAWCALLS:-0}" ]; then
        echo "$name Metal validation did not exercise the expected picture draw paths" >&2
        echo "actual: pictureDrawCalls=$actual_picture_draw_calls picture2DDrawCalls=$actual_picture2d_draw_calls picture360DrawCalls=$actual_picture360_draw_calls picture360EquirectDrawCalls=$actual_picture360_equirect_draw_calls picture360CubemapDrawCalls=$actual_picture360_cubemap_draw_calls" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if [ "$name" = "static" ] || [ "${name#repeat_}" != "$name" ]; then
        if [ "$actual_hash" = "5448870274179528411" ]; then
            echo "$name Metal validation matched the old backdrop-only hash; paint draw calls were submitted but not visibly composited" >&2
            tail -n 80 "$log_path" >&2
            exit 1
        fi
        if [ "$actual_hash" = "15781045072442920602" ]; then
            echo "$name Metal validation matched the old static opaque-paint hash; paint alpha/sample-mask coverage is not being applied" >&2
            tail -n 80 "$log_path" >&2
            exit 1
        fi
    fi

    if [ "$name" = "pretessellated" ]; then
        if [ "$actual_hash" = "17258452306413009819" ]; then
            echo "$name Metal validation matched the old pretessellated opaque-paint hash; paint alpha/sample-mask coverage is not being applied" >&2
            tail -n 80 "$log_path" >&2
            exit 1
        fi
    fi

    if [ "${IMM_METAL_VALIDATE_EXPECTED_VALUES:-1}" != "0" ]; then
        if [ \( "$expected_hash" != "-" -a "$actual_hash" != "$expected_hash" \) ] ||
           [ "$actual_draw_calls" != "$expected_draw_calls" ] ||
           [ "$actual_triangles" != "$expected_triangles" ] ||
           [ "$actual_pixels" != "$expected_pixels" ] ||
           [ "$actual_nonzero" != "$expected_nonzero" ]; then
            echo "$name Metal validation output changed" >&2
            echo "expected: pixels=$expected_pixels nonZero=$expected_nonzero hash=$expected_hash drawCalls=$expected_draw_calls triangles=$expected_triangles" >&2
            echo "actual:   pixels=$actual_pixels nonZero=$actual_nonzero hash=$actual_hash drawCalls=$actual_draw_calls triangles=$actual_triangles" >&2
            tail -n 80 "$log_path" >&2
            exit 1
        fi
    fi

    if [ -n "$capture_path" ]; then
        capture_width=$resize_width
        capture_height=$resize_height
        if [ -z "$capture_width" ]; then
            capture_width=1280
            capture_height=720
        fi
        if [ "$capture_format" = "png" ]; then
            validate_png_capture "$name" "$capture_path" "$log_path"
        else
            validate_ppm_capture "$name" "$capture_path" "$capture_width" "$capture_height" "$log_path"
        fi
        echo "$name Metal validation capture: $capture_path"
    fi

    printf '%s\n' "$validation_line"
}

run_repeat_contract_check() {
    repeat_count=${IMM_METAL_VALIDATE_REPEAT_COUNT:-3}
    if [ "$repeat_count" -lt 1 ]; then
        repeat_count=1
    fi

    i=1
    while [ "$i" -le "$repeat_count" ]; do
        run_case "repeat_$i" - 38 645802 921600 921600 "" "" "$static_settings_path" "$content_path"
        i=$((i + 1))
    done

    echo "Standalone Metal repeated launch validation passed."
}

run_reload_contract_check() {
    IMM_METAL_VALIDATE_FRAME=4 \
    IMM_METAL_VALIDATE_MAX_FRAME=120 \
    IMM_METAL_VALIDATE_RELOAD_FRAME=2 \
    IMM_METAL_VALIDATE_RELOAD_PATH="$content_path" \
    run_case reload - 38 645802 921600 921600 "" "" "$static_settings_path" "$content_path"

    echo "Standalone Metal in-process reload validation passed."
}

if [ "$check_repeat_contract" -eq 1 ]; then
    run_repeat_contract_check
    exit 0
fi

if [ "$check_reload_contract" -eq 1 ]; then
    run_reload_contract_check
    exit 0
fi

run_case static - 38 645802 921600 921600 "" "" "$static_settings_path" "$content_path"
run_case pretessellated - 38 645802 921600 921600 "" "" "$pretessellated_settings_path" "$content_path"

IMM_METAL_VALIDATE_FRAME=12 \
IMM_METAL_VALIDATE_RESIZE_FRAME=2 \
run_case resize - 38 645802 480000 480000 800 600 "$static_settings_path" "$content_path"

echo "Standalone Metal validation passed."
