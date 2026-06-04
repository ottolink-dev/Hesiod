# Tiling & Overlap

## The idea

A large heightmap can be expensive to compute all at once, so Hesiod can split it
into **tiles** and compute them piece by piece. **Overlap** is a margin shared
between neighbouring tiles so that operations needing nearby data (erosion,
blurring) don't produce visible **seams** where tiles meet.

You meet these as parameters in batch/headless mode, for example:

```text
hesiod --batch graph.hsd --shape=1024,1024 --tiling=1,1 --overlap=0
```

`--tiling` sets how many tiles the map is divided into; `--overlap` sets the shared
margin. More overlap means cleaner seams at some compute cost.

## How Hesiod handles it

Everything is computed in the normalized unit square, so tiling and overlap change
*how* the map is computed, not the coordinates of the result. (Details:
[Coordinate system](../user_manual/coordinate_system/index.md).)

## See also

- [Coordinate system](../user_manual/coordinate_system/index.md) — the normalized domain tiles live in.
- [Broadcast & Receive](broadcast-receive.md) — stitching whole graphs in shared world space.
