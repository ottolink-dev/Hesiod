#!/bin/bash
# Regenerate the node-reference documentation end to end:
#   inventory -> refresh catalog -> (re)generate example graphs -> render
#   snapshots -> collect images/examples -> rebuild markdown.
# Run from the repository root with a built binary in build/bin.
set -e

# 1. refresh the node catalog (and per-node settings screenshots) from the binary
cd build
./bin/hesiod --inventory
cd ..
cp build/node_documentation_stub.json Hesiod/data/node_documentation.json

# 2. (re)generate example graphs for every non-WIP node lacking a curated one,
#    then sync them into the build's data copy so --snapshot can load them
python3 scripts/gen_node_examples.py
cp -u Hesiod/data/examples/*.hsd build/data/examples/. 2>/dev/null || true
cp -u Hesiod/data/examples/*.hsd build/bin/data/examples/. 2>/dev/null || true

# 3. render the node-graph example snapshots
cd build
./bin/hesiod --snapshot
cd ..

# 4. collect generated images + example files into the docs tree
cp -u build/*_settings.png docs/images/nodes/.
rm -f build/*_settings.png

cp -u build/*_hsd_example.png docs/images/nodes/.
rm -f build/*_hsd_example.png

cp -u Hesiod/data/examples/*.hsd docs/examples/.

# 5. rebuild the node-reference markdown
python3 scripts/generate_node_reference.py
