# Allocation attribution: 27 November 2026

All **15 real Hesiod heaptrack captures completed and validated**. They use
Hesiod application sources from upstream `377f6370`, HighMap `d73e05ad`, and
assessment tooling commit `1a0a0de7`. The executable is the same frozen
RelWithDebInfo binary identified by SHA256 in [provenance.json](provenance.json).
Tracy is disabled; allocations are observed through heaptrack 1.5.0.

These figures are **intercepted CPU heap bytes live at the process's global heap
peak**, not RSS, GPU VRAM or separate per-node peaks. Each ownership group is a
disjoint set of allocation call stacks active at that same instant. Allocation
sites indicate the call path that created a buffer, not necessarily its current
owner. Native RSS measurements are a separate series.

## Measured heap peaks

Each cell is one capture, not a median. All cases use sequential CPU tiles, one
OpenMP thread, zero graph overlap and deterministic fixture parameters. Independent
graph source nodes can still execute concurrently. Full fixtures connect all
ports; minimal quilting fixtures retain both guides and omit A–D; minimal
ExportAsset supplies only elevation (unmasked regular triangles).

| Node | 512² minimal, MiB | 512² full, MiB | 1024² minimal, MiB | 1024² full, MiB | 512² full, 4×4 tiles, MiB |
|---|---:|---:|---:|---:|---:|
| ExportAsset | 174.974 | 185.228 | 579.321 | 619.576 | 185.242 |
| TextureQuiltingShuffle | 63.589 | 121.033 | 133.425 | 364.296 | 121.042 |
| TextureQuiltingExpand | 64.324 | 128.350 | 136.565 | 394.778 | 128.360 |

Changing 1×1 to 4×4 graph tiles did not reduce the observed full-fixture heap
peak: changes were under 0.02 MiB. Gathered arrays and full-array algorithm
buffers are identical in size; small storage metadata differences remain. This
is evidence for these configurations, not a claim about every overlap/storage
policy. The native suite covers those additional settings.

At the global peak, allocations reached through the target node's call path
increased from 134.666 to 539.013 MiB for ExportAsset, 70.500 to 283.763 MiB for
Shuffle, and 77.808 to 314.236 MiB for Expand when pixel count increased fourfold.
The ratios are about four, with patch-dimension rounding for quilting. This
small two-resolution study identifies large per-pixel constants; it does not
establish all-resolution complexity or a regression threshold by itself.

## ExportAsset: mesh export, scene copies and normal-map lifetimes

The full 1024² capture has this **disjoint** breakdown:

| Allocation call path live at global heap peak | Bytes | MiB |
|---|---:|---:|
| Assimp JoinVerticesProcess | 343,058,000 | 327.166 |
| Assimp scene copy | 83,777,319 | 79.896 |
| Original HighMap masked mesh | 83,771,404 | 79.891 |
| Normal-map generation | 12,583,008 | 12.000 |
| Direct nx/ny/nz channel copies | 12,582,912 | 12.000 |
| Alpha channel | 4,194,304 | 4.000 |
| Virtual normal-map channels | 16,777,856 | 16.001 |
| Elevation/mask gathers | 8,388,608 | 8.000 |
| Other ExportAsset/Assimp paths | 62,600 | 0.060 |
| OpenCL initialization | 34,753,010 | 33.143 |
| Other process allocations, including sources | 49,722,982 | 47.420 |
| **Total** | **649,672,003** | **619.576** |

Assimp's vertex-joining preparation dominates this peak. The original scene and
its copy coexist. The masked coordinate hash map is allocated during mesh
construction but is no longer live at this global peak; optimizing that map alone
would not remove the dominant observed allocations. The minimal, unmasked case
still has almost the same JoinVertices and scene-copy footprint, so the optional
mask is not the cause of this dominant peak.

