#!/bin/sh
set -eu

usage() {
    echo "usage: $0 [appImmViewerMetal-path] <imm-file-or-directory>..." >&2
    echo "" >&2
    echo "Environment:" >&2
    echo "  IMM_METAL_SWEEP_OUTPUT       TSV summary path (default: build/macos/metal-content-sweep.tsv)" >&2
    echo "  IMM_METAL_SWEEP_LOG_DIR      Per-file log directory (default: build/macos/metal-content-sweep-logs)" >&2
    echo "  IMM_METAL_SWEEP_CAPTURE_DIR  Optional per-file PNG capture directory" >&2
    echo "  IMM_METAL_SWEEP_MAX_FRAME    Validation max frame (default: 300)" >&2
    echo "  IMM_METAL_SWEEP_MAX_FILES    Optional maximum number of rendered IMM files" >&2
    echo "  IMM_METAL_SWEEP_MAX_BYTES    Optional maximum file size to render, in bytes" >&2
    echo "  IMM_METAL_SWEEP_MIN_PASSED   Minimum passed nonblank renders required (default: 1)" >&2
    echo "  IMM_METAL_SWEEP_FAIL_ON_FAILED  Set to 1 to fail if any nonblank validation failure occurs (default: 0)" >&2
}

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
default_app="$repo_root/build/macos/viewer/appImmViewerMetal.app/Contents/MacOS/appImmViewerMetal"

if [ "${1:-}" = "--help" ] || [ "${1:-}" = "-h" ]; then
    usage
    exit 0
fi

app_path=$default_app
if [ $# -gt 0 ] && [ -x "$1" ]; then
    app_path=$1
    shift
fi

if [ $# -lt 1 ]; then
    usage
    exit 1
fi

canonical_file_path() {
    path=$1
    dir=$(dirname -- "$path")
    base=$(basename -- "$path")
    printf '%s/%s\n' "$(CDPATH= cd -- "$dir" && pwd)" "$base"
}

if [ ! -x "$app_path" ]; then
    echo "appImmViewerMetal is not executable: $app_path" >&2
    exit 1
fi
app_path=$(canonical_file_path "$app_path")

output_path=${IMM_METAL_SWEEP_OUTPUT:-"$repo_root/build/macos/metal-content-sweep.tsv"}
log_dir=${IMM_METAL_SWEEP_LOG_DIR:-"$repo_root/build/macos/metal-content-sweep-logs"}
capture_dir=${IMM_METAL_SWEEP_CAPTURE_DIR:-}
max_frame=${IMM_METAL_SWEEP_MAX_FRAME:-300}
max_files=${IMM_METAL_SWEEP_MAX_FILES:-0}
max_bytes=${IMM_METAL_SWEEP_MAX_BYTES:-0}
min_passed=${IMM_METAL_SWEEP_MIN_PASSED:-1}
fail_on_failed=${IMM_METAL_SWEEP_FAIL_ON_FAILED:-0}

case "$max_files" in
    ''|*[!0-9]*)
        echo "IMM_METAL_SWEEP_MAX_FILES must be a non-negative integer" >&2
        exit 1
        ;;
esac
case "$max_bytes" in
    ''|*[!0-9]*)
        echo "IMM_METAL_SWEEP_MAX_BYTES must be a non-negative integer" >&2
        exit 1
        ;;
esac
case "$min_passed" in
    ''|*[!0-9]*)
        echo "IMM_METAL_SWEEP_MIN_PASSED must be a non-negative integer" >&2
        exit 1
        ;;
esac
case "$fail_on_failed" in
    0|1) ;;
    *)
        echo "IMM_METAL_SWEEP_FAIL_ON_FAILED must be 0 or 1" >&2
        exit 1
        ;;
esac

mkdir -p "$(dirname -- "$output_path")" "$log_dir"
if [ -n "$capture_dir" ]; then
    mkdir -p "$capture_dir"
    capture_dir=$(CDPATH= cd -- "$capture_dir" && pwd)
