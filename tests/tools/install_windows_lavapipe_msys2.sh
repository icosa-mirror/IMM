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
readonly llvm_package="mingw-w64-x86_64-llvm-libs-22.1.8-3-any.pkg.tar.zst"
readonly llvm_url="https://mirror.msys2.org/mingw/mingw64/${llvm_package}"
readonly llvm_sha256="ab50a733cbafd24e5180c00dede15014ecb7c133b836ce59262c22c27d46d659"
readonly llvm_download_path="$(mktemp --suffix=.pkg.tar.zst)"

trap 'rm -f "${download_path}" "${llvm_download_path}"' EXIT
curl --fail --location --retry 3 --output "${download_path}" "${mesa_url}"
printf '%s  %s\n' "${mesa_sha256}" "${download_path}" | sha256sum --check --strict
pacman --upgrade --noconfirm "${download_path}"
pacman --query mingw-w64-x86_64-mesa

# This Mesa binary imports libLLVM-22.dll. Current llvm-libs packages provide
# LLVM 23, and Mesa's unversioned package dependency does not retain LLVM 22.
# Keep the old ABI alongside the current toolchain, without downgrading packages.
curl --fail --location --retry 3 --output "${llvm_download_path}" "${llvm_url}"
printf '%s  %s\n' "${llvm_sha256}" "${llvm_download_path}" | sha256sum --check --strict
tar -xf "${llvm_download_path}" -C / mingw64/bin/libLLVM-22.dll
test -f /mingw64/bin/libLLVM-22.dll
printf 'Pinned Mesa LLVM runtime: /mingw64/bin/libLLVM-22.dll\n'
