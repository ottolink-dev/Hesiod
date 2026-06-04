# Broadcast & Receive

## The idea

Normally terrain flows along wires inside one graph. **Broadcast** and **Receive**
let terrain jump *between* graphs without a wire — useful when you split a world
into several graphs and need one graph's output available in another.

A **Broadcast** node publishes the heightmap on its input under a named **tag**.
A **Receive** node in any graph picks a tag from a list and outputs that heightmap.

## How Hesiod handles it

These are the `Broadcast` and `Receive` nodes (in the **Routing** category).

- `Broadcast` passes its `input` straight through to a `thru` output and publishes
  it under an automatically managed `tag`.
- `Receive` offers a `tag` chooser that the graph manager fills in with the tags
  currently being broadcast. When it computes, it doesn't just copy the source
  heightmap — it **re-projects** it from the broadcasting graph's coordinate frame
  into the receiving graph's frame (`interpolate_heightmap`). That is what makes
  Broadcast/Receive the backbone of multi-graph worlds, where each graph occupies a
  different rectangle of one shared world space.

## See also

- [Building a "patch of graphs"](../user_manual/patch-of-graphs.md) — the advanced
  worldbuilding workflow built on Broadcast/Receive.
- [Tiling & overlap](tiling-overlap.md) — the shared world space these graphs live in.
