# Tiling & Overlap

## The idea

A large heightmap can be expensive to compute all at once, so Hesiod can work on it
in smaller **tile**-sized chunks to keep memory use down. This is purely an *internal*
strategy: the chunks are stitched back together into **one seamless heightmap**. You
never handle separate pieces in the graph — your map stays a single, continuous whole
from start to finish. (Splitting into actual separate tile *files* is a different thing
that only happens if you ask for it at export — see [Tiling vs. exporting
tiles](#tiling-vs-exporting-tiles) below.)

**Overlap** is the shared margin those internal chunks use so that effects which read
*nearby* pixels stay consistent across chunk boundaries.

## Why overlap exists

Think of a big mural painted by several people, each given one section of the wall.

Some terrain effects need to **look at the surrounding pixels** to decide a pixel's
value. Erosion is the classic example: to know where water carves a channel, you have
to see the slope *around* that spot. Blurring and smoothing are the same — each pixel
becomes a blend of its neighbours.

Now picture a pixel right at the edge of a chunk. Its neighbours live in the *next*
chunk, which is being worked on separately. Without extra information, that edge pixel
is computed with a piece missing — like two painters who each paint only their own
section perfectly but never blend strokes at the border, leaving a faint line down the
middle.

**Overlap** fixes this by telling each chunk to compute a little way *past* its own
border, into its neighbour's area:

```text
 Looking across one chunk boundary:

   no overlap:    [   chunk A   ][   chunk B   ]
                               ^ edge effects have nothing to read across the border

   with overlap:  [   chunk A  ····][····  chunk B   ]
                              ^^^^^^^^^ a shared margin both chunks compute,
                                        so their edges agree
```

It is the same as telling each painter "paint a few centimetres into the next section
too, so the strokes merge." The edge pixels now have the neighbour context they need,
and the chunks line up cleanly.

## The overlap dial

Overlap is a **fraction from 0 to 1** of the chunk size — a quality-vs-cost dial:

- **0** — no shared margin. Cheapest. Fine when you are not using neighbour-based
  effects.
- **larger** (e.g. `0.25`, `0.5`) — a wider shared margin. Safer for heavy
  erosion/smoothing, but it costs a little more, because the overlapping strip is
  computed by *both* chunks.

You set these in batch/headless mode:

```text
hesiod --batch=graph.hsd --shape=1024,1024 --tiling=4,4 --overlap=0.5
```

`--tiling` sets how many chunks the map is computed in; `--overlap` sets the shared
margin. (Note: `--shape` controls the computed resolution — set it explicitly in batch
mode rather than relying on the value stored in the file.)

## When it actually matters

- If the map is computed **as one piece** — which is what happens on the GPU, where the
  whole heightmap is processed at once — there are no chunks, so overlap does nothing.
- Overlap only comes into play when the map is genuinely computed **in tiles** *and*
  you are running effects that read neighbouring pixels (erosion, smoothing, blur).

In practice Hesiod blends chunk boundaries for you, so you normally will not see a hard
seam — overlap is the safeguard that keeps those boundaries correct, and you turn it up
when an effect leans heavily on its neighbours.

## Tiling vs. exporting tiles

These two uses of the word "tile" are easy to mix up:

- **Tiling (compute)** — the internal chunking described above. The result is always
  **one seamless heightmap**; nothing is actually split.
- **Exporting tiles** — the [`ExportTiled`](../node_reference/nodes/ExportTiled.md) node
  *does* cut the finished map into separate image files (`tile_000_000.png`,
  `tile_000_001.png`, …), for example to stream into a game engine. It has its own
  **Overlapping Edges** option, which adds a one-pixel shared border to each tile so a
  neighbour's edge value is available when those tiles are processed individually later.

So the map is one continuous thing the whole way through the graph; it only becomes
genuine, separate tiles at the moment you deliberately export them.

## How Hesiod handles it

Everything is computed in the normalized unit square, so tiling and overlap change
*how* the map is computed, not the coordinates of the result. (Details:
[Coordinate system](../user_manual/coordinate_system/index.md).)

## See also

- [Coordinate system](../user_manual/coordinate_system/index.md) — the normalized domain tiles live in.
- [Broadcast & Receive](broadcast-receive.md) — stitching whole graphs in shared world space.
