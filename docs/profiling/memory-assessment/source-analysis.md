# Allocation and lifetime source audit

This audit concerns Hesiod `377f6370` and its pinned HighMap
`d73e05adee1a04cc1e955d3f3162398a37f1c17d`. It accompanies the focused
measurements for [Hesiod #783](https://github.com/ottolink-dev/Hesiod/issues/783).
The byte counts in this document are **source-derived payload estimates**,
not measured RSS or proof that each allocation overlaps the process peak.
Allocation captures establish which stacks are live at the observed heap peak;
see [capture procedure and accounting](allocation-capture.md) and the
[measured allocation results](../assessments/2026-11-27/allocations/README.md).

Let `P = width × height` and `B = 4P` bytes for one float channel. A channel is
1 MiB at 512², 4 MiB at 1024², and 16 MiB at 2048². Array/vector metadata,
allocator overhead, tile halos, library state and GPU memory are additional.

## ExportAsset

[`compute_export_asset_node`](../../../Hesiod/src/model/nodes/nodes_function/export_asset.cpp)
always gathers elevation into one full `Array`. The optional color texture is
gathered and written in a nested scope, so its temporary `Texture` is destroyed
before mesh export. The normal-map stages have a different lifetime:

1. `normal_map(array)` produces `nvec`, a three-channel `Texture`.
2. `Texture::channel()` returns a reference, but `Array nx/ny/nz = ...` copies
   each channel into independent storage: **3B of explicit additional payload**.
3. A full alpha channel adds B. `normal_map.from_arrays(...)` creates virtual
   storage for four channels (at least 4B in RAM, plus any tile halos).
4. `nvec`, the three copied channels, alpha and the virtual normal map remain
   in scope during `export_asset`, even though the PNG has already been written.
   Their nominal payload is **11B**: 11, 44 or 176 MiB at the resolutions above.
   This is a candidate lifetime reduction, not a prediction of RSS saved.
5. With a mask connected, a further B is gathered and the masked mesh path runs.
   The optimized mesh option is bypassed in that branch.

HighMap's [`export_asset.cpp`](../../../external/HighMap/HighMap/src/export/export_asset.cpp)
builds an Assimp scene before calling `Assimp::Exporter::Export`. For the regular
triangle path there are P vertices and `2(width−1)(height−1)` faces. Vertex and UV
buffers each hold three floats per vertex. Every triangle has a separate
`new unsigned[3]` allocation, in addition to the `aiFace` array. The masked path
also builds an `unordered_map` from grid coordinates to vertex indices. A dense
mask retains almost all vertices/faces while paying for that map.

Mesh arrays and Assimp's export-time working storage are global to the mesh.
Changing the input tile count does not turn this API into a streaming exporter.
PNG encoding and Assimp serialization can add allocations outside HighMap; those
must be attributed through allocation stacks rather than guessed from RSS.

## TextureQuiltingShuffle and TextureQuiltingExpand

The [shuffle](../../../Hesiod/src/model/nodes/nodes_function/texture_quilting_shuffle.cpp)
and [expand](../../../Hesiod/src/model/nodes/nodes_function/texture_quilting_expand.cpp)
nodes explicitly gather a guide and every channel of every connected texture,
run a full-array algorithm, then scatter outputs. With guide texture plus A–D
connected, this is **20 texture channels plus one height guide = 21B** before
algorithm scratch. The fixture reuses some upstream sources, but the node still
gathers each connected port separately; source sharing does not deduplicate
these secondary `Array` values.

HighMap's [`quilting.cpp`](../../../external/HighMap/HighMap/src/synthesis/quilting.cpp)
retains all original secondary inputs while creating:

- a primary output and one output per secondary channel; their dimensions
  include the algorithm's patch overlap and can exceed the final image;
- a domain-width horizontal strip for each channel;
- a primary patch and each secondary patch;
- seam error/mask arrays and transposes while joining strips.

These are full-domain or patch/strip buffers, not VirtualArray tiles. The
algorithm's `overlap` parameter controls patch stitching and is distinct from
the graph tile halo/overlap tested by the runner.

At the end of `quilting`, `*secondary_arrays[k] = secondary_arrays_output[k]`
performs copy assignment from an lvalue. The output buffers are no longer used
after this loop. With larger quilted dimensions, an input vector may allocate
new storage while its old storage and every quilt output still exist. This is
a concrete ownership-transfer candidate. The wrapper then extracts slices to
restore the requested dimensions, allocating again. Expand with
`keep_input_shape=true` additionally retains resampled smaller input channels.

Both nodes scatter their heightmap and texture outputs while their gathered
secondary arrays and primary result are still live. RAM output storage therefore
coexists with full arrays. Disk storage can bound tile residency; it cannot
remove these gathered inputs or the algorithm scratch arrays.

## Candidates that are not full-image copies

`secondary_arrays_storage` grows without `reserve`, but `Array` contains a
movable `std::vector<float>` and does not declare a copy constructor or destructor
that would suppress the implicit move. Compile-time checks against the pinned headers confirmed both
`is_nothrow_move_constructible_v<Array>` and
`is_nothrow_move_assignable_v<Array>`. Vector growth is therefore not evidence
of repeated deep image copies. Reserving metadata capacity may still be useful.

The unused `VirtualArray r/g/b/a` objects in each texture reconstruction loop
create storage objects. RAM pixel buffers are allocated lazily by
`RamTileStorage::get_tile`, which these unused objects never call. Removing these
objects is cleanup, not a four-image memory optimization. Disk mode may incur
storage setup work; measure separately before assigning a significant peak.

## Coverage boundaries

The focused full fixtures use an explicit heightmap guide, all optional
texture inputs, and a dense mask for ExportAsset. They are deliberately stressful,
not typical-use estimates. Minimal fixtures keep both quilting guides but omit
A–D; minimal ExportAsset supplies only elevation and exercises the unmasked
regular triangle path. The texture-only fallback (no heightmap guide), sparse
masks, alternate mesh formats, optimized meshes and different patch sizes need
distinct cases before generalizing their behavior. RSS is
whole-process resident memory. Heaptrack observes intercepted heap allocations;
it does not account for every memory mapping, allocator overhead or GPU VRAM.
