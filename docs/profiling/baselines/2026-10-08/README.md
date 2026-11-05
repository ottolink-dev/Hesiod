# Day 1 baseline: 8 October 2026

This is the immutable first broad node-profiling baseline for
[issue #783](https://github.com/ottolink-dev/Hesiod/issues/783). It records the
**measured October 8 executable**, not the later commit that publishes these
results or the rebased profiling implementation. Do not replace these files
when running newer code: create a new dated baseline and compare matched cases.

## Results

The overnight run started at 22:12:25 Europe/London and finished after
4,101.39 seconds (68 minutes), within its eight-hour budget. Of 276 registered
non-deprecated nodes, 273 completed all nine measurements: **2,457 successful
runs**, two failed attempts, and one explicit missing fixture. Failed nodes
were excluded from subsequent repetitions and larger sizes.

The complete [report.csv](report.csv) contains one row per node. Its JSON
counterpart is [report.json](report.json). At 2048², 48 nodes took under 10 ms,
97 took 10–100 ms, 94 took 100 ms–1 s, 23 took 1–10 s and 11 took at least 10 s.
These are target algorithm-zone elapsed times, not whole application runtimes.

| Node | 512² median | 2048² median | Time multiplier | 2048² median process peak |
| --- | ---: | ---: | ---: | ---: |
| Abs | 0.93 ms | 16.39 ms | 17.6× | 391 MiB |
| HeightmapToNormalMap | 8.99 ms | 120.32 ms | 13.4× | 487 MiB |
| ExportAsset | 1.07 s | 107.64 s | 100.3× | 3,081 MiB |
| HydraulicBlur | 0.74 s | 104.16 s | 140.6× | 424 MiB |
| RecastCliffDirectional | 0.45 s | 62.24 s | 139.1× | 488 MiB |
| RecastCliff | 0.29 s | 37.98 s | 131.4× | 456 MiB |
| TextureQuiltingShuffle | 0.21 s | 11.81 s | 55.7× | 1,792 MiB |
| TextureQuiltingExpand | 0.18 s | 6.42 s | 36.2× | 1,648 MiB |

512² → 2048² increases pixel count 16×. The larger time multipliers identify
scaling candidates, not proven algorithmic defects: filter radii and other
resolution-dependent parameters can increase work per pixel. ExportAsset also
includes export work. TextureQuiltingShuffle and TextureQuiltingExpand merit
memory investigation alongside ExportAsset.

For each node, define repeat spread as `(maximum - minimum) / median`.
The median of these spreads across successful nodes was 9.1% at 512², 3.8% at
1024² and 2.3% at 2048². Three fresh-process repeats support initial triage,
not small speedup claims. Runs were sequential; system load and caches were
not controlled as a laboratory benchmark.

## Measurement conditions and interpretation

- Tracy 0.13.1; Hesiod RelWithDebInfo, GCC 16.2.0; OpenCL enabled.
- 512², 1024² and 2048²; three fresh processes per node and size.
- One OpenMP thread, 1×1 tiling, zero overlap, deterministic inputs and seeds.
- GPU-backed implementations selected NVIDIA GeForce RTX 5060 Laptop GPU,
  OpenCL 3.0 CUDA. CPU implementations stayed on CPU. This was not a paired
  CPU-versus-GPU experiment. CPU model and driver version were not recorded.
- One synthetic fixture/default parameter group per node. Fixed-size geometry
  and imported images do not grow with the requested heightmap resolution.
- `Preview` measures backend dispatch, not rendering. `Receive` sums two
  expected invocations in a producer/consumer fixture.
- `Node algorithm` zones measure elapsed callback time, including synchronous
  transfers/waits where present. No GPU kernel timestamps were collected.
- GNU time provides whole-process peak RSS, including startup, dependencies,
  supporting nodes and profiler overhead. It is neither per-node allocation
  nor VRAM. Do not subtract peaks to infer allocation ownership.
- RSS plots sample every 10 ms. At 512²/1024²/2048², respectively 585/357/158
  of the 819 successful runs per size had fewer than two samples inside the
  target zone. Even adequately sampled zones can miss short-lived peaks.
- Tracy system sampling was disabled (`TRACY_NO_SYS_TRACE=1`) after a separate
  local shutdown crash in SymbolWorker/CallstackDataCb/rpmalloc. Explicit zones
  and process-memory plots remained enabled.
- No output-correctness claim follows from a successful timing capture. Export
  cases additionally required a nonempty output file.

## Failures and investigations

These findings describe the Day 1 revisions; recheck before filing new defects
against current upstream. Both failing nodes were classified WIP, not deprecated.

**FlowStream:** the overnight attempt ended with signal 6. A focused GDB run
without a connected capture stopped earlier at SIGSEGV in HighMap
`src/hydrology/flow_stream.cpp:107`, indexing the first path. The Gaussian terrain
has no qualifying strict boundary minima, and the function does not guard its
empty path list. Optimisation prevented displaying vector lengths in GDB;
the access location is confirmed, and source/fixture analysis explains it.
A fix must define no-exit behavior and handle empty paths in callers too:
`dig_river` contains an unsigned `size() - 1` loop. Preserve this fixture as a
regression; use known outlets for meaningful successful flow profiling.

**HydraulicVpipes:** reproduced `CL_INVALID_KERNEL_NAME`. The C++ erosion
implementation calls obsolete buffer-based kernels, while bundled kernels use
new names, image inputs, separate outputs and changed scalar arguments. Renaming
alone is insufficient. The existing flow-simulation implementation demonstrates
bindings for the first two current passes. The wrapper also leaves several
advertised inputs/outputs unwired. The node exception is caught and the process
exits zero; the harness correctly rejects it based on the application error log.

**BlenderBridge:** `needs_fixture`; an isolated Blender receiver is needed.
A disconnected no-op was not counted as coverage.

See [failure investigation](failures/findings.txt), original failure logs and
focused debugger logs in [failures](failures/). No fix for these two algorithm
failures is included in the profiling integration.

## Earlier pilot and priorities

[pilot/summary.csv](pilot/summary.csv) preserves the earlier 72-run pilot:
four cases × three sizes × three repeats × normal/instrumented executables.
The cases were GaussianPulse alone, Abs, CPU approximate DistanceTransform and
HeightmapToNormalMap. The normal build was Release and the Tracy build was
RelWithDebInfo: this is **not a controlled profiler-overhead comparison**.

At 2048², pilot Tracy medians were approximately 35.91 ms for GaussianPulse,
16.68 ms for Abs, 119.26 ms for DistanceTransform and 120.40 ms for
HeightmapToNormalMap. Source triage found three apparently unused channel copies
(`nx`, `ny`, `nz`) in HeightmapToNormalMap, alongside gather/scatter work. That is
a candidate for a correctness-checked change, not a measured speedup or an
allocation attribution.

Next investigations:

1. Recheck the two failures against current HighMap and define regression cases.
2. Add stage zones to ExportAsset and HydraulicBlur, separating conversion,
   computation, transfer/readback and export stages where applicable.
3. Investigate memory growth in ExportAsset and texture quilting with allocation
   evidence; compare tiled and full-array runs separately.
4. Keep node-level zones as the baseline and add finer zones selectively.
   Use matched build configurations for controlled overhead measurements.

## Archived evidence and provenance

- `results.json.gz`: all 2,459 per-attempt result records, including failures.
- `manifest.json`: original 276-case coverage manifest.
- `fixtures-and-runner.tar.gz`: exact frozen fixtures, profiling scripts and
  their required HSD builder modules. Extract for historical inspection, not
  to silently replace the current runner.
- `provenance.json`: original HEAD, recursive dependency revisions, binary/tool/
  fixture hashes, configuration and final state. Machine-specific runtime paths
  and unrelated working-tree file names were omitted.
- `measured-profiling.patch.gz`: original profiling delta against the recorded HEAD,
  before rebasing. The run used a dirty working tree, not a published clean
  commit. The measured binary hash is the authoritative executable identity;
  this archive does not promise a bit-for-bit rebuild.
- `capture-sha256.json`: checksums of original raw captures. The approximately
  55 MB of raw `.tracy` captures, full logs and generated exports remain in the
  original operator's local `build/tracy/overnight` bundle and are **not included
  in this repository**. CSV/results remain accessible without those captures.
- `pilot/`: original pilot summaries, inventory, manifest, provenance and results.
- `SHA256SUMS`: checksums of the baseline files, excluding this checksum file.

Original public report:
[issue #783 comment](https://github.com/ottolink-dev/Hesiod/issues/783#issuecomment-6076172520).
For a new run, follow [the profiling guide](../../README.md), save a new dated
result set, and record changes in source, dependencies, fixtures, build flags,
hardware and execution settings before comparing it with Day 1.
