# HighMap stages and node hotspots

Step 2 instruments shared HighMap operations and adds finer stages to HydraulicBlur
and ExportAsset, selected from the Day 1 findings. It changes no algorithms or
parameters. It does not claim individual coverage of every HighMap function.
The dependency change is reviewed separately in
[HighMap PR #537](https://github.com/ottolink-dev/HighMap/pull/537); this branch
pins its commit `6751a406cc846f92eb9dd157feb6e007d8224f1f`.

Hesiod passes `HESIOD_ENABLE_TRACY` to HighMap's `HIGHMAP_ENABLE_TRACY` option.
Standalone HighMap defaults to OFF and can enable it with an installed Tracy
package (`Tracy_DIR`). The HighMap target propagates the client and compile
flag publicly, because downstream code instantiates its tile-dispatch templates.
This keeps the same template definitions in the library and its consumers and
uses one Tracy client per executable. Disabled hooks do not evaluate metadata.

## Reading the zones

| Zones | Meaning |
| --- | --- |
| `HighMap::for_each_tile` | Dispatch including validation, computation and optional trimming; text records compute mode, array and tile dimensions |
| `HighMap::tile.acquire`, `.algorithm`, `.release` | Tile storage access, callback and release, respectively |
| `HighMap::distributed_tile_loop` | Dispatch and joining worker threads, including waiting |
| `HighMap::single_array` | Full-array computation, including intermediate-array lifetimes |
| `HighMap::single_array.gather` | Input and output materialisation and resampling before the callback |
| `HighMap::single_array.algorithm` | The supplied callback |
| `HighMap::single_array.scatter` | Output resampling/write-back |
| `HighMap::VirtualArray::*` | Array materialisation, write-back/interpolation and overlap synchronization |
| `HighMap::VirtualTexture::*` | Channel gathering/scattering and display-byte conversion |
| `HighMap::hydraulic_blur.*` | Clamp, smoothing and combination stages |
| `HighMap::export_asset.mesh*`, `.triangulate`, `.write` | Mesh construction, optimized triangulation and Assimp export calls |
| `HydraulicBlur::*` | Input-range scans, input copy, remapping and post-processing in the Hesiod node |
| `ExportAsset::*` | Elevation conversion, optional texture export, normal-map generation/conversion/blending/writing and masked/unmasked export |

Nested durations must not be summed. Worker-thread tile zones appear on their
own thread tracks, not as nested children of the dispatching thread. Dispatch
wall time includes worker waits; summing worker durations is not elapsed time.
Conversion also uses tile dispatch, so a `tile.algorithm` zone can represent a
copy/interpolation callback rather than the selected terrain algorithm. Read its
calling context. These are CPU zones, not OpenCL kernel or GPU transfer timestamps,
and they do not populate Tracy's allocation view.

There are no per-cell hooks. Nevertheless, capturing many small tiles can generate
many events; compare equivalent configurations and measure overhead separately.

## Repeatable checks

Build with tests enabled, then run:

```sh
cmake --build build/tracy --target hesiod profile_highmap --parallel
ctest --test-dir build/tracy -R 'profiling_highmap|graph_editor' --output-on-failure
```

`profile_highmap` checks all output cells after sequential, distributed,
full-array and downscaled dispatch. It uses a 32x24 ramp for tiled modes and a constant field for full-array
modes, each in four tiles. Full-array modes resample even at native resolution,
so the check does not assume identity interpolation for a nonconstant field. Stride 2 and cutoff 0.5 exercise resampling and
reconstruction. Start a Tracy
capture, then run `TRACY_NO_SYS_TRACE=1 HESIOD_TRACY_WAIT=1 ./profile_highmap`
from the executable directory to inspect all four modes. With hooks disabled it
runs the same numerical checks without waiting for capture.

For end-to-end node captures, use the existing bounded suite's `--cases
GaussianPulse Abs HydraulicBlur ExportAsset` filter, initially at 64 square pixels
and one repeat. ExportAsset fixtures enable export and isolate output files.
Include `DistanceTransform` to check a forced full-array node. The viewer workload
also exercises these shared conversion hooks underneath its existing zones.

These are diagnostic checks, separate from the immutable October 8 baseline.
Record both the Hesiod and HighMap commits with any new measurements.


## Validation (2026-10-19)

The Tracy-enabled application, viewer and dispatch workloads built successfully.
Both `graph_editor` and `profiling_highmap` CTests passed. All seven modified
implementation files also compiled with hooks disabled and contained no Tracy
symbols. The dispatch capture verified eight phases and tile work on multiple
threads, with gather/algorithm/scatter and storage stages present.

Five bounded 64² node captures passed: GaussianPulse, Abs, DistanceTransform,
HydraulicBlur and ExportAsset. CSV checks verified HydraulicBlur's clamp/smooth/
combine stages, node range/copy/remap/post-processing, VirtualTexture conversion,
and masked mesh generation/export. Two additional unmasked ExportAsset captures
passed for regular and optimized triangles, including the triangulation zone.
These one-repeat runs establish instrumentation coverage, not stable timings or
an overhead estimate.
