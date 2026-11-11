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

source "$source_dir/scripts/macos/patch_dependencies.sh"

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
