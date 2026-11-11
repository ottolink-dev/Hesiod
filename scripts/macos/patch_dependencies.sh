#!/usr/bin/env bash
# Sourced by build-macos.sh with source_dir set to the application checkout.
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
# November 2026 HighMap restricts OpenMP flags to C++; preserve that scope
# while splitting Apple's multi-argument flags. Older pins use a separate patch.
openmp_patch="$source_dir/scripts/macos/patches/highmap-openmp.patch"
if git -C "$source_dir/external/HighMap" apply --reverse --check "$openmp_patch" 2>/dev/null ||
    grep -Fq 'add_compile_options($<$<COMPILE_LANGUAGE:CXX>:${OpenMP_CXX_FLAGS}>)' \
      "$source_dir/external/HighMap/CMakeLists.txt"; then
  apply_patch_once HighMap "$openmp_patch"
else
  apply_patch_once HighMap "$source_dir/scripts/macos/patches/highmap-openmp-legacy.patch"
fi
# Newer HighMap revisions already supply this helper; retain compatibility
# with the older dependency pinned by main as well as the current dev branch.
if ! grep -Eq 'float[[:space:]]+hash13f[[:space:]]*\(' \
    "$source_dir/external/HighMap/HighMap/src/gpu_opencl/kernels/common/_common_rand.cl"; then
  apply_patch_once HighMap "$source_dir/scripts/macos/patches/highmap-hash13.patch"
fi
apply_patch_once Meta "$source_dir/scripts/macos/patches/meta-sstream.patch"
