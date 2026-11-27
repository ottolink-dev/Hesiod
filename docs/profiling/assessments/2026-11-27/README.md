# Focused memory assessment — 27 November 2026

This continues [Hesiod #783](https://github.com/ottolink-dev/Hesiod/issues/783)
by attributing the memory peaks of ExportAsset, TextureQuiltingShuffle and
TextureQuiltingExpand, and comparing resolution, connected inputs, tiling and
storage policies. The [October 8 baseline](../../baselines/2026-10-08/README.md)
is preserved separately. These are new measurements of current upstream
application code, not a relabelled historical baseline.

The strongest optimization targets are Assimp's export preparation, Expand's
overlapping secondary-output copies, Shuffle's simultaneous channel workspace,
and ExportAsset's normal-map lifetimes. Four
[local issue drafts](../../memory-assessment/optimization-followups.md) include
measured evidence, proposed investigations and acceptance criteria. None has
been filed remotely, and no terrain or export algorithm has been changed.

The completed study contains **900/900 successful primary native measurements**,
**210/210 passing same-grid growth guards**, six separate fixed-tile-core
comparisons, and **15/15 validated allocation captures**. A corrected 84-run
smoke precedes these measurements; two earlier smoke attempts are retained with
their original rejected validation statuses. Derived comparisons reuse existing
records and are not counted as new executions.

## Native resident memory: RAM series

The RAM series completed **378/378 fresh-process measurements** in 33.67 minutes:
126 configuration cells, each repeated three times. It covers both candidate
input variants, matched source-only controls and Abs at 512²/1024²/2048²,
with 1×1 tiles at zero overlap and 4×4 tiles at zero/0.1 overlap.
[report.csv](memory-ram/report.csv), [report.json](memory-ram/report.json) and
[provenance.json](memory-ram/provenance.json) retain the exact results and settings.

These are median **whole-process peak RSS** values in MiB, including startup
and upstream source nodes; they are not the allocation peaks in the next section.

| Full-input fixture, 1×1 tiles, no overlap | 512² | 1024² | 2048² |
|---|---:|---:|---:|
| ExportAsset | 513.9 | 1,031.2 | 3,284.0 |
| TextureQuiltingShuffle | 425.2 | 684.8 | 1,741.4 |
| TextureQuiltingExpand | 433.2 | 719.9 | 1,908.9 |
| Abs | 342.2 | 353.5 | 401.2 |

Connected input count matters substantially. The following 2048² comparisons
change one recorded fixture/layout choice at a time; full and minimal fixtures
have different source subgraphs and are not a subtraction-based allocation test.

| Candidate, RAM at 2048² | Minimal, 1×1 | Full, 1×1 | Full, 4×4, overlap 0 | Full, 4×4, overlap 0.1 |
|---|---:|---:|---:|---:|
| ExportAsset | 3,027.6 | 3,284.0 | 3,194.2 | 3,274.9 |
| TextureQuiltingShuffle | 738.9 | 1,741.4 | 1,648.9 | 1,718.1 |
| TextureQuiltingExpand | 750.0 | 1,908.9 | 1,790.1 | 1,848.6 |

The modest RSS changes with RAM tiling do not establish a tiled algorithm.
Source-node temporaries, tile residency and allocator retention can affect
whole-process RSS. The separate allocation captures still show globally
materialized arrays and algorithm workspace.

All **84 adjacent-resolution RSS guards passed**, requiring complete repetitions
and allowing `peak_small × pixel_ratio × 1.25 + 32 MiB`. This is a coarse
regression tripwire, not proof of optimal memory use. Fixed startup memory can
hide growth in smaller components. The allocation evidence independently shows
large per-pixel working sets and avoidable overlap despite passing these guards.
No independently measured source-control peak is subtracted and labelled a node
allocation total.

## Native resident memory: disk-backed series

The matching disk-backed matrix completed **378/378 measurements**, with all
**84 adjacent-resolution guards passing**, in 29.79 minutes. Its 126 cells use
the same sizes, variants, source controls, layouts and three repetitions as the
RAM series. [report.csv](memory-disk/report.csv),
[report.json](memory-disk/report.json) and
[provenance.json](memory-disk/provenance.json) retain the full comparison.

These are median whole-process peak RSS values in MiB for full-input 2048²
fixtures. All columns use zero overlap except the last.

| Candidate | RAM, 1×1 | Disk, 1×1 | RAM, 4×4 | Disk, 4×4 | Disk, 4×4, overlap 0.1 |
|---|---:|---:|---:|---:|---:|
| ExportAsset | 3,284.0 | 3,011.2 | 3,194.2 | 2,978.1 | 2,977.8 |
| TextureQuiltingShuffle | 1,741.4 | 1,522.7 | 1,648.9 | 1,476.8 | 1,476.1 |
| TextureQuiltingExpand | 1,908.9 | 1,707.7 | 1,790.1 | 1,619.7 | 1,620.6 |
| Abs | 401.2 | 400.9 | 371.7 | 341.9 | 343.7 |

Disk-backed tile storage lowers these process peaks, but the candidate nodes
still gather global arrays and execute their full-array algorithms. It does not
make their workspace proportional to a single graph tile. Filesystem page cache
is outside this metric, so the RSS reduction is not an equal measured reduction
in total host memory. The heap captures below use RAM storage; they do not
attribute the exact RAM-versus-disk RSS difference.

## Larger resolutions

The quilting/Abs extension completed **120/120 additional runs** at 4096² in
37.38 minutes. It retains minimal/full variants and matched source controls,
with both storage policies, 1×1/4×4 grids, zero overlap and three repetitions.
The [native report](memory-4096-quilting/report.json) records every cell.
Full-input median whole-process peaks, in MiB, are:

| Candidate at 4096² | RAM, 1×1 | RAM, 4×4 | Disk, 1×1 | Disk, 4×4 |
|---|---:|---:|---:|---:|
| TextureQuiltingShuffle | 5,529.5 | 5,558.0 | 4,889.2 | 4,886.8 |
| TextureQuiltingExpand | 6,087.2 | 6,119.7 | 5,454.1 | 5,451.5 |
| Abs | 592.1 | 472.5 | 592.5 | 352.0 |

Changing the RAM grid to 4×4 does not reduce these full quilting peaks. Disk
storage reduces residency, but the full quilting working sets still occupy
several GiB. Abs behaves differently because its computation can follow tile
dispatch; a fixed-size tile-core comparison is recorded separately below.

All **40 matched 2048→4096 growth guards passed**. The
[derived report](comparisons/quilting-2048-4096-report.json) verifies matching
binary, scripts, application data, fixture bytes and policy settings, then reuses
120 existing 2048 runs alongside the 120 new runs. These are not additional
executions. The [complete derived input](comparisons/quilting-2048-4096.json.gz)
retains the selected original records. The single-resolution native report
correctly marks its own growth check incomplete; only the combined two-size
report establishes these comparisons.

The selected ExportAsset extension completed **6/6 runs** in 81.10 minutes:
minimal/full variants, RAM storage, 1×1 tiles, zero overlap and three repetitions.
It does not add 4096 export source controls or disk/other-grid cases. The
[native report](memory-4096-export/report.json) records these medians:

| ExportAsset variant, RAM / 1×1 | 2048² peak, MiB | 4096² peak, MiB | RSS ratio |
|---|---:|---:|---:|
| Minimal | 3,027.6 | 10,964.2 | 3.621× |
| Full | 3,284.0 | 11,604.0 | 3.534× |

The three full runs span 11,603.3–11,604.6 MiB. Their individual wall times are
about 26–27 minutes; the per-run timeout was 40 minutes and the RSS guard 16 GiB.
These timings describe the test duration, not an isolated performance benchmark.
Both [derived export growth guards](comparisons/export-2048-4096-report.json)
passed after verifying matching provenance and fixtures; the
[complete derived records](comparisons/export-2048-4096.json.gz) reuse six
existing 2048 results alongside the six new runs.

## Fixed-size tile cores

A separate **18/18-run supplement** completed in 5.14 minutes, using full-input
fixtures, disk storage, zero overlap and an 8×8 grid at 4096². Pairing these
three-repeat medians with the existing 2048²/4×4 results keeps each tile core at
**512×512**, while total pixels and tile count both increase fourfold.

| Fixture | Role | 2048² / 4×4, MiB | 4096² / 8×8, MiB | RSS ratio |
|---|---|---:|---:|---:|
| TextureQuiltingShuffle | Target and sources | 1,476.8 | 4,891.4 | 3.312× |
| TextureQuiltingShuffle | Sources only | 360.4 | 360.4 | 1.000× |
| TextureQuiltingExpand | Target and sources | 1,619.7 | 5,445.2 | 3.362× |
| TextureQuiltingExpand | Sources only | 360.0 | 360.7 | 1.002× |
| Abs | Target and sources | 341.9 | 341.4 | 0.999× |
| Abs | Sources only | 340.3 | 340.5 | 1.000× |

The source controls and Abs retain approximately constant process peaks, while
quilting grows substantially despite unchanged tile-core dimensions. Together
with the allocation captures and source audit, this supports the conclusion that
the quilting working sets remain global. No independent source peak is subtracted
to estimate a target allocation total. File-backed page cache remains outside
process RSS, so this does not establish constant total host memory or I/O cost.

The [native report](memory-4096-fixed-tiles/report.json),
[paired summary](comparisons/fixed-core-2048-4096-report.json) and
[complete paired records](comparisons/fixed-core-2048-4096.json.gz) retain all
six comparisons. They reuse 18 existing 2048 runs and are deliberately separate
from the same-grid growth guards: resolution and grid both change here.

## Allocation evidence

All **15 allocation captures** completed and passed validation. Each value below
is intercepted heap memory live at the **global process heap peak**, from one
capture; it is not process RSS, a per-node peak, GPU memory or a median.

| Full-input fixture, 1×1 tiles | 512² heap peak, MiB | 1024² heap peak, MiB | Main finding |
|---|---:|---:|---|
| ExportAsset | 185.228 | 619.576 | Assimp vertex joining and scene copying account for 65.70% of the 1024² peak. |
| TextureQuiltingShuffle | 121.033 | 364.296 | Output, strip and patch workspace dominates; final copy-assignment buffers are absent at this peak. |
| TextureQuiltingExpand | 128.350 | 394.778 | Two distinct 111.333 MiB secondary-output sets coexist at the 1024² peak. |

Changing these full fixtures to 4×4 tiles at 512² changed their heap peaks by
less than 0.02 MiB. The gathered arrays and algorithm workspace remain global.
This comparison uses RAM storage and zero graph overlap; it does not establish
the effect of every storage/overlap configuration.

Expand's duplicated sets each contain `20 × 1208 × 1208 × 4` bytes. They are the
original secondary quilt outputs and their copy-assigned destinations, both
live simultaneously. Ownership transfer is a concrete optimization candidate,
but removing this overlap can move the peak to another stage. Shuffle requires
a separate workspace investigation rather than a promise that the same copy
change will lower its peak.

ExportAsset also retains **44 MiB of normal-map pixel payload at 1024²**, plus
736 bytes of attributed metadata, through mesh serialization. Three copied
normal channels account for 12 MiB of that payload. In contrast, the masked
coordinate hash map is no longer live at the global heap peak. A memory-peak
finding does not identify the cause of the masked export's long runtime.

The [allocation report](allocations/README.md) supplies exact disjoint groups,
minimal-input comparisons, source locations and all 15 symbolized captures.
An independent agent verified every stack sum and group against the recorded
global peaks and regenerated the summary from the archived evidence. The
[source lifetime audit](../../memory-assessment/source-analysis.md) separates
source-derived estimates from measured live allocations.

## Source coverage

The [current inventory](inventory.csv) contains 262 registrations: 40
`full_array` signals, 173 `configurable_tiles` and 49 `review_other` entries.
Its `unmeasured` field describes static triage, not the focused results above.
The [source review](../../memory-assessment/inventory-review.md) examines all
40 full-array candidates and distinguishes global algorithms, I/O/geometry
conversions, optional distributed branches and reduced-size gathers.

For example, Dendry gathers a fixed 128² control image; HeightmapToKernel gathers
at kernel size; five hydrology/morphology nodes offer an alternate distributed
path. Those qualifications prevent treating every regex hit as an unavoidable
native-resolution gather. Conversely, the other 222 entries are not proven to
have bounded memory: called helpers and tile callbacks can hide large working
sets. Only the three named candidates receive new allocation attribution here.

## Build and measurement scope

| Item | Recorded configuration |
|---|---|
| Upstream application base | `377f6370` |
| HighMap | `d73e05adee1a04cc1e955d3f3162398a37f1c17d` |
| Profiling tooling commit | `1a0a0de7` |
| Native build | Linux x86-64, GCC 16.2.0, Qt 6.12.0, RelWithDebInfo (`-O2 -g -DNDEBUG`), Tracy off, full node set |
| CPU / visible memory | Intel Core Ultra 9 275HX, 24 logical CPUs, approximately 62 GiB RAM |
| OpenCL initialization device | NVIDIA GeForce RTX 5060 Laptop GPU; selected fixture algorithms run on CPU |
| Tile execution | Sequential tiles, one OpenMP thread; independent graph nodes may still execute concurrently |
| Temporary tile storage | Per-run `TMPDIR` on the study's ext4 NVMe filesystem under `/home` |

All measured native and allocation series use the same executable SHA256:
`0fa7fb40613cbb1af3401bf96e2490a8c68383a2b39f2021ff50f27a3110d4cc`.
Provenance records the recursive submodule pins, fetched dependency revisions,
binary/data/script hashes, CMake cache, environment and exact fixture JSON.
The two test-only include repairs in `fc60ba14` enabled GUI test compilation;
they do not change the measured application implementation.

Full quilting fixtures connect guide RGBA plus four secondary textures and an
explicit heightmap guide. They reuse deterministic upstream sources, so their
20-channel algorithm fan-in is not a worst-case graph with five distinct source
textures. Minimal quilting retains both guides. Full ExportAsset adds a dense
mask, color and detail-normal texture; minimal ExportAsset has elevation alone.
Alternate formats, optimized meshes, sparse masks, texture-only guide fallback,
different patch parameters, device memory and arbitrary real-world graphs remain
outside this assessment.

Native peak RSS comes from a fresh small supervisor's `wait4` accounting for
its direct Hesiod child. The supervisor prevents accumulated controller memory
from inflating inherited pre-execution RSS and records its remaining small RSS
floor. Process RSS includes startup, sources, computation and teardown. It must
not be confused with the heaptrack allocation figures, and independently
measured source-only peaks must not be subtracted and called node allocations.
Disk-backed storage can also move pressure into filesystem page cache outside
the process RSS metric.

Completion checks require exact final runtime rows with positive timings from
fresh fixtures, matching effective settings and no application errors.
ExportAsset must produce a GLB containing mesh metadata and PNGs of the expected
dimensions. These checks establish completed work and structural exports, not
numerical correctness of every terrain pixel or mesh. The first two small smoke
attempts exposed unreliable IPC/trace-based checks; their evidence is preserved.
The corrected smoke completed **84/84** runs before measured series began.
The original rejected reports are retained under `memory-smoke-64/` and
`memory-smoke-64-v2/`; the corrected run is under `memory-smoke-64-v3/`.
Later validation audits do not overwrite those original statuses or turn
single-resolution smoke runs into growth comparisons.

A separate export repeatability audit found byte-identical artifact sets across
all six RAM/disk repetitions in each of 18 matching configurations: **108
ExportAsset runs** across the three main resolutions, variants and tile layouts.
This compares repetitions and storage policies within a configuration, not
different layouts or arbitrary graphs. Hashes, sizes and the audit procedure
are retained in the evidence. Separate policy audits verified all 756 main
matrix runs against their CLI flags, effective storage markers and isolated
one-thread OpenMP configuration.
The extension audit also found identical artifact sets across the three repeats
of each 4096 export variant, bringing the repeatability check to **114 runs in
20 matching configurations**. Policy audits cover **all 900 primary runs**,
including the larger-resolution and fixed-core extensions.

Elapsed times are diagnostic context, not an isolated performance benchmark.
Other host builds were observed, and an isolated two-worker GNode sanitizer build
overlapped part of the RAM series. Exact timing caveats are retained in validation
evidence. No speedup or historical timing-regression claim is made.
Filesystem and driver caches were not flushed between fresh processes.
Global host swap counters also changed during shared-host work. The evidence
retains resource preflights and five-second process `VmSwap` observations for
parts of the 4096 phases, including explicit gaps and unsampled fast runs.
These sparse observations do not establish that every measurement was swap-free.
No nonzero `VmSwap` was observed among the 99 sampled runs in the main large-run
observer or the 13 sampled runs in the fixed-core observer.

## Tests and reproduction

The [validation report](baseline-failures.md) records both passes and failures.
All 34 profiling tests, six CTests, 904 HighMap tests, 59 Meta tests and UI palette
checks passed; the dispatch workload had no Valgrind errors or lost allocations.
The historical archive's 23 checksums also passed. Existing Python/catalogue
failures, the unavailable CPU OpenCL device and a sanitizer-confirmed GNode
lifecycle defect are retained rather than presented as a clean full-suite pass.

Use the [memory-study procedure](../../memory-assessment/README.md) and
[allocation-capture procedure](../../memory-assessment/allocation-capture.md)
to reproduce the fixtures and accounting. Keep any future optimization's
correctness checks separate from its memory measurements and compare identical
input, storage, tiling and build configurations.

## Retained artifacts

- Each native `memory-*/` directory contains configuration, provenance, terminal
  status, JSON/CSV reports, compressed original results and `run-evidence.tar.gz`.
  The archive retains fixtures, application/batch logs, supervisor requests and
  measurements, isolated settings, script snapshots with their JSON assets,
  source diff and CMake cache. It excludes large executables, application runtime
  data, generated exports and temporary tile storage. The original executables,
  data and exports remain in local ignored outputs; identifying hashes and
  structural export metadata remain in the results.
- `comparisons/` contains readable derived summaries and compressed complete
  input records. Reused lower-resolution records are identified explicitly and
  do not increase the number of native executions.
- [Allocation evidence](allocations/README.md) includes all 15 symbolized
  captures, collapsed global-peak stacks and an independent summary generator.
- [validation-evidence.tar.gz](validation-evidence.tar.gz) retains test logs,
  sanitizer diagnostics, failure-path probes, policy/repeatability audits,
  resource observations, archive-verification results and their helper scripts.
  [validation-summary.json](validation-summary.json) indexes the broad tests;
  the focused measurement matrices have their own reports above.

[SHA256SUMS](SHA256SUMS) covers the committed assessment artifacts. Verify it
from this directory:

```sh
sha256sum -c SHA256SUMS
```

The full build, uncompressed original evidence and generated mesh/PNG files are
also retained locally under `build/assessment/` and `output/` in the assessment
worktree. The archived native reports were regenerated byte-for-byte from their
retained results with both the captured and committed tooling; fixture and
script hashes were checked independently. The historical October baseline
remains separate and unchanged.
