# Source review of the 40 full-array candidates

This is a bounded source audit of the current
[inventory snapshot](../assessments/2026-11-27/inventory.csv), not a new measurement
of every node. It reviews Hesiod **`377f6370`** and HighMap **`d73e05ad`**.
Every one of the snapshot's 40 `full_array` candidates appears once below.
Only **ExportAsset, TextureQuiltingExpand and TextureQuiltingShuffle** have current
allocation attribution in the [focused study](../assessments/2026-11-27/allocations/README.md).
All other rows are **unmeasured here**; historical timing/RSS results do not
establish their present allocation ownership or worst-case behavior.

The queue is a text-search signal: `to_array`, `from_array` or a single-array mode
reference is sufficient to enter it. The review separates 22 algorithm/dispatch
candidates, 16 I/O/geometry/authoring conversions and two reduced-size conversion
signals. Those categories describe current source paths, not intrinsic
impossibility of a different tiled implementation.

The remaining **173 `configurable_tiles` and 49 `review_other`** nodes have not
received this source review. Neither category guarantees bounded memory. A tile
callback can retain full outputs, allocate resolution-dependent scratch, launch
concurrent work, invoke a gathering helper or transfer full buffers to a GPU.

## Shared dispatch evidence

HighMap's [`single_array_compute`](../../../external/HighMap/HighMap/include/highmap/virtual_array/virtual_array.inl#L443)
gathers every connected input **and output** through
`to_array(...).resample_to_shape(shape_wrk)`, calls the algorithm, then scatters.
Even a reduced working stride first invokes native-shape `to_array` in this
path. Disk storage changes tile residency and gathering concurrency, not the
existence of these full float arrays. Output gathering is explicitly marked as
a retrofit TODO in the source. This is a shared follow-up candidate, with no
new peak measurement claimed here.