fi
log_dir=$(CDPATH= cd -- "$log_dir" && pwd)

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/imm-metal-sweep.XXXXXX")
cleanup() {
    rm -rf "$tmp_dir"
}
trap cleanup EXIT INT TERM

content_list="$tmp_dir/content-files.txt"
: > "$content_list"

for target in "$@"; do
    if [ -d "$target" ]; then
        find "$target" -type f \( -iname '*.imm' -o -iname '*.IMM' \) -print >> "$content_list"
    elif [ -f "$target" ]; then
        case "$target" in
            *.imm|*.IMM) printf '%s\n' "$target" >> "$content_list" ;;
            *) echo "skipping non-IMM file: $target" >&2 ;;
        esac
    else
        echo "content path not found: $target" >&2
    fi
done

sort -u "$content_list" -o "$content_list"

file_size_bytes() {
    stat -f '%z' "$1" 2>/dev/null || stat -c '%s' "$1" 2>/dev/null || wc -c < "$1" | tr -d ' '
}

printf 'path\tsizeBytes\tstatus\tframe\tpixels\tnonZero\tdrawCalls\tpaintDrawCalls\tpictureDrawCalls\tpicture2DDrawCalls\tpicture360DrawCalls\tpicture360EquirectDrawCalls\tpicture360CubemapDrawCalls\tmodelDrawCalls\ttriangles\tcapture\tlog\n' > "$output_path"

