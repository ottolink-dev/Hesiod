# Glossary

**Batch / headless mode** — running a graph from the command line with no GUI to
compute and export a terrain (`hesiod --batch …`). See [Tiling & overlap](tiling-overlap.md).

**Broadcast** — a node that publishes its input heightmap under a tag so other
graphs can receive it. See [Broadcast & Receive](broadcast-receive.md).

**Graph** — a network of connected nodes that together produce a terrain.

**Heightmap** — a 2-D grid of elevation values; the data flowing between nodes. See
[Heightmaps & virtual arrays](heightmaps.md).

**`.hsd` file** — a saved Hesiod project (its graph and parameters).

**Mask** — a heightmap used as a per-cell weight to scope where an
operation applies. See [Masks & selectors](masks-selectors.md).

**Node** — a single operation in a graph (a primitive, filter, erosion step, …).

**Normalized space** — the unit square `[0,1] × [0,1]` all coordinates use until
export. See [Coordinate system](../user_manual/coordinate_system/index.md).

**Overlap** — a shared margin between tiles that prevents seams. See
[Tiling & overlap](tiling-overlap.md).

**Receive** — a node that outputs a heightmap broadcast under a chosen tag,
re-projected into the receiving graph's frame. See [Broadcast & Receive](broadcast-receive.md).

**Selector** — a node that builds a mask from a terrain property (slope, elevation,
…). See [Masks & selectors](masks-selectors.md).

**Tile** — one piece of a heightmap computed separately. See [Tiling & overlap](tiling-overlap.md).

**Virtual array** — Hesiod's representation of a heightmap at a node port
(`VirtualArray`). See [Heightmaps & virtual arrays](heightmaps.md).
