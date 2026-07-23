#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -eq 0 ]; then
  echo "usage: $0 <sdk-package> [<sdk-package>...]" >&2
  exit 2
fi

set +o pipefail
yes | sdkmanager --licenses
sdkmanager_status=${PIPESTATUS[1]}
set -o pipefail
if [ "$sdkmanager_status" -ne 0 ]; then
  exit "$sdkmanager_status"
fi

cleanup_partial_packages() {
  local package
  for package in "$@"; do
    case "$package" in
      ndk\;*)
        local version="${package#ndk;}"
        rm -rf "${ANDROID_SDK_ROOT:-${ANDROID_HOME:-}}/ndk/${version}"
        ;;
    esac
  done
}

for attempt in 1 2 3; do
  if sdkmanager --install "$@"; then
    exit 0
  fi
  if [ "$attempt" -eq 3 ]; then
    break
  fi
  echo "sdkmanager install failed on attempt ${attempt}; cleaning partial NDK packages before retry" >&2
  cleanup_partial_packages "$@"
  sleep $((attempt * 5))
done

echo "sdkmanager install failed after 3 attempts" >&2
exit 1
