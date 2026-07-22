# Node Reference

Every node Hesiod ships, with its inputs, outputs, parameters and a worked example.
There are **302** of them, so this section is built for looking things up rather than
reading front to back.

## Finding a node

- **[Categories](categories.md)** — the full table, grouped by what a node is *for*.
  Start here when you know the job but not the name.
- **Search** (the box at the top) matches node names and parameter descriptions.
- The left-hand nav lists every node under its category.

If you are still learning the vocabulary, the concept pages are the better entry
point: [heightmaps](../core_concepts/heightmaps.md) for the data nodes pass around,
and [masks & selectors](../core_concepts/masks-selectors.md) for restricting an
operation to part of the terrain.

## How nodes are grouped

| Category | Nodes | What lives here |
| :--- | ---: | :--- |
| Primitive | 61 | Noise, shapes and patterns — where a terrain starts |
| Geometry | 33 | Clouds, paths and the conversions between them |
| Terrain Features | 30 | Selectors, and recognisable landforms |
| Filter | 27 | Smoothing, sharpening and recurve operators |
| Erosion | 17 | Hydraulic and thermal erosion |
| Hydrology | 14 | Flow, flooding and water depth |
| Math | 14 | Arithmetic and range remapping |
| Operator | 14 | Blending, warping and transforms |
| Texture | 13 | Colour, normal maps and compositing |
| Export | 11 | Writing heightmaps, textures and assets to disk |
| Converter | 4 | Moving between heightmaps, masks and kernels |
| Boundaries | 4 | Edge treatment and falloff |
| Routing | 4 | Broadcast, Receive and graph plumbing |
| Debug | 3 | Inspecting what a graph is doing |
| Bridges | 1 | Handing terrain to other tools |
| WIP | 52 | Experimental — expect these to move or change |

## Reading a node page

Each page opens with a description and a screenshot of the node's settings panel,
then lists:

- **Inputs** and **Outputs** — every port with its data type. Types must match to
  connect; see [heightmaps](../core_concepts/heightmaps.md) for what the common ones
  mean.
- **Parameters** — every setting, its type and what it does.
- **Example** — a screenshot with the `.hsd` file behind it, which you can download
  and open directly.

!!! note

    Pages are generated from the running binary, so they describe the nodes as built.
    A node that appears here but not in your build — or vice versa — means your
    Hesiod predates or postdates this documentation.