index=0
rendered=0
passed=0
blank=0
failed=0
skipped_size=0
skipped_limit=0
while IFS= read -r content_path; do
    [ -n "$content_path" ] || continue
    index=$((index + 1))
    size_bytes=$(file_size_bytes "$content_path")
    if [ "$max_bytes" -gt 0 ] && [ "$size_bytes" -gt "$max_bytes" ]; then
        skipped_size=$((skipped_size + 1))
        printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
            "$content_path" \
            "$size_bytes" \
            "skipped_size" \
            "" "" "" "" "" "" "" "" "" "" "" "" "" "" >> "$output_path"
        continue
    fi
    if [ "$max_files" -gt 0 ] && [ "$rendered" -ge "$max_files" ]; then
        skipped_limit=$((skipped_limit + 1))
        printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
            "$content_path" \
            "$size_bytes" \
            "skipped_limit" \
            "" "" "" "" "" "" "" "" "" "" "" "" "" "" >> "$output_path"
        continue
    fi
    rendered=$((rendered + 1))

    base=$(basename -- "$content_path")
    safe=$(printf '%03d-%s' "$rendered" "$base" | tr -c 'A-Za-z0-9_.-' '_')
    log_path="$log_dir/$safe.log"
    player_log_path="$log_dir/$safe.player.log"
    capture_path=
    if [ -n "$capture_dir" ]; then
        capture_path="$capture_dir/$safe.png"
    fi

    echo "Running Metal content sweep: $content_path"

    set +e
    IMM_METAL_VALIDATE_FRAME=1 \
    IMM_METAL_VALIDATE_MAX_FRAME="$max_frame" \
    IMM_METAL_VALIDATE_MIN_NONZERO=1 \
    IMM_METAL_VALIDATE_MIN_DRAWCALLS=0 \
    IMM_METAL_VALIDATE_MIN_PICTURE_DRAWCALLS=0 \
    IMM_METAL_VALIDATE_MIN_PICTURE360_DRAWCALLS=0 \
    IMM_METAL_VALIDATE_MIN_TRIANGLES=0 \
    IMM_METAL_VALIDATE_HELPER_DRAWS=0 \
    IMM_METAL_EXIT_AFTER_VALIDATE=1 \
    IMM_METAL_VALIDATE_CAPTURE_PATH="$capture_path" \
    IMM_METAL_LOG_PATH="$player_log_path" \
        "$app_path" "$content_path" > "$log_path" 2>&1
    app_status=$?
    set -e

    validation_line=$(grep "IMM Metal validation:" "$log_path" | tail -n 1 || true)
    failure_line=$(grep "IMM Metal validation failed:" "$log_path" | tail -n 1 || true)
    status=failed
    line=$failure_line
    if [ "$app_status" -eq 0 ] && [ -n "$validation_line" ]; then
        status=passed
        line=$validation_line
    elif [ -n "$failure_line" ]; then
        status=failed
        line=$failure_line
    fi

    frame=$(printf '%s\n' "$line" | sed -n 's/.* frame=\([0-9][0-9]*\) .*/\1/p')
    pixels=$(printf '%s\n' "$line" | sed -n 's/.* pixels=\([0-9][0-9]*\) .*/\1/p')
    nonzero=$(printf '%s\n' "$line" | sed -n 's/.* nonZero=\([0-9][0-9]*\) .*/\1/p')
    draw_calls=$(printf '%s\n' "$line" | sed -n 's/.* drawCalls=\([0-9][0-9]*\) .*/\1/p')
    paint_draw_calls=$(printf '%s\n' "$line" | sed -n 's/.* paintDrawCalls=\([0-9][0-9]*\) .*/\1/p')
    picture_draw_calls=$(printf '%s\n' "$line" | sed -n 's/.* pictureDrawCalls=\([0-9][0-9]*\) .*/\1/p')
    picture2d_draw_calls=$(printf '%s\n' "$line" | sed -n 's/.* picture2DDrawCalls=\([0-9][0-9]*\) .*/\1/p')
    picture360_draw_calls=$(printf '%s\n' "$line" | sed -n 's/.* picture360DrawCalls=\([0-9][0-9]*\) .*/\1/p')
    picture360_equirect_draw_calls=$(printf '%s\n' "$line" | sed -n 's/.* picture360EquirectDrawCalls=\([0-9][0-9]*\) .*/\1/p')
    picture360_cubemap_draw_calls=$(printf '%s\n' "$line" | sed -n 's/.* picture360CubemapDrawCalls=\([0-9][0-9]*\) .*/\1/p')
    model_draw_calls=$(printf '%s\n' "$line" | sed -n 's/.* modelDrawCalls=\([0-9][0-9]*\) .*/\1/p')
    triangles=$(printf '%s\n' "$line" | sed -n 's/.* triangles=\([0-9][0-9]*\) .*/\1/p')

    if [ "$status" = "failed" ] &&
       [ "${nonzero:-}" = "0" ] &&
       [ "${draw_calls:-}" = "0" ] &&
       [ "${triangles:-}" = "0" ]; then
        status=blank
    fi

    case "$status" in
        passed) passed=$((passed + 1)) ;;
        blank) blank=$((blank + 1)) ;;
        failed) failed=$((failed + 1)) ;;
    esac

    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$content_path" \
        "$size_bytes" \
        "$status" \
        "${frame:-}" \
        "${pixels:-}" \
        "${nonzero:-}" \
        "${draw_calls:-}" \
        "${paint_draw_calls:-}" \
        "${picture_draw_calls:-}" \
        "${picture2d_draw_calls:-}" \
        "${picture360_draw_calls:-}" \
        "${picture360_equirect_draw_calls:-}" \
        "${picture360_cubemap_draw_calls:-}" \
        "${model_draw_calls:-}" \
        "${triangles:-}" \
        "$capture_path" \
        "$log_path" >> "$output_path"
done < "$content_list"

echo "Metal content sweep summary: $output_path"
echo "Metal content sweep counts: passed=$passed blank=$blank failed=$failed skipped_size=$skipped_size skipped_limit=$skipped_limit"

if [ "$passed" -lt "$min_passed" ]; then
    echo "Metal content sweep did not reach IMM_METAL_SWEEP_MIN_PASSED=$min_passed: passed=$passed" >&2
    exit 1
fi

if [ "$fail_on_failed" -eq 1 ] && [ "$failed" -gt 0 ]; then
    echo "Metal content sweep found failed nonblank validation rows: failed=$failed" >&2
    exit 1
fi
