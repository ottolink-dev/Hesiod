#!/usr/bin/env bash
# Swap the legacy attributes umbrella include for the Meta compat header.
# brush.cpp is excluded (stays on the legacy library: ArrayAttribute).
set -euo pipefail
cd "$(dirname "$0")/.."

FILES=$(grep -rl '#include "attributes.hpp"' \
  Hesiod/src/model/nodes/nodes_function/ \
  Hesiod/src/model/nodes/post_process.cpp \
  Hesiod/src/model/nodes/pre_process_mask.cpp \
  Hesiod/src/model/nodes/default_noise.cpp \
  | grep -v 'nodes_function/brush.cpp')

for f in $FILES; do
  sed -i 's|#include "attributes.hpp"|#include "hesiod/model/nodes/compat_attributes.hpp"|' "$f"
done
echo "swapped: $(echo "$FILES" | wc -l) files"
