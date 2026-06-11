#!/usr/bin/env bash
set -euo pipefail

cmdline_tools_version="${ANDROID_CMDLINE_TOOLS_VERSION:-14742923}"
sdk_root="${ANDROID_SDK_ROOT:-${RUNNER_TEMP:-$HOME}/android-sdk}"
download_path="${RUNNER_TEMP:-/tmp}/commandlinetools-linux-${cmdline_tools_version}_latest.zip"
extract_root="${RUNNER_TEMP:-/tmp}/android-cmdline-tools"

rm -rf "$extract_root" "$sdk_root/cmdline-tools/latest"
mkdir -p "$extract_root" "$sdk_root/cmdline-tools"

curl -fL --retry 5 --retry-delay 2 --retry-all-errors \
  -o "$download_path" \
  "https://dl.google.com/android/repository/commandlinetools-linux-${cmdline_tools_version}_latest.zip"

unzip -q "$download_path" -d "$extract_root"
mv "$extract_root/cmdline-tools" "$sdk_root/cmdline-tools/latest"
mkdir -p "$HOME/.android"
touch "$HOME/.android/repositories.cfg"

{
  echo "ANDROID_HOME=$sdk_root"
  echo "ANDROID_SDK_ROOT=$sdk_root"
} >> "$GITHUB_ENV"

{
  echo "$sdk_root/cmdline-tools/latest/bin"
  echo "$sdk_root/platform-tools"
  echo "$sdk_root/emulator"
} >> "$GITHUB_PATH"

"$sdk_root/cmdline-tools/latest/bin/sdkmanager" --version
