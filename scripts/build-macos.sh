#!/usr/bin/env bash
# Native Apple Silicon development build. Install dependencies separately;
# see README.md. Extra arguments are forwarded to CMake's configure step.
set -euo pipefail

if [[ "$(uname -s)" != Darwin || "$(uname -m)" != arm64 ]]; then
  echo "Run this script from a native ARM terminal on an Apple Silicon Mac." >&2
  exit 1
fi

source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${HESIOD_BUILD_DIR:-$source_dir/build/macos}"
brew_prefix="$(brew --prefix)"

# Keep fixes to pinned dependencies reproducible until they land upstream.
apply_patch_once() {
  local module="$1" patch_file="$2"
  if git -C "$source_dir/external/$module" apply --reverse --check "$patch_file" 2>/dev/null; then
    return # Already applied.
  fi
  git -C "$source_dir/external/$module" apply --check "$patch_file"
  git -C "$source_dir/external/$module" apply "$patch_file"
}
apply_patch_once HighMap "$source_dir/scripts/macos/patches/highmap-macos.patch"
# Newer HighMap revisions already supply this helper; retain compatibility
# with the older dependency pinned by main as well as the current dev branch.
if ! grep -Eq 'float[[:space:]]+hash13f[[:space:]]*\(' \
    "$source_dir/external/HighMap/HighMap/src/gpu_opencl/kernels/common/_common_rand.cl"; then
  apply_patch_once HighMap "$source_dir/scripts/macos/patches/highmap-hash13.patch"
fi
apply_patch_once Meta "$source_dir/scripts/macos/patches/meta-sstream.patch"

cmake -S "$source_dir" -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DFETCHCONTENT_UPDATES_DISCONNECTED=ON \
  -DCMAKE_PREFIX_PATH="$brew_prefix;$brew_prefix/opt/qt" \
  -DHESIOD_ENABLE_GENERATE_APP_IMAGE=OFF \
  -DHESIOD_ENABLE_UI_TESTS=OFF \
  "$@"

# Keep peak memory modest on laptops; override when desired.
cmake --build "$build_dir" --target hesiod --parallel "${HESIOD_BUILD_JOBS:-2}"

printf '\nRun Hesiod with:\n  open "%s/bin/Hesiod.app"\n' "$build_dir"