The normal-map hypothesis is also measured: all three explicit channel copies
are live, with source locations `export_asset.cpp:190`, `:191` and `:192`; alpha
is at `:193`. Together with the generated normal texture and virtual normal-map
channels, their payload is **44 MiB at 1024²**, plus 736 bytes of attributed
metadata. The same families total **11 MiB + 736 bytes at 512²**. Their scopes
extend past PNG writing into mesh export. Releasing them earlier and using
channel references are concrete lifetime/copy candidates, though allocator page
retention can prevent an equal RSS reduction.

## Expand: an output-copy overlap at the actual peak

At 1024², both of these allocations are live simultaneously:

- **116,741,120 bytes** in 20 secondary quilt output arrays allocated at
  `quilting.cpp:74`.
- **116,741,120 bytes** in their copy-assigned destinations at `quilting.cpp:222`.

Each set is `20 × 1208 × 1208 × 4` bytes, or 111.333 MiB. The default patch width
is 307; expansion ratio 2 gives work patch width 153, seven patches across and
137 overlap cells, producing a 1208-wide temporary quilt before cropping. The
original full-size gathered secondary inputs are also still live through the
wrapper. At 512², each duplicated output set is 28,800,000 bytes
(`20 × 600 × 600 × 4`).

This is direct allocation evidence for replacing the final secondary output
copies with ownership transfer after reviewing alias behavior. It does not
promise a 111.333 MiB reduction in the next global peak: removing this overlap
can move the maximum to an earlier patch/strip phase.

## Shuffle: output, strip and patch workspace dominates

Shuffle's full 1024² global peak occurs inside quilting with approximately
199.761 MiB in output/strip/patch workspace and **84 MiB** in gathered guide plus
20 secondary channels. The 20 secondary outputs account for 114,624,720 bytes;
the 20 secondary horizontal strips account for 55,828,080 bytes. Patch and seam
buffers add further live storage. No final secondary-copy-assignment allocation
is live at this peak, unlike Expand.

A move-only final handoff can still reduce copying, but is not established as a
peak-memory fix for this Shuffle configuration. The stronger memory direction is
limiting channel fan-in or redesigning output/strip workspace while preserving
cross-patch seams. Reducing to the guide texture alone cuts the observed 1024²
heap peak from 364.296 to 133.425 MiB.

## Evidence and reproduction

[allocation-summary.csv](allocation-summary.csv) and
[allocation-summary.json](allocation-summary.json) contain exact global peaks and
all disjoint groups. [summarize.py](summarize.py) regenerates them from the collapsed
peak stacks, assigning each stack once and asserting that group sums equal the
recorded global peak.

[captures.tar.gz](captures.tar.gz) contains all 15 interpreted, symbolized raw
heaptrack `.zst` captures (12.72 MiB before the outer archive), their fixtures,
results, application and batch logs, unmerged allocation reports, full collapsed
peak stack exports, script snapshot (including its JSON assets), build configuration,
provenance and the application node catalog. It excludes redundant JSON stack
exports, generated mesh/PNG files, the 1.3 GiB executable, and application runtime
data other than `node_documentation.json`. Export hashes and sizes remain in
results. Symbols and source locations are retained in the interpreted captures
and text reports; the executable's hash and build provenance identify the local
binary. The archive supports standalone summary regeneration and capture
inspection; rerunning native workloads requires rebuilding the pinned application
with its runtime data and installing the profiling tools.

```sh
tar -xzf captures.tar.gz
python summarize.py allocation-study
# Reinspect one interpreted capture with an installed heaptrack_print:
heaptrack_print --merge-backtraces=0 \
  --file allocation-study/runs/TextureQuiltingExpand-full-1024-t1/heaptrack.zst
```

The archive also includes profiler calibration sources: disjoint 8 MiB then
16 MiB allocations showed 0 and 16 MiB respectively in the global-peak stack
export, confirming that it is not a sum of independent high-water marks.
[Capture procedure](../../../memory-assessment/allocation-capture.md),
[source lifetime audit](../../../memory-assessment/source-analysis.md), and
[local optimization issue drafts](../../../memory-assessment/optimization-followups.md)
provide the methodology and next steps. No issues have been posted remotely.
