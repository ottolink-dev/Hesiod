#!/usr/bin/env bash
#
# Headless legacy-decode gate: load every .hsd in the example corpora through a
# built hesiod and fail on any load/compute error.
#
# Replaces the `--compat-check` CLI that was removed with the compat facade. It
# drives the real loader (legacy_converter included) rather than inspecting json,
# so it catches decode regressions the GUI matrix cannot reach.
#
#   usage: scripts/corpus_decode_check.sh [path/to/hesiod]
#          (default: build/bin/hesiod)
#
# Exits non-zero if any file fails for a reason not in the known-benign list.
# Known-benign: files needing assets absent from the repo (CSV/image imports),
# the deprecated MorphologicalGradient warning, and known-malformed fixtures.

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${1:-$ROOT/build/bin/hesiod}"
SHAPE="${SHAPE:-64,64}"          # keep compute cheap; we are testing decode
TIMEOUT="${TIMEOUT:-180}"

# hesiod resolves data/ relative to CWD
RUNDIR="$(dirname "$BIN")"
[ -d "$RUNDIR/data" ] || { echo "no data/ next to $BIN — build first"; exit 2; }

BENIGN='could not load CSV file|could not load image file|is deprecated, use'
KNOWN_BAD_FILES='CoastalErosionDiffusion.hsd|ZeroedEdges.bak.hsd'

mapfile -t FILES < <(find "$ROOT/Hesiod/data/examples" "$ROOT/docs/examples" \
                     -name '*.hsd' 2>/dev/null | sort)
[ ${#FILES[@]} -gt 0 ] || { echo "no corpus files found under $ROOT"; exit 2; }

pass=0; benign=0; fail=0
cd "$RUNDIR" || exit 2

for f in "${FILES[@]}"; do
  base=$(basename "$f")
  log=$(mktemp)
  QT_QPA_PLATFORM=offscreen timeout "$TIMEOUT" "$BIN" --batch "$f" --shape="$SHAPE" \
    > "$log" 2>&1
  rc=$?

  errs=$(grep -aE "\[---E---\]|terminate called|Segmentation|what\(\):|unknown attribute|Required json key" "$log" \
         | grep -aviE "documentation file not found|color_gradients|texture_downloader|opencv_build|data/icons" \
         | sed 's/^\[[^]]*\] *\[[^]]*\] *\[[^]]*\] *//' | sort -u)
  rm -f "$log"

  if [ $rc -eq 0 ] && [ -z "$errs" ]; then
    pass=$((pass + 1))
  elif echo "$base" | grep -qE "$KNOWN_BAD_FILES" || echo "$errs" | grep -qE "$BENIGN"; then
    benign=$((benign + 1))
  else
    fail=$((fail + 1))
    printf 'FAIL %-40s rc=%d %s\n' "$base" "$rc" "$(echo "$errs" | tr '\n' ' ')"
  fi
done

printf 'corpus-decode-check: %d files, %d ok, %d known-benign, %d failures\n' \
       "${#FILES[@]}" "$pass" "$benign" "$fail"
[ "$fail" -eq 0 ]
