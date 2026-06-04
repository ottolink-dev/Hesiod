# Core Concepts

Hesiod builds terrain by connecting **nodes** into a **graph**. Before diving into
the [node reference](../node_reference/) or the [user manual](../user_manual/index.md),
it helps to know a handful of ideas that everything else is built on. Each page
below is short and links down to the deeper documentation when you want detail.

![The Hesiod interface: a node graph on the left produces the rendered terrain on the right.](../images/demo_screenshot.png)

The node graph on the left is where you work; the terrain on the right is what it
produces. Everything in these pages is about what flows through that graph.

## The vocabulary

- **[Heightmaps & virtual arrays](heightmaps.md)** — the data every node reads and
  writes: a grid of elevation values.
- **[Masks & selectors](masks-selectors.md)** — how you restrict an operation to
  *part* of the terrain (steep slopes, low ground, river beds…).
- **[Broadcast & Receive](broadcast-receive.md)** — how terrain moves between
  separate graphs without a wire.
- **[Tiling & overlap](tiling-overlap.md)** — how Hesiod computes large maps in
  pieces, and why the pieces line up.
- **[Glossary](glossary.md)** — quick definitions for every term above.

## Two ways to use Hesiod

The documentation's main path is a **single-terrain** workflow: build one graph,
noise → erosion → colorize → export. The **advanced track** is *worldbuilding* —
stitching several graphs into one large world (see
[Building a "patch of graphs"](../user_manual/patch-of-graphs.md)). The concepts
here underpin both.
