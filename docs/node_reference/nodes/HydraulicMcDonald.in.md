## Usage

`HydraulicMcDonald` is a GPU-only, flow-coupled particle erosion model. Unlike
the other hydraulic operators it is *physically* parameterized: the heightmap is
interpreted as a **World Extent** × **World Extent** km domain whose `[0, 1]`
elevation span maps to **Height Scale** km. The defaults describe a 40 × 40 km
region with 4 km of relief; change these to keep behaviour consistent when you
change resolution.

Keep **Multiscale** enabled for anything above ~512 px. Single-scale erosion is
numerically unstable and diverges after a few hundred steps at 1024² and above;
the multiscale schedule (`Steps Per Level`, coarsest first) erodes on a halving
resolution ladder and is the stable path to high-resolution terrain.

Outputs:

- **output** — the eroded surface (bedrock + deposited sediment).
- **sediment** — the final sediment layer, useful as a deposition mask.
- **discharge** — the water discharge field, usable directly as a river / water
  mask.

The model couples particles through a shared flow field, so a single tile
(`tiling = 1 × 1`) yields the most coherent drainage network; tiled runs erode
each tile as its own scaled world and stitch the elevations at the seams.