In contrast, [`VirtualArray::to_array(requested_shape, cm)`](../../../external/HighMap/HighMap/src/virtual_array/array/virtual_array.cpp#L435)
allocates the **requested** output shape and samples source tiles into it. It
does not inherently allocate an additional native-resolution float array.
That distinction matters for Dendry and HeightmapToKernel below.

## Whole-domain algorithm or forced-dispatch paths: 22 nodes

Source links identify each `compute_*_node` implementation. “Measured” means
only the configurations in the focused study, not all parameter branches.

| Node(s) | Direct source evidence | Tiling limitation and current assessment |
|---|---|---|
| [Compare](../../../Hesiod/src/model/nodes/nodes_function/compare.cpp#L85) | `for_each_tile({a,b},{out}, ..., cm_single_array)` around `hmap::compare`. | Two full input gathers plus output dispatch storage; changing graph tiles does not change this selection. Unmeasured. |
| [Flip](../../../Hesiod/src/model/nodes/nodes_function/flip.cpp#L73), [Rotate](../../../Hesiod/src/model/nodes/nodes_function/rotate.cpp#L48), [Translate](../../../Hesiod/src/model/nodes/nodes_function/translate.cpp#L54), [Zoom](../../../Hesiod/src/model/nodes/nodes_function/zoom.cpp#L58) | Each passes `cm_single_array`; Translate/Zoom also accept displacement inputs. Rotate invokes `hmap::gpu::rotate` inside the gathered callback. | Global image transforms in current implementation. Connected displacement arrays increase gathered input count; Rotate also needs device-memory assessment. Unmeasured. |
| [KmeansClustering2](../../../Hesiod/src/model/nodes/nodes_function/kmeans_clustering2.cpp#L57), [KmeansClustering3](../../../Hesiod/src/model/nodes/nodes_function/kmeans_clustering3.cpp#L62) | `cm_single_array`; optional normalization copies each feature before `kmeans_clustering2/3`. | Two/three full features and output dispatch storage, plus normalization and clustering scratch. Both mode and normalization branch need profiling. Unmeasured. |
| [MakePeriodic](../../../Hesiod/src/model/nodes/nodes_function/make_periodic.cpp#L54), [MakePeriodicStitching](../../../Hesiod/src/model/nodes/nodes_function/make_periodic_stitching.cpp#L45) | `cm_single_array` around `make_periodic` / `make_periodic_stitching`. | Current callbacks operate on whole-domain boundaries; patch overlap is separate from graph tile halos. Unmeasured. |
| [DepressionFilling](../../../Hesiod/src/model/nodes/nodes_function/depression_filling.cpp#L79), [DistanceTransform](../../../Hesiod/src/model/nodes/nodes_function/distance_transform.cpp#L78), [FloodingFromBoundaries](../../../Hesiod/src/model/nodes/nodes_function/flooding_from_boundaries.cpp#L80), [FloodingFromPoint](../../../Hesiod/src/model/nodes/nodes_function/flooding_from_point.cpp#L66), [FloodingLakeSystem](../../../Hesiod/src/model/nodes/nodes_function/flooding_lake_system.cpp#L65) | `use_distributed_computation ? cm_cpu : cm_single_array`; setup defaults the flag to **false**. Callbacks pass `TileRegion` and `ICommunicator` to HighMap algorithms. | Genuine global gathers in the default branch, **not unconditional non-distributability**. The alternate branch follows configured CPU dispatch and needs correctness/communication/concurrency measurements. Unmeasured. |
| [LooseSymmetry](../../../Hesiod/src/model/nodes/nodes_function/loose_symmetry.cpp#L87) | Native-shape `to_array`, `hmap::loose_symmetry`, then `from_array`. | Full input and returned output coexist; patch size and analysis/synthesis strides also scale with configuration. Unmeasured. |
| [QuiltingBlend](../../../Hesiod/src/model/nodes/nodes_function/quilting_blend.cpp#L81), [QuiltingExpand](../../../Hesiod/src/model/nodes/nodes_function/quilting_expand.cpp#L65), [QuiltingShuffle](../../../Hesiod/src/model/nodes/nodes_function/quilting_shuffle.cpp#L62) | Native gathers, scalar `quilting_*` routines, then scatter. Blend gathers up to four inputs; Expand/Shuffle preallocate an output before assigning the algorithm result. | The scalar variants still use global quilting workspace, but the measured 20-secondary-channel texture peaks must not be assigned to these nodes. Unmeasured. |
| [TextureQuiltingExpand](../../../Hesiod/src/model/nodes/nodes_function/texture_quilting_expand.cpp#L110), [TextureQuiltingShuffle](../../../Hesiod/src/model/nodes/nodes_function/texture_quilting_shuffle.cpp#L108) | Gather guide and every connected texture channel, call scalar quilting, scatter outputs. | **Measured:** full-array workspace persists with tiled graph storage. Expand's final-copy overlap and Shuffle's output/strip/patch peak differ; see attribution report. |
| [HeightmapToNormalMap](../../../Hesiod/src/model/nodes/nodes_function/heightmap_to_normal_map.cpp#L46), [NormalMapToHeightmap](../../../Hesiod/src/model/nodes/nodes_function/normal_map_to_heightmap.cpp#L54) | First gathers elevation and creates a normal texture; second gathers three channels and selects direct integration or a Poisson solver before scatter. | Full arrays/textures remain regardless of graph tiles. HeightmapToNormalMap also creates unused nx/ny/nz copies. Solver choice changes scratch/work. Unmeasured. |

## I/O, geometry and authoring conversions: 16 nodes

These are real memory candidates, but their controlling sizes can include source
image dimensions, geometry cardinality, export format and optional ports in
addition to graph resolution. “Tiled output” is not equivalent to a streaming
implementation.

| Node(s) | Direct source evidence | Tiling limitation and current assessment |
|---|---|---|
| [ExportAsset](../../../Hesiod/src/model/nodes/nodes_function/export_asset.cpp#L154) | Gather elevation/optional mask, create normal texture and optional image files, then Assimp scene/export buffers. | **Measured:** serialization/scene copying dominates current peaks; normal-map buffers also overlap export. RAM tiles do not stream the mesh exporter. |
| [ExportHeightmap](../../../Hesiod/src/model/nodes/nodes_function/export_heightmap.cpp#L116), [ExportAsCubemap](../../../Hesiod/src/model/nodes/nodes_function/export_as_cubemap.cpp#L75) | Native gather before image export; Heightmap may resample, Cubemap calls `export_as_cubemap` with its own output resolution. | Whole input remains live while codec/projection output is generated. Auto-export false skips these paths. Unmeasured. |
| [ExportNormalMap](../../../Hesiod/src/model/nodes/nodes_function/export_normal_map.cpp#L86), [ExportTiled](../../../Hesiod/src/model/nodes/nodes_function/export_tiled.cpp#L102) | Each gathers into local `array`, applies flips there, then passes a **second fresh `to_array`** to the export routine. | Two full gathers can coexist during export. ExportTiled divides a complete array into output files; it does not stream the input VirtualArray. First flipped array is not the argument exported. Unmeasured source candidate. |
| [ExportScene](../../../Hesiod/src/model/nodes/nodes_function/export_scene.cpp#L151) | Gather elevation and optional texture, copy forest/rock/cloud/path collections into vectors, call `export_usd`. | Both pixel and geometry cardinality matter; downstream USD writer allocations require attribution rather than extrapolating Assimp findings. Unmeasured. |
| [BlenderBridge](../../../Hesiod/src/model/nodes/nodes_function/blender_bridge.cpp#L70) | Gather elevation, optionally `to_raw` texture, move vectors into an invoked streaming callback. | The vectors can outlive compute in queued work; transport/receiver/backlog state matters. Needs an isolated connected fixture. Unmeasured. |
| [ImportHeightmap](../../../Hesiod/src/model/nodes/nodes_function/import_heightmap.cpp#L103) | Decode full source image into `Array`; Bicubic/Nearest explicitly resample to output shape, then `from_array`. | Other sampling uses tiled scatter from the source-sized array. Source-file size remains a separate resident input; later clipping/dequantization dispatch is tiled. Unmeasured. |
| [TextureUvChecker](../../../Hesiod/src/model/nodes/nodes_function/texture_uv_checker.cpp#L58) | Load bundled 2K texture, then `tensor4.resample_to_shape(node.cfg().shape)` before channel scatters. | Fixed source image does **not** mean fixed compute memory: full target texture is also materialized. Unmeasured. |
| [Brush](../../../Hesiod/src/model/nodes/nodes_function/brush.cpp#L58) | Copy persisted painting into `painted`, resample to graph shape, then `from_array`. Invalid-data fallback also allocates graph shape. | Painting storage may be fixed independently, but compute still creates a full graph-resolution array before tiled output storage. Unmeasured. |
| [CloudRandomDistance](../../../Hesiod/src/model/nodes/nodes_function/cloud_random_distance.cpp#L50) | With density, first gather and generate a global cloud; then independently generate per-tile clouds and overwrite output with `merge_clouds`. | Global density and earlier cloud survive during tiled work. Without density there is no image gather. Redundant first stage is a source-review candidate, not a measured optimization. |
| [SeedForest](../../../Hesiod/src/model/nodes/nodes_function/seed_forest.cpp#L152) | Build density by tiles; [`hmap::va::seed_forest_clusters`](../../../external/HighMap/HighMap/src/world/flora/forest_seeding.cpp#L519) uses per-tile sums/sub-forests; later gather elevation for `set_elevation_from_terrain`. | Mixed pipeline: tiled seeding does not eliminate the final full elevation gather; forest cardinality and merge storage are separate scaling axes. Unmeasured. |
| [RockSimulation](../../../Hesiod/src/model/nodes/nodes_function/rock_simulation.cpp#L152), [RocksToHeightmap](../../../Hesiod/src/model/nodes/nodes_function/rocks_to_heightmap.cpp#L71) | Simulation gathers elevation/density/optional exclusion; both use `RockField::to_heightmap(shape)` to create full raster arrays before scatter. | Full raster outputs coexist with rock objects; simulation has additional emission/total-elevation temporaries. Vary rock count separately from image resolution. Unmeasured. |
| [PathDig](../../../Hesiod/src/model/nodes/nodes_function/path_dig.cpp#L67), [PathToHeightmap](../../../Hesiod/src/model/nodes/nodes_function/path_to_heightmap.cpp#L48) | PathDig tiles normal digging but gathers/scatters the output in `force_downhill`; PathToHeightmap tiles outline rasterization but allocates full shape when `filled`. | Branch-dependent global arrays. `Path::to_array` is a rasterization method, **not** a VirtualArray gather; regex hits alone do not identify which branch gathers. Unmeasured. |

## Reduced-size conversion signals: two nodes

| Node | Direct source evidence | Why the `full_array` label needs qualification |
|---|---|---|
| [Dendry](../../../Hesiod/src/model/nodes/nodes_function/dendry.cpp#L80) | `p_ctrl->to_array({128,128}, cm_cpu)` followed by tile callbacks for output generation. | The control gather is fixed at 128² floats, not the full graph resolution. It is skipped without a control input. Tile storage, per-tile algorithm scratch and concurrent execution remain unmeasured. |
| [HeightmapToKernel](../../../Hesiod/src/model/nodes/nodes_function/heightmap_to_kernel.cpp#L79) | `to_array(kernel_shape, cm_cpu)`, where `kernel_shape={2*ir+1,2*ir+1}`; optional envelope creates another kernel array. | A kernel-sized conversion, not automatically a native-resolution gather. `ir` derives from a normalized radius, so memory can still grow with graph size and radius. Measure the actual kernel dimensions. |

## Concrete next profiling cases from this review

The strongest additional source-only copy/lifetime leads are the duplicate
ExportNormalMap/ExportTiled gathers, unused HeightmapToNormalMap channel copies,
CloudRandomDistance's global stage before tiled replacement, scalar quilting's
preallocated output overwritten by the algorithm result, and shared single-array
output gathering/resampling. None has a new measured peak or predicted savings
in this review. Some paths also raise output-semantics questions (notably export
flips); verify correctness before changing them.

For breadth beyond the three measured candidates, prioritize: the five
conditional distributed hydrology/morphology nodes in both modes; the export
cases with real files and multiple formats; and mixed geometry nodes with
independently varied object counts. Preserve failure/no-op outcomes rather than
calling a skipped branch memory-efficient. The two reduced-size cases and the
other 222 queue entries need workload-specific checks before any bounded-memory
claim.
