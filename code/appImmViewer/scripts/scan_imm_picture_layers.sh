#!/usr/bin/env bash
set -eu

usage() {
    echo "usage: $0 [--tool /path/to/ImmPictureScan] [--max-files N] [--skip-files N] [--max-size-mb N] [--per-file-timeout-sec N] [--name-regex REGEX] [--min-cubemap-files N] [--output output.tsv] path-or-file" >&2
    echo "       $0 --summarize-tsv scan-output.tsv..." >&2
}

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
if [ "${1:-}" = "--summarize-tsv" ]; then
    shift
    if [ "$#" -lt 1 ]; then
        usage
        exit 2
    fi
    awk -F '\t' '
        FNR == 1 { next }
        {
            files++
        }
        $2 == "timeout" {
            timeouts++
        }
        $2 == "scan_failed" {
            scanFailed++
        }
        $4 + 0 > 0 {
            pictureFiles++
        }
        $5 + 0 > 0 {
            image2DFiles++
        }
        $6 + 0 > 0 {
            equirect360Files++
        }
        ($7 + 0 > 0 || $8 + 0 > 0 || $9 + 0 > 0) {
            cubemapFiles++
            print "cubemap\t" FILENAME "\t" $0
        }
        END {
            printf "summary files=%d pictureFiles=%d image2DFiles=%d equirect360Files=%d cubemapFiles=%d timeouts=%d scanFailed=%d\n",
                files + 0,
                pictureFiles + 0,
                image2DFiles + 0,
                equirect360Files + 0,
                cubemapFiles + 0,
                timeouts + 0,
                scanFailed + 0
        }
    ' "$@"
    exit 0
fi
tool_path="$repo_root/build/macos/tools/ImmPictureScan"
max_files=200
skip_files=0
max_size_mb=200
per_file_timeout_sec=30
name_regex=
min_cubemap_files=0
output_path="$repo_root/build/macos/imm-picture-layers.tsv"

while [ $# -gt 0 ]; do
    case "$1" in
        --tool)
            shift
            [ $# -gt 0 ] || { usage; exit 2; }
            tool_path=$1
            ;;
        --max-files)
            shift
            [ $# -gt 0 ] || { usage; exit 2; }
            max_files=$1
            ;;
        --skip-files)
            shift
            [ $# -gt 0 ] || { usage; exit 2; }
            skip_files=$1
            ;;
        --max-size-mb)
            shift
            [ $# -gt 0 ] || { usage; exit 2; }
            max_size_mb=$1
            ;;
        --per-file-timeout-sec)
            shift
            [ $# -gt 0 ] || { usage; exit 2; }
            per_file_timeout_sec=$1
            ;;
        --name-regex)
            shift
            [ $# -gt 0 ] || { usage; exit 2; }
            name_regex=$1
            ;;
        --min-cubemap-files)
            shift
            [ $# -gt 0 ] || { usage; exit 2; }
            min_cubemap_files=$1
            ;;
        --output)
            shift
            [ $# -gt 0 ] || { usage; exit 2; }
            output_path=$1
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        --*)
            usage
            exit 2
            ;;
        *)
            break
            ;;
    esac
    shift
done

if [ $# -ne 1 ]; then
    usage
    exit 2
fi

input_path=$1
if [ ! -x "$tool_path" ]; then
    echo "ImmPictureScan is not executable: $tool_path" >&2
    exit 1
fi
if [ ! -e "$input_path" ]; then
    echo "input path does not exist: $input_path" >&2
    exit 1
fi
case "$max_files" in
    ''|*[!0-9]*)
        echo "--max-files must be a non-negative integer" >&2
        exit 2
        ;;
esac
case "$skip_files" in
    ''|*[!0-9]*)
        echo "--skip-files must be a non-negative integer" >&2
        exit 2
        ;;
esac
case "$max_size_mb" in
    ''|*[!0-9]*)
        echo "--max-size-mb must be a non-negative integer" >&2
        exit 2
        ;;
esac
case "$per_file_timeout_sec" in
    ''|*[!0-9]*)
        echo "--per-file-timeout-sec must be a non-negative integer" >&2
        exit 2
        ;;
esac
case "$min_cubemap_files" in
    ''|*[!0-9]*)
        echo "--min-cubemap-files must be a non-negative integer" >&2
        exit 2
        ;;
esac

