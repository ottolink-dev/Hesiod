# Building a "patch of graphs" (tiled equirectangular worlds)

This guide explains how to author a large heightmap out of **several graphs laid
out side by side in one shared world space** — a "patch" or mosaic — instead of
stretching a single graph. It is aimed at producing a **2:1 equirectangular
heightmap** (e.g. for Azgaar's Fantasy Map Generator) with:

- multiple, individually authored landmasses,
- land kept off the poles,
- continuous terrain **across every seam**, and
- **longitude wrap** (the left edge matches the right edge).

Everything below maps directly to how Hesiod implements it
(`GraphManager`, `CoordFrame`, the `Broadcast`/`Receive` nodes, and
`export_flatten`).

---

## 1. The mechanics (read this first)

Three facts drive the whole workflow.

### a. Every graph is a rectangle in one shared world space

Internally each graph (`GraphNode`) **is** a coordinate frame
(`hmap::CoordFrame`): it has an **origin `(x, y)`**, a **size `(w, h)`**, and a
rotation. The pixels a graph computes live inside that rectangle. The
**Graph Layout Manager** is where you place those rectangles relative to each
other.

> A graph's **pixel resolution** (its *shape*, e.g. `1024×1024`) is independent
> of its **world size** (e.g. `1.0 × 1.0`). Use **square pixel shapes** for
> tiles — a single *rectangular* graph stretches its noise; square tiles in a
> 2:1 layout do not.

> **The graph-config shape control is square-only.** The slider is hardcoded to
> powers of two — `512`, `1024`, `2048`, `4096` — and always sets `NxN`. You
> *cannot* (and should not) enter `1024x512`. The 2:1 equirectangular ratio does
> **not** come from pixel shape; it comes from (a) the **frame sizes** you set in
> the Layout Manager and (b) the **Flatten/export shape**. See Steps 1, 2 and 5.

### b. Continuity across seams = Broadcast -> Receive

```
   graph "base"  (lower layer)          graph "tile_00" (upper layer)
  +-----------------------+            +-----------------------+
  |  ... continents ...   |            |   [Receive: "base"] --+--> + detail --> out
  |          |            |            |        ^              |
  |     [Broadcast] ......:............:........:  resamples the base field
  |        tag = "base"   |   shared   |   over THIS tile's footprint
  +-----------------------+   world    +-----------------------+
                              space
```

A `Broadcast` node publishes `(tag, its frame, its data)`. A `Receive` node in
**another** graph picks that tag and **resamples the source over the world-space
overlap of the two frames**. So two frames that touch or overlap read the *exact
same field* across the boundary — that is what makes the seam seamless.

> **Direction is one-way.** Broadcasts only reach graphs that are **lower in
> the layer list** (below the broadcaster). So your shared/base layer must sit at
> the **top** of the list, and the per-region tiles sit below it.

### c. Export stitches all frames into one image

The **Flatten/export** button computes the global bounding box of *all* frames,
drops each graph's selected output into it, and writes:

- `export.png` — a **16-bit grayscale** heightmap (this is what you feed to Azgaar), and
- `export_preview.png` — a terrain-coloured hillshade, for eyeballing continuity.

**The output's aspect ratio is the aspect ratio of the union of your frames.**
A 2:1 equirectangular map therefore just means *the frames together form a 2:1
rectangle*.

---

## 2. The Graph Layout Manager window

Open it with **View -> Graph Layout Manager**.

```
 +======================================================================+
 |  Hesiod - GraphManager                                          _ [] X |
 +----------------------------------+-----------------------------------+
 |                                  |  Graph list (BOTTOM = upper layer)|
 |        FRAME CANVAS              | +-------------------------------+ |
 |   (world space, Y points UP)     | | base      [bg-tag v]  [thumb] | |
 |                                  | | tile_00   [bg-tag v]  [thumb] | |
 |    +--------+ +--------+         | | tile_01   [bg-tag v]  [thumb] | |
 |    |tile_00 | |tile_01 |        <--->| tile_02   [bg-tag v]  [thumb] | |
 |    +--------+ +--------+         | | tile_03   [bg-tag v]  [thumb] | |
 |    |  base (spans everything) |  | +-------------------------------+ |
 |    +-----------------------+    |                                   |
 |                                  | [ New ] [ Zoom ] [Flatten/export]|
 |   drag to move, wheel = zoom     | [        Apply (red when dirty) ] |
 +----------------------------------+-----------------------------------+
```

- **Left = frame canvas.** Each rectangle is a graph. **Drag** to move, **mouse
  wheel** to zoom, **left-drag empty space** to pan. The Y axis points **up**.
- **Right = ordered layer list.** **Bottom of the list = top layer.** Drag rows to
  reorder. Each row has a **background-tag combobox** — this selects which output
  that graph contributes to the export.
- **New** — add a graph. **Zoom** — fit all frames in view. **Flatten/export** —
  stitch and write the image. **Apply** — commit frame moves (turns **red** when
  there are uncommitted changes; you *must* click it after moving frames).

---

## 3. Step-by-step

### Step 0 — Open the manager

**View -> Graph Layout Manager.**

### Step 1 — Create the shared base graph (bottom layer)

This graph guarantees global continuity, places the continents, and carries the
longitude wrap.

1. Click **New** -> name it `base`.

   ```
   +-----------------------------+      +-------------------------------------+
   |  Enter new graph ID         |  ->  |  Hesiod - Model configuration       |
   |  [ base                  ]  |      |  Sxshape  [==O=========]  1024x1024 | <- square only
   |          [ OK ] [Cancel]    |      |  Tiling   [==O=========]  4x4        |    (2^n: 512..4096)
   +-----------------------------+      |  Overlap  [======O=====]  0.50       |
                                        |  Cache Data on Disk [ ]              |
                                        |  CPU  [ Distributed         v ]      |
                                        |  GPU  [ Single array        v ]      |
                                        |                 [ OK ] [ Cancel ]    |
                                        +-------------------------------------+
   ```

   > The **Sxshape** slider only produces **square** sizes (`512`, `1024`,
   > `2048`, `4096`). Leave `base` at **`1024x1024`** — the 2:1 shape comes from
   > the *frame* (Step 2) and the *export* (Step 5), not from here.

2. Open `base` in the editor (double-click the row, or right-click ->
   *Show in graph editor*).
3. Build the continents **left to right**, each node feeding the next. Start with
   the 3-node minimum below, get it working, *then* add Parts 2-4.

#### Part 1 — The minimum that works (3 nodes)

```
   [Noise FBM] ---> [Make Periodic] ---> [Broadcast]
```

| Node | What it is for | What to set |
|------|----------------|-------------|
| **Noise FBM** (`noise_fbm`) | Makes the big blobby shapes that become land and ocean. | **Spatial Frequency** = `(4, 2)`. Low numbers = few big continents. X is double Y to undo the 2x1 stretch (see note at the end). |
| **Make Periodic** (`make_periodic`) | Makes the **left edge match the right edge** so the map wraps around (longitude). | **periodicity_type** = `X-only`. |
| **Broadcast** (`broadcast`) | Publishes this terrain so the tile graphs can read it. Its `tag` is filled in automatically. | nothing - just note the tag. |

Wire `Noise FBM -> Make Periodic -> Broadcast`. That is a complete, working base.
If you Flatten now you will see wrapping blobby terrain.

#### Part 2 — Add sea level & contrast (1 node)

```
   [Noise FBM] -> [Make Periodic] -> [Remap] -> [Broadcast]
```

| Node | What it is for | What to set |
|------|----------------|-------------|
| **Remap** (`remap`) | Stretches the brightness to the full black->white range so Azgaar reads a clean elevation span. Also where you bias how much is ocean. | leave at `0 -> 1` to start. |

Insert `Remap` just before `Broadcast`.

#### Part 3 — Keep land off the poles (2 nodes)

The top and bottom of an equirectangular map are the poles - you want them to be
ocean. Do that by **multiplying** your terrain by a mask that is bright in the
middle and dark at top/bottom.

```
   [Noise FBM] -> [Make Periodic] --+
                                    +--> [Blend: multiply] -> [Remap] -> [Broadcast]
   [Wave Sine] --------------------+
```

| Node | What it is for | What to set |
|------|----------------|-------------|
| **Wave Sine** (`wave_sine`) | Makes a single bright horizontal band down the middle (dark at top & bottom) = your "stay away from the poles" mask. | **angle** = `90`, **kw** = `0.5`. Tune until only the middle is bright. |
| **Blend** (`blend`) | Multiplies the terrain by the mask, so the poles get pushed down to ocean. | **blending_method** = `multiply`. Connect your **terrain** (the `Make Periodic` output - i.e. the land field built so far) to `input 1`, and **Wave Sine** to `input 2`. |

> **"Terrain" = your land field, not a node named Terrain.** Up to this point that
> wire is the `Make Periodic` output (Noise FBM, plus any hand-placed continents
> from Part 4). The mask must be bright in the **middle** and dark at **both** top
> and bottom; if your Wave Sine preview is a one-sided gradient, tune `kw`/`angle`
> until you see a bright horizontal band with dark poles - otherwise one pole gets
> boosted instead of sunk.

#### Part 4 — Hand-place continents (optional - the per-landmass control)

If you want to *choose* where continents go instead of letting noise decide, add
a cloud of control points and blend it in.

```
   [Cloud Random] -> [Cloud to Array Interp] --+
                                               +--> [Blend: add] -> (continue to Remap -> Broadcast)
   [Noise FBM] -> [Make Periodic] -------------+
```

| Node | What it is for | What to set |
|------|----------------|-------------|
| **Cloud Random** (`cloud_random`) | Drops a handful of points - **each point is one continent's centre**. You can move them and set each one's height/weight. | start with ~4-6 points. |
| **Cloud to Array Interp** (`cloud_to_array_interp`) | Turns those points into smooth elevation humps (one bump per continent). | defaults. |
| **Blend** (`blend`) | Adds the continent humps onto the noise so land appears where your points are. | **blending_method** = `add`. |

> **Why "anisotropic Spatial Frequency"?** That just means the `(4, 2)` on Noise
> FBM - the X number is twice the Y number. Since the base's square pixels get
> stretched 2x horizontally by the 2x1 frame (Step 2), doubling the X frequency
> cancels the stretch so the blobs come out round. The tiles do **not** need this
> - they are already square.

4. However far you take it, **end the chain with a `Broadcast` node**. Its `tag`
   is assigned automatically - note it (you will pick it inside each tile).

### Step 2 — Place the base frame to cover the whole world

On the canvas, select the `base` rectangle and size it to your **full world box**.
For 2:1, make it twice as wide as tall:

```
 origin (0,0), size (2,1)            <-- 2:1 world box

 +---------------------------------------------+
 |                                             |
 |                  base                       |   1 unit tall
 |                                             |
 +---------------------------------------------+
                  2 units wide
```

Click the red **Apply** button to commit the frame.

### Step 3 — Add the per-region (tile) graphs on top

For each landmass/region you want to author separately:

1. **New** -> name it `tile_00`, `tile_01`, ... Give tiles a **square** pixel
   shape (e.g. `1024×1024`).
2. **Keep tiles below `base` in the list** (drag if needed). Below = can receive
   from base.
3. In the tile's editor, drop a **`Receive`** node and pick the base's **tag**
   from its dropdown:

   ```
   [Receive: tag = "base" v] --> [add local detail: noise / erosion / masks] --> [out]
   ```

   The Receive node hands you the base field sampled exactly for this tile's
   footprint — including the shared values along every edge it touches.
4. Layer your local character on top, then route the result to the output you
   will export.

### Step 4 — Lay out the tiles in the grid

Position each tile frame on the 2:1 grid. With `base` at size `(2,1)`, square
tiles of size `(0.5, 0.5)` tile it as a **4×2 patch of 8 tiles**:

```
   y
   ^   +--------+--------+--------+--------+
 1.0   |tile_04 |tile_05 |tile_06 |tile_07 |   (top row)
       |(0,.5)  |(.5,.5) |(1,.5)  |(1.5,.5)|
 0.5   +--------+--------+--------+--------+
       |tile_00 |tile_01 |tile_02 |tile_03 |   (bottom row)
       |(0,0)   |(.5,0)  |(1,0)   |(1.5,0) |
 0.0   +--------+--------+--------+--------+--> x
       0       0.5      1.0      1.5      2.0

   (labels show each tile's origin; every tile size = 0.5 x 0.5)
```

- Make adjacent frames **share an edge** (or overlap slightly). Because every
  tile receives the *same* continuous base, land flows across each shared edge
  automatically.
- Click **Apply** after moving frames.
- Use **Zoom** to fit the whole layout.

### Step 5 — Flatten and export

1. In each graph's list row, set the **background-tag combobox** to the output
   that graph should contribute. (This is literally what the exporter reads;
   graphs with no valid tag are skipped.)
2. Click **Flatten/export**. The dialog now has an **aspect** dropdown that drives
   the output shape:

   ```
   +---------------------------------------------+
   |  Hesiod - Graph flatten configuration       |
   |  filename  [ world_export.png            ]   |
   |  aspect    [ 2:1 (equirectangular)    v ]   | <- pick this for a patch
   |  shape     [=========O====]   2048x1024     | <- long edge; H is derived
   |  tiling    [===O==========]   4x4           |
   |  overlap   [======O=======]   0.50          |
   |                       [   OK   ] [ Cancel ]  |
   +---------------------------------------------+
   ```

   - **aspect** — choose `2:1 (equirectangular)` for an Azgaar-ready patch. Other
     presets: `1:1 (square)`, `3:2`, `16:9`, and `Custom`.
   - **shape** — a single slider sets the **long edge** (`256 … 4096`, powers of
     two). The short edge is computed from the aspect ratio and snapped to a
     multiple of 16 so it stays tiling-friendly. At `2:1` with the long edge on
     `2048` you get `2048x1024`; the label to the right always shows the final
     `W x H`.
   - **Custom** unlocks a second **height** slider so you can set both edges
     independently (still powers of two).

   > This replaces the old square-only shape control. You no longer set width and
   > height as two separate boxes — you pick the aspect and one resolution slider.

3. You get the 16-bit grayscale heightmap (feed to Azgaar) and a `_preview.png`
   hillshade to check continuity.

---

## 4. The two things that are NOT automatic

Design these in deliberately — the engine will not add them for you.

- **Longitude wrap (left edge = right edge).** This comes *only* from the **base
  field being periodic in X** (`MakePeriodic` on X, or periodic noise). The
  Broadcast/Receive resampling does not wrap on its own — it is the periodic base
  that makes the far-left and far-right tiles read matching values.

- **Land off the poles.** The top and bottom of the world must be ocean. Do it
  **once in `base`** with a latitude mask; every tile inherits it through its
  Receive.

---

## 5. Layer order, at a glance

```
   LAYER LIST                     WORLD                         BROADCAST FLOW
   (bottom = upper)
 +-----------+
 | base      |   continents + periodic-X + pole mask --> Broadcast ----+
 +-----------+   (spans the full 2:1 world; top of the list)           |
 | tile_00   |   detail ----.                                          |
 |   ...     |              |     [ tiles read base in        tiles can receive
 | tile_07   |   detail ----+       their own footprint ]     from base (above)
 +-----------+                                                         v
```

Keep `base` at the **top**. Tiles below it receive; the export flattens all
frames into one 2:1 image.
