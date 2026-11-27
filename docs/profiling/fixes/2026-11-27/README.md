# Memory follow-up implementation

Fixes for [Hesiod #783](https://github.com/ottolink-dev/Hesiod/issues/783),
following the [frozen baseline assessment](../../assessments/2026-11-27/README.md).
The measured implementation addressed 21 tracked findings; two follow-ups remain.
The changes are published for review against `dev` in [Hesiod #942](https://github.com/ottolink-dev/Hesiod/pull/942). The original
assessment and all evidence records below remain unchanged, including their
historical commit IDs and issue status. See [issue/commit mapping](evidence/issue-status.json)
and [source provenance](evidence/source-provenance.json).

## Publication port to dev

Dependency PRs: [CLWrapper #11](https://github.com/ottolink-dev/CLWrapper/pull/11),
[GNode #35](https://github.com/ottolink-dev/GNode/pull/35), and
[HighMap #598](https://github.com/ottolink-dev/HighMap/pull/598). All target `dev`.
Hesiod and HighMap are draft integration PRs pending the remaining validation
and dependency review; nothing has been merged or deployed.

The publication branch starts from upstream `dev` commit
`697ad0ae03ff79aff4bd1e5b57a5d2349916fc1d`. It ports only the assessment, fixes,
regression tools and evidence from the measured branch. ExportAsset retains
upstream `dev`'s `for_each_tile` normal-map blending API inside the shortened
normal-map lifetime scope. Other upstream submodule changes, including GNodeGUI,
are preserved; dependency publication branches supply the corresponding fixes.

The native tests, exact output comparisons and memory measurements below were
run on the main-based implementation identified in the evidence. They have
**not been rerun on this dev port** and do not establish its native correctness
or memory usage. A compile-only check passed for all nine changed node implementations against
the publication Hesiod/HighMap headers. It reused existing generated headers and
unchanged dependency include paths; it did not configure/link/run the full app.
[Exact commands and results](publication/node-compilation-summary.json) are
recorded separately. Full application linking/runtime validation remains pending
before this draft is ready to merge.

The dev port passes 160 Python tests and 29 subtests, with ten skips: seven need
Apple linker tools and three need a native Hesiod binary. The 34 profiling
unittests also pass. Commands run from the publication checkout, using the
existing validation environment, were:

```sh
env -u HESIOD_BIN /path/to/validation/venv/bin/python -m pytest tests -q -rs
/path/to/validation/venv/bin/python -m unittest discover -s tests/profiling -v
```

A separate integrity check confirmed that all 243 assessment and fix-evidence
files are byte-identical to the measured branch. Only this report's publication
context and its checksum were updated; historical evidence was not rewritten.

## Changes

- [Hesiod #933](https://github.com/ottolink-dev/Hesiod/issues/933): normal-map
  construction, blending and PNG writing now finish in a scope before mesh
  serialization. Channel pointers avoid three extra copies while their texture
  owner remains alive.
- [Hesiod #934](https://github.com/ottolink-dev/Hesiod/issues/934): normal-map and
  tiled exports consume the already gathered/flipped array. This intentionally
  fixes previously ignored export flip flags.
- [Hesiod #935](https://github.com/ottolink-dev/Hesiod/issues/935),
  [#936](https://github.com/ottolink-dev/Hesiod/issues/936), and
  [#937](https://github.com/ottolink-dev/Hesiod/issues/937): remove unused normal
  channels, the discarded global density-cloud pass, scalar quilting output
  preallocation and unused texture wrapper metadata.
- [HighMap #587](https://github.com/ottolink-dev/HighMap/issues/587) and
  [#588](https://github.com/ottolink-dev/HighMap/issues/588): move finished
  secondary arrays and extracted patches; record each guide row's random states
  and seam masks, then build secondary strips one channel at a time. Source
  arrays stay unchanged until all channels finish, preserving duplicate and
  guide-alias behavior.
- [HighMap #589](https://github.com/ottolink-dev/HighMap/issues/589): avoid
  resampling an already owning snapshot to its existing shape. Independent
  output snapshots remain necessary because callbacks may read old output
  values. Reduced and mixed shapes retain the old interpolation: direct
  reduced VirtualArray gathering has different coordinates and halo behavior.
- [GNode #34](https://github.com/ottolink-dev/GNode/issues/34): callbacks retain
  a shared lifetime gate. Callback execution and destruction take the same
  recursive state lock; destruction disables queued callbacks and waits for
  any active callback before releasing the worker. The graph cancellation
  callback is cleared after joining its update thread.
- [Hesiod #938](https://github.com/ottolink-dev/Hesiod/issues/938),
  [#939](https://github.com/ottolink-dev/Hesiod/issues/939),
  [#940](https://github.com/ottolink-dev/Hesiod/issues/940), and
  [#941](https://github.com/ottolink-dev/Hesiod/issues/941): update obsolete test
  fixtures and navigation expectations, restore missing node/port metadata,
  distinguish missing metadata in the CLI check, and remove the duplicate
  stepping-shore UI edge.

Additional discoveries are tracked in the parent issue as separate tasks:

- [HighMap #590](https://github.com/ottolink-dev/HighMap/issues/590): reject
  incompatible secondary/guide shapes before quilting indexes missing patches
  or mutates destinations.
- [HighMap #591](https://github.com/ottolink-dev/HighMap/issues/591): a zero-radius
  CPU/GPU smoothing operation is now an identity, preventing NaNs when small
  quilting seams round the radius to zero. Positive-radius memory comparisons
  were recorded separately before this numerical correction.
- [HighMap #592](https://github.com/ottolink-dev/HighMap/issues/592): make the
  shared numerical assertion reject nonfinite operands even with fast-math,
  compare zero fields symmetrically, and avoid float accumulator overflow.

- [HighMap #594](https://github.com/ottolink-dev/HighMap/issues/594): define
  singleton histogram matching at the reference median, avoiding a zero rank
  denominator. This is an explicit new convention for a previously invalid
  result; ordinary multivalue ranking stays unchanged.
- [HighMap #595](https://github.com/ottolink-dev/HighMap/issues/595): correct the
  even-length endpoint-pruning test and add an odd-length case. The morphology
  algorithm remains unchanged.
- [HighMap #596](https://github.com/ottolink-dev/HighMap/issues/596): align the
  CPU PerlinHalf smoothing parameter with the GPU and verify finite noise/FBM
  output on both backends.
- [HighMap #597](https://github.com/ottolink-dev/HighMap/issues/597): dispatch all
  documented zero-width smooth clamps to their hard-clamp equivalents. Thirty
  positive-width cases (23,040 values) remain bit-identical.
- [CLWrapper #10](https://github.com/ottolink-dev/CLWrapper/issues/10): exercise
  rediscovery with an available device type; skip only the CPU-specific case
  when the host has no CPU OpenCL device. Restore filter/selection after each
  test. Production device selection remains unchanged.

## Remaining work

[HighMap #593](https://github.com/ottolink-dev/HighMap/issues/593) remains a
separate numerical follow-up. Integer division before `ceil` can leave a quilt
too small for the requested crop at low overlap. Unchanged baseline examples:
37×29 input and 12×8 patches produce a 36×24 quilt; Shuffle/Blend then return
empty, and ratio-two resized Expand similarly requests 74×58 from 72×56. This
patch preserves those cases; it does not count byte-identical empty arrays as
successful nonempty numerical output. Changing coverage needs its own output
compatibility assessment.

### Assimp export

[HighMap #586](https://github.com/ottolink-dev/HighMap/issues/586) remains open.
The installed Assimp 6.0.5 public `Exporter::Export` implementation always makes
an owned scene copy before export; glTF also enforces vertex joining. Setting
`AI_SCENE_FLAGS_NON_VERBOSE_FORMAT` does not remove the copy and can trigger
additional expansion before joining. Public registration supports new exporter
workers, but does not expose the built-in worker as a supported bypass.

No private ABI calls or substitute mesh serializer are introduced here. A
separate Assimp API/upstream change or independently validated exporter is
needed to remove this measured bottleneck. Normal-map lifetime reduction helps
Hesiod's contribution but does not eliminate Assimp's scene allocation.

Source: [Assimp 6.0.5 Exporter.cpp](https://github.com/assimp/assimp/blob/v6.0.5/code/Common/Exporter.cpp)
and [public Exporter.hpp](https://github.com/assimp/assimp/blob/v6.0.5/include/assimp/Exporter.hpp).

## Validation

### Final suites

The final source tree (`33c8c4d8`, with HighMap `d036c3efc`, GNode `149a4a9`
and CLWrapper `5c742c2`) passes **996/996 HighMap tests** and **66/66 CTest
entries**. The CLWrapper executable reports 15 passed and one explicit
CPU-device capability skip. GNode passes 60/60 and Meta passes 59/59; some
executables are also covered by CTest, so these counts should not be summed.

The final executable also passes 94 deterministic application-output checks:
50 unchanged standard cases, 24 intended export-flip corrections compared with
independently reflected source data, and 20 ExportAsset detail cases covering
all five blending modes, masks and orientations. PNG/CSV bytes and normalized
GLB JSON/binary chunks are checked exactly, along with export counts and PNG
sizes/depths. Three port-diagnostic probes pass: valid catalogue success,
missing-node documentation failure and missing-port mismatch failure.

The final Python run reports 169 passed, 29 passing subtests and seven
platform-specific skips. All 271 shipped HSD fixtures pass consistency checks.

### Focused checks

- Quilting memory-only changes: 3,240 cases matched all primary and secondary
  float output bytes against the unchanged baseline library. These cover two
  guides, rectangular/non-divisible shapes, 0/4/20 secondary channels, all eight
  transform combinations, overlaps, Shuffle/Expand variants, duplicates and
  guide aliases. The strict audit covers 79,868,656 finite float cells and also
  records 1,544 preserved empty arrays; the preexisting wrapper coverage bug
  that causes those empties is tracked separately. Thirteen targeted tests also passed under Valgrind with one
  OpenMP thread and no detected errors or definite leaks.
- Dispatcher: 74 focused tests passed; all 72 parameterized output fingerprints
  matched the baseline. A separate allocation probe counted 8 → 4 full-size
  gather allocations for two inputs/two outputs at 1024², in both RAM and disk
  modes. Stride-two allocation counts stayed unchanged. This is an allocation
  count, not a peak-memory measurement.
- GNode: all 60 tests passed under ASan/UBSan with stack-use-after-return
  detection; ten lifecycle tests passed 100 repetitions (1,000 executions). A separate
  ThreadSanitizer build passed 20 repetitions of those ten tests (200
  executions) with no reported race.
- Python: 169 passed, seven Apple-platform tests skipped; all 271 shipped HSD
  fixtures passed consistency checks. The corrected catalogue eliminates all
  108 previous port mismatches against the frozen binary.

### Isolated library memory

| Operation, 1024² and 20 channels | Baseline peak heap | Fixed peak heap | Reduction |
| --- | ---: | ---: | ---: |
| Shuffle | 302.15 MB | 223.96 MB | 25.9% |
| Expand, ratio 2, keep shape | 334.09 MB | 243.10 MB | 27.2% |

These are decimal MB of intercepted live heap from standalone heaptrack runs,
including inputs and stream buffers. They are not application RSS. Every saved
output matched byte for byte. Concurrent compilation makes elapsed times from
these particular captures unsuitable as performance evidence. Quilting still
gathers complete source/output arrays; processing secondary strips one at a
time reduces workspace replication but does not make the entire algorithm
bounded by tile size.

The strengthened comparison helper initially exposed four formerly hidden
failures: CPU PerlinHalf noise/FBM, singleton histogram matching, and the incorrect
even-line endpoint expectation. Their failing logs remain separate evidence;
none were silenced by changing tolerance or skipping numerical tests. The
subsequent source/test fixes are listed above.

### Application peak RSS

All 96 new fixed runs passed, covering 32 configurations with three repetitions
per configuration. They are paired with 96 existing frozen-baseline observations;
those old observations are reused, not counted as new executions. The matrix
covers 1024²/2048², 1×1/4×4 grids, RAM/streamed disk storage, full/minimal
ExportAsset and full Shuffle/Expand inputs. All 16 scaling guards and policy
audits pass. Median peak RSS decreased in every cell, by 2.74%–23.78%.

Selected **2048², RAM, 1×1** medians (MiB):

| Case | Baseline | Fixed | Reduction |
| --- | ---: | ---: | ---: |
| ExportAsset (minimal) | 3,027.64 | 2,850.18 | 5.86% |
| ExportAsset (full) | 3,283.96 | 3,106.29 | 5.41% |
| TextureQuiltingShuffle | 1,741.37 | 1,437.78 | 17.43% |
| TextureQuiltingExpand | 1,908.94 | 1,506.63 | 21.07% |

See the [complete 32-cell CSV](evidence/memory/memory-fix-comparison.csv) and
[paired-run verification](evidence/memory/memory-fix-comparison.json) for all
medians, observed ranges and provenance. Across the tested 2048² configurations,
Shuffle reductions are 17.4%–19.8% and Expand reductions are 20.5%–23.8%.

All 48 exported GLB/PNG sets are byte-identical to their matching baseline.
The comparator requires identical fixture bytes, policies, thread settings,
relevant runner hashes and measurement methods. Reviewed application-data
changes are limited to the corrected node catalogue, stepping-shore fixture and
`git_version.txt`; the generated version label is consumed by About/splash UI.
Unexpected data changes or differing export hashes fail the comparison. The
first comparator invocation stopped on the version-label change; after source
review confirmed its UI-only consumers, the explicit allowlist was updated and
the comparator passed. The measurements were not rerun or replaced.

A separate [three-run fresh-baseline check](evidence/memory/memory-baseline-calibration-comparison.json)
uses the original executable and data, with its original source association
recorded separately from the current orchestration checkout. Fixtures, policies
and exported assets match. Fresh peaks are 0.11%–0.24% below the old medians and
just outside their narrow three-observation min/max ranges. These are not new
baseline repetitions or uncertainty estimates.

RSS is whole-process resident memory, including startup and producers; disk
page cache is excluded. These measurements do not attribute every byte to a
node. Shared-host timings are descriptive, and no latency improvement is
claimed. Full-resolution 4096² exports were not repeated in this fix matrix.

Compact logs, output fingerprints, test reports, reproducers and raw library
heap captures are retained in [evidence](evidence/). Compressed application-run
archives omit executables, exports and caches. Large originals remain under
ignored `output/fixes/`.