mkdir -p "$(dirname -- "$output_path")"
max_size_bytes=$((max_size_mb * 1024 * 1024))
printf 'path\tstatus\tlayers\tpictures\timage2D\tequirect360\tcubemap360\tcubemapCross\tcubemapVstrip\tdetails\n' > "$output_path"

count=0
seen=0
failures=0
timeouts=0
tmp_row=$(mktemp "${TMPDIR:-/tmp}/imm-picture-row.XXXXXX")
tmp_list=$(mktemp "${TMPDIR:-/tmp}/imm-picture-list.XXXXXX")
cleanup() {
    rm -f "$tmp_row" "$tmp_list"
}
trap cleanup EXIT INT TERM

sanitize_field() {
    printf '%s' "$1" | tr '\t\r\n' '   '
}

scan_one() {
    imm_path=$1
    if [ -n "$name_regex" ] && [[ ! "$imm_path" =~ $name_regex ]]; then
        return 0
    fi
    seen=$((seen + 1))
    if [ "$seen" -le "$skip_files" ]; then
        return 0
    fi
    if [ "$max_files" -gt 0 ] && [ "$count" -ge "$max_files" ]; then
        return 1
    fi
    count=$((count + 1))
    rm -f "$tmp_row"
    set +e
    "$tool_path" --no-header "$imm_path" > "$tmp_row" 2>/dev/null &
    scan_pid=$!
    start_time=$(date +%s)
    scan_status=0
    while kill -0 "$scan_pid" 2>/dev/null; do
        now=$(date +%s)
        if [ $((now - start_time)) -ge "$per_file_timeout_sec" ]; then
            kill "$scan_pid" 2>/dev/null || true
            wait "$scan_pid" 2>/dev/null || true
            scan_status=124
            break
        fi
        sleep 1
    done
    if [ "$scan_status" -eq 0 ]; then
        wait "$scan_pid"
        scan_status=$?
    fi
    set -e

    if [ "$scan_status" -eq 124 ]; then
        printf '%s\ttimeout\t0\t0\t0\t0\t0\t0\t0\t\n' "$(sanitize_field "$imm_path")" >> "$output_path"
        timeouts=$((timeouts + 1))
        failures=$((failures + 1))
    elif [ -s "$tmp_row" ]; then
        cat "$tmp_row" >> "$output_path"
        if [ "$scan_status" -ne 0 ]; then
            failures=$((failures + 1))
        fi
    else
        printf '%s\tscan_failed\t0\t0\t0\t0\t0\t0\t0\t\n' "$(sanitize_field "$imm_path")" >> "$output_path"
        failures=$((failures + 1))
    fi
    return 0
}

if [ -f "$input_path" ]; then
    scan_one "$input_path" || true
else
    find "$input_path" -type f \( -iname '*.imm' -o -iname '*.IMM' \) -size -"${max_size_bytes}"c -print0 | sort -z > "$tmp_list"
    while IFS= read -r -d '' imm_path; do
        scan_one "$imm_path" || break
    done < "$tmp_list"
fi

image2d_count=$(awk -F '\t' 'NR > 1 && $5+0 > 0 { count++ } END { print count+0 }' "$output_path")
equirect_count=$(awk -F '\t' 'NR > 1 && $6+0 > 0 { count++ } END { print count+0 }' "$output_path")
cubemap_count=$(awk -F '\t' 'NR > 1 && ($7+0 > 0 || $8+0 > 0 || $9+0 > 0) { count++ } END { print count+0 }' "$output_path")
picture_count=$(awk -F '\t' 'NR > 1 && $4+0 > 0 { count++ } END { print count+0 }' "$output_path")
if [ -n "$name_regex" ]; then
    echo "Scanned $count IMM files matching '$name_regex' after skipping $skip_files; pictureFiles=$picture_count image2DFiles=$image2d_count equirect360Files=$equirect_count cubemapFiles=$cubemap_count failures=$failures timeouts=$timeouts output=$output_path"
else
    echo "Scanned $count IMM files after skipping $skip_files; pictureFiles=$picture_count image2DFiles=$image2d_count equirect360Files=$equirect_count cubemapFiles=$cubemap_count failures=$failures timeouts=$timeouts output=$output_path"
fi

if [ "$cubemap_count" -lt "$min_cubemap_files" ]; then
    echo "IMM picture metadata scan found cubemapFiles=$cubemap_count, expected at least $min_cubemap_files" >&2
    exit 1
fi
