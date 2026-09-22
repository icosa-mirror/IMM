#!/usr/bin/env bash
set -euo pipefail

# Mesa 26.2.3 exits with STATUS_HEAP_CORRUPTION in the hosted Windows
# standalone and Godot Vulkan lanes. Keep the last validated lavapipe build
# pinned until the upstream package is known to work with these runners.
readonly mesa_version="26.1.8-1"
readonly mesa_package="mingw-w64-x86_64-mesa-${mesa_version}-any.pkg.tar.zst"
readonly mesa_url="https://mirror.msys2.org/mingw/mingw64/${mesa_package}"
readonly mesa_sha256="f39e52451d345ad7e59d9221783e1ad3672e8a4b4145f8eea4eeb46c4156015d"
readonly download_path="$(mktemp --suffix=.pkg.tar.zst)"

trap 'rm -f "${download_path}"' EXIT
curl --fail --location --retry 3 --output "${download_path}" "${mesa_url}"
printf '%s  %s\n' "${mesa_sha256}" "${download_path}" | sha256sum --check --strict
pacman --upgrade --noconfirm "${download_path}"
pacman --query mingw-w64-x86_64-mesa
