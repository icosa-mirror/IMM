#!/bin/sh
set -eu

usage() {
    echo "usage: $0 [--cli-contract|--native-frame-failure-contract|--bundle-contract|--content-override-contract|--repeat-contract] [appImmViewerMetal-path] [content-path]" >&2
}

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
check_cli_contract=0
check_native_frame_failure_contract=0
check_bundle_contract=0
check_content_override_contract=0
check_repeat_contract=0
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
elif [ "${1:-}" = "--repeat-contract" ]; then
    check_repeat_contract=1
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
if [ -n "$capture_dir" ]; then
    mkdir -p "$capture_dir"
    capture_dir=$(CDPATH= cd -- "$capture_dir" && pwd)
fi

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/imm-metal-validate.XXXXXX")
cleanup() {
    rm -rf "$tmp_dir"
}
trap cleanup EXIT INT TERM

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

    if [ "$magic" != "P6" ] ||
       [ "$dimensions" != "${expected_width} ${expected_height}" ] ||
       [ "$max_value" != "255" ]; then
        echo "$name Metal validation capture has an unexpected PPM header: $capture_path" >&2
        echo "expected: P6 / ${expected_width} ${expected_height} / 255" >&2
        echo "actual:   $magic / $dimensions / $max_value" >&2
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
        capture_path="$capture_dir/$name.ppm"
        rm -f "$capture_path"
    fi
    (
        cd "$repo_root"
        IMM_METAL_VALIDATE_FRAME="${IMM_METAL_VALIDATE_FRAME:-1}" \
        IMM_METAL_VALIDATE_MAX_FRAME="${IMM_METAL_VALIDATE_MAX_FRAME:-240}" \
        IMM_METAL_VALIDATE_MIN_NONZERO="${IMM_METAL_VALIDATE_MIN_NONZERO:-16}" \
        IMM_METAL_VALIDATE_MIN_DRAWCALLS="${IMM_METAL_VALIDATE_MIN_DRAWCALLS:-1}" \
        IMM_METAL_VALIDATE_MIN_PICTURE_DRAWCALLS="${IMM_METAL_VALIDATE_MIN_PICTURE_DRAWCALLS:-1}" \
        IMM_METAL_VALIDATE_MIN_PICTURE360_DRAWCALLS="${IMM_METAL_VALIDATE_MIN_PICTURE360_DRAWCALLS:-1}" \
        IMM_METAL_VALIDATE_MIN_TRIANGLES="${IMM_METAL_VALIDATE_MIN_TRIANGLES:-1}" \
        IMM_METAL_VALIDATE_RESIZE_FRAME="${IMM_METAL_VALIDATE_RESIZE_FRAME:-}" \
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

    if [ "${IMM_METAL_VALIDATE_HELPER_DRAWS:-1}" != "0" ]; then
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
    fi

    if [ -n "$resize_width" ]; then
        if ! grep -q "IMM Metal validation resize: .* width=$resize_width height=$resize_height" "$log_path"; then
            echo "$name did not report the expected validation resize to ${resize_width}x${resize_height}" >&2
            tail -n 80 "$log_path" >&2
            exit 1
        fi
    fi

    validation_line=$(grep "IMM Metal validation:" "$log_path" | tail -n 1)

    actual_hash=$(printf '%s\n' "$validation_line" | sed -n 's/.* hash=\([0-9][0-9]*\) .*/\1/p')
    actual_draw_calls=$(printf '%s\n' "$validation_line" | sed -n 's/.* drawCalls=\([0-9][0-9]*\) .*/\1/p')
    actual_picture_draw_calls=$(printf '%s\n' "$validation_line" | sed -n 's/.* pictureDrawCalls=\([0-9][0-9]*\) .*/\1/p')
    actual_picture360_draw_calls=$(printf '%s\n' "$validation_line" | sed -n 's/.* picture360DrawCalls=\([0-9][0-9]*\) .*/\1/p')
    actual_triangles=$(printf '%s\n' "$validation_line" | sed -n 's/.* triangles=\([0-9][0-9]*\) .*/\1/p')
    actual_pixels=$(printf '%s\n' "$validation_line" | sed -n 's/.* pixels=\([0-9][0-9]*\) .*/\1/p')
    actual_nonzero=$(printf '%s\n' "$validation_line" | sed -n 's/.* nonZero=\([0-9][0-9]*\) .*/\1/p')

    if [ -z "$actual_hash" ] ||
       [ -z "$actual_draw_calls" ] ||
       [ -z "$actual_picture_draw_calls" ] ||
       [ -z "$actual_picture360_draw_calls" ] ||
       [ -z "$actual_triangles" ] ||
       [ -z "$actual_pixels" ] ||
       [ -z "$actual_nonzero" ]; then
        echo "$name Metal validation line could not be parsed" >&2
        echo "$validation_line" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if [ "$actual_picture_draw_calls" -lt "${IMM_METAL_VALIDATE_MIN_PICTURE_DRAWCALLS:-1}" ] ||
       [ "$actual_picture360_draw_calls" -lt "${IMM_METAL_VALIDATE_MIN_PICTURE360_DRAWCALLS:-1}" ]; then
        echo "$name Metal validation did not exercise the expected picture/360 draw paths" >&2
        echo "actual: pictureDrawCalls=$actual_picture_draw_calls picture360DrawCalls=$actual_picture360_draw_calls" >&2
        tail -n 80 "$log_path" >&2
        exit 1
    fi

    if [ "$name" = "static" ] || [ "${name#repeat_}" != "$name" ]; then
        if [ "$actual_hash" = "5448870274179528411" ]; then
            echo "$name Metal validation matched the old backdrop-only hash; paint draw calls were submitted but not visibly composited" >&2
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
        validate_ppm_capture "$name" "$capture_path" "$capture_width" "$capture_height" "$log_path"
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
        run_case "repeat_$i" 17436244883086101860 38 645802 921600 921600 "" "" "$static_settings_path" "$content_path"
        i=$((i + 1))
    done

    echo "Standalone Metal repeated launch validation passed."
}

if [ "$check_repeat_contract" -eq 1 ]; then
    run_repeat_contract_check
    exit 0
fi

run_case static 17436244883086101860 38 645802 921600 921600 "" "" "$static_settings_path" "$content_path"
run_case pretessellated 15688240155497491850 38 645802 921600 921600 "" "" "$pretessellated_settings_path" "$content_path"

IMM_METAL_VALIDATE_FRAME=12 \
IMM_METAL_VALIDATE_RESIZE_FRAME=2 \
run_case resize - 38 645802 480000 480000 800 600 "$static_settings_path" "$content_path"

echo "Standalone Metal validation passed."
