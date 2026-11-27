# Focused memory assessment

This continues [Hesiod #783](https://github.com/ottolink-dev/Hesiod/issues/783)
with fresh-process resident-memory measurements and separate allocation
captures for ExportAsset, TextureQuiltingShuffle and TextureQuiltingExpand.
The [October survey](../baselines/2026-10-08/README.md) is historical and is
not overwritten by this study.

- [Measured results and validation](../assessments/2026-11-27/README.md)
- [Source review of all 40 full-array candidates](inventory-review.md)
- [Allocation and lifetime source audit](source-analysis.md)
- [Local optimization issue drafts](optimization-followups.md)

## Build and run

Use a full node build with the repository's pinned recursive submodules.
Tracy is unnecessary for these measurements; disable it to avoid mixing its
allocation overhead into the baseline. Keep debugging symbols for allocation
stack attribution.

```sh
cmake -S . -B build/assessment -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DHESIOD_ENABLE_TESTS=ON -DHESIOD_ENABLE_UI_TESTS=ON \
  -DHESIOD_ENABLE_TRACY=OFF -DCMAKE_C_FLAGS=-std=gnu17 \
  -DHESIOD_ENABLE_GENERATE_APP_IMAGE=OFF -DHIGHMAP_ENABLE_BENCHMARKS=OFF
cmake --build build/assessment --parallel 8
python3 -m unittest discover -s tests/profiling -v

# Validate runtime configuration and fixture completion before escalating.
python3 scripts/profiling/memory_study.py run \
  --binary build/assessment/bin/hesiod --output output/memory-smoke \
  --sizes 64 --tiles 1 4 --overlaps 0 .1 --repeats 1 \
  --policies ram-sequential disk-sequential --include-inputs \
  --timeout 60 --max-disk-gib 32

python3 scripts/profiling/memory_study.py run \
  --binary build/assessment/bin/hesiod --output output/memory-ram \
  --sizes 512 1024 2048 --tiles 1 4 --overlaps 0 .1 --repeats 3 \
  --policies ram-sequential --include-inputs --check --max-disk-gib 32
```

The recorded GCC 16 build uses GNU C17 for vendored Triangle. Its Nix-specific
OpenEXR/Imath paths and the broader dependency-test configuration are described
in the [validation report](../assessments/2026-11-27/baseline-failures.md) and
retained CMake cache.

Use a new output directory for each series. The runner copies the executable
and application data and records fixture, script and binary hashes, build
configuration, source/submodule revisions and runtime settings. Run reports
are updated as measurements finish. A failed execution stops later repetitions
and resolution escalation for that fixture/configuration; others continue.
Invalid fixture preparation aborts the suite with a failed state. An incomplete
matrix returns nonzero. Reports can be rebuilt without running
the application:

```sh
python3 scripts/profiling/memory_study.py report output/memory-ram --check
```

Run disk-backed storage as a separate series with
`--policies disk-sequential`. Both policies execute tiles sequentially with one
OpenMP thread by default. `--force-sequential` uses RAM storage; `--min-memory`
uses streamed disk storage. Neither flag changes the candidates' global
gather/algorithm/scatter implementation into a chunked algorithm.
Independent graph nodes can still run concurrently: sequential tile dispatch
does not serialize the graph scheduler. Matched source controls preserve the
same source topology.
Each run sets `TMPDIR` to its own `tmp/` directory, keeping streamed tiles on
the study filesystem and including leftover temporary files in its disk budget.
Record that filesystem when comparing disk-backed runs across machines.
Disk-backed runs can move memory pressure into the filesystem page cache, which
is outside the process RSS metric. A lower process peak does not establish the
same reduction in total host memory consumption.

The runner has a total duration budget, per-process timeout, sampled process
group RSS guard, and a disk budget checked between runs. The RSS guard is a
soft stop condition; it is not a hard memory reservation or a GPU limit. Use
an externally configured cgroup when a hard memory cap is needed. Keep the
machine awake and the launching session alive for a long foreground run.

The larger follow-ups use the same runner and frozen build. These study-specific
limits were used after checking available RAM and disk space:

```sh
python3 scripts/profiling/memory_study.py run \
  --binary build/assessment/bin/hesiod --output output/memory-4096-quilting \
  --cases TextureQuiltingShuffle TextureQuiltingExpand Abs \
  --sizes 4096 --tiles 1 4 --overlaps 0 --repeats 3 \
  --policies ram-sequential disk-sequential --include-inputs \
  --max-rss-mib 12288 --max-disk-gib 32

python3 scripts/profiling/memory_study.py run \
  --binary build/assessment/bin/hesiod --output output/memory-4096-fixed-tiles \
  --cases TextureQuiltingShuffle TextureQuiltingExpand Abs --variants full \
  --sizes 4096 --tiles 8 --overlaps 0 --repeats 3 \
  --policies disk-sequential --include-inputs \
  --max-rss-mib 12288 --max-disk-gib 32

python3 scripts/profiling/memory_study.py run \
  --binary build/assessment/bin/hesiod --output output/memory-4096-export \
  --cases ExportAsset --sizes 4096 --tiles 1 --overlaps 0 --repeats 3 \
  --policies ram-sequential --timeout 2400 \
  --max-rss-mib 16384 --max-disk-gib 32
```

These single-size runs intentionally omit `--check`: one size cannot establish
a resolution-growth comparison. The assessment retains separate derived reports
that select matching 2048 records, verify shared build/fixture/settings metadata,
and combine those records with the new 4096 results. The 8×8 supplement compares
fixed 512×512 tile cores against 2048²/4×4; its changed-grid ratios are separate
from same-grid growth guards. Analysis helpers and exact phase commands are in
the assessment's validation evidence archive.

## Controlled comparisons

The full variant connects every input, including four secondary textures for
quilting and a dense mask, color and detail normal texture for ExportAsset.
The minimal quilting variant retains a texture guide and an explicit heightmap
guide but omits secondary textures. Minimal ExportAsset uses elevation alone.
The texture-only guide fallback and sparse masks are outside this matrix.
The fixture JSON is the authoritative record of explicit parameters; other
parameters use the recorded binary's defaults.

`--include-inputs` also runs the matching source subgraph with the target
removed. Abs is a simple tiled control. Input controls expose source/startup
cost, but subtracting independently observed process peaks does not recover
the target's allocation total. The graph overlap controls tile halos; the
quilting node's overlap controls patch seams and is a different parameter.

Each run requires error-free application logs, matching effective resolution,
tiling and overlap, and exactly one final runtime row with a strictly positive
timing for each expected node. Fixtures carry no persisted runtime information;
these timings are set when compute callbacks end, and the final log is written
after the graph worker finishes. The runner does not use the CLI's
unsynchronized IPC stream or potentially missing trace messages as completion
evidence. ExportAsset also requires a GLB with mesh metadata and
PNG headers with the requested dimensions. These checks reject silent no-ops
and failed batches; they are not a numerical-equivalence test of terrain,
texture pixels or mesh geometry.

## Interpreting memory and scaling

The kernel's `wait4` peak RSS belongs to one Hesiod process directly executed
by a fresh, small Python supervisor. Using a separate supervisor prevents the
controller's catalogue and accumulated results from inflating the workload's
inherited pre-execution RSS. The remaining supervisor RSS floor is recorded in
each result. The peak includes startup, source nodes, the target, serialization
and teardown; kernel accounting may also include waited-for descendants. It is
not sampled RSS, allocation ownership, or GPU device memory. The runner never
uses the cumulative `RUSAGE_CHILDREN` maximum across earlier runs.

Reports retain median, minimum and maximum whole-process peaks for each
configuration. Adjacent resolution comparisons use the squared resolution
ratio as the expected pixel growth. The optional guard is:

```text
peak_large <= peak_small * pixel_ratio * scaling_slack + absolute_slack_mib
```

Defaults allow 1.25 times pixel growth plus 32 MiB, starting at 512 pixels per
axis. Every planned repetition must succeed; missing, invalid or failed data
cannot pass. At least two eligible resolutions are needed for a check.
This is a coarse regression tripwire, not proof of optimal complexity: fixed
startup memory can conceal excessive growth in a smaller component. Compare
the same fixture, fan-in, storage policy, tile layout and machine before
interpreting a change. Use allocation evidence to investigate a regression.
Elapsed times are diagnostic context; concurrent host work, thermal state and
filesystem caches must be controlled before treating them as a performance
benchmark. Fresh processes do not reset filesystem or driver caches; this
runner does not flush those caches between repetitions.

Heaptrack captures are separate experiments. Its global heap-peak snapshot
identifies allocation stacks live together; cumulative allocated bytes and
independent per-stack maxima answer different questions and must not be added
and called the process peak. Heaptrack itself changes timing and RSS. Heap
payload measurements exclude allocator overhead, some mappings, and GPU VRAM.
See the measured report for capture commands and supported conclusions.
