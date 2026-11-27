# Validation and existing failures

These checks accompany the issue #783 memory assessment, based on upstream
Hesiod `377f6370` and its exact recursive submodule revisions. The fresh Linux
build used GCC 16.2.0, Qt 6.12.0, RelWithDebInfo, Tracy disabled, and an NVIDIA
GeForce RTX 5060 Laptop GPU OpenCL device. No production algorithms were changed.
Command logs, test summaries, sanitizer diagnostics and machine-specific build
details are retained in [validation-evidence.tar.gz](validation-evidence.tar.gz).
The original files remain in the local ignored `output/validation/` directory;
`summary.json` indexes the broad test results.

## Results

| Check | Observed result |
| --- | --- |
| Profiling Python tests, final tooling | 34 passed |
| Complete Python suite with fresh native binary | 151 passed, 17 failed, 7 skipped; 29 subtests passed |
| CI Python tests, included above | 15 passed, 7 skipped requiring Apple linker tools |
| Native CTest | All 6 passed: nanoflann build/run, graph editor, dispatch workload, autosave, brush storage |
| Native test cases | Graph editor 38, autosave 23, brush storage 10, nanoflann 12 passed |
| Standalone UI palette | Zero failing checks, using Qt offscreen |
| HighMap GoogleTest | All 904 passed in 22.037 seconds |
| Meta GoogleTest | All 59 passed |
| CLWrapper GoogleTest | 14 passed, 1 failed because no CPU OpenCL device was available |
| GNode GoogleTest | One complete 58-test rerun passed; repeated execution crashes; standalone ASan confirms stack-use-after-return |
| Valgrind dispatch workload | Zero errors; zero definitely, indirectly, or possibly lost bytes |
| October 8 archive checksums | All 23 entries passed |
| Static lint of shipped `.hsd` files | 270 passed, 1 existing model/UI edge mismatch |
| Native `--check-port-links` | 108 existing catalogue/live-node mismatches |

The first native test build failed because `test_autosave.cpp` and
`test_graph_editor.cpp` included the removed
`hesiod/model/graph/graph_manager.hpp` path. Commit `fc60ba14` changes only those
two includes to `hesiod/model/graph_manager/graph_manager.hpp`. The corrected
suite builds and passes; no forwarding shim or production change is involved.

## Known failures

**Python:** Sixteen HSD tests still reference removed `NoiseFbm`/`NoiseIq`
catalogue entries. They are spread across `test_catalog` (3), `test_cli` (4),
`test_compile` (2), `test_enums` (2), `test_inspect` (1), `test_integration` (2),
and `test_validate` (2). The seventeenth failure is
`test_generate_node_reference.py::test_build_nav_summary_groups_by_primary_and_secondary`:
it expects `* [Categories](categories.md)` as the first line, while the
implementation returns `* [Node Reference](index.md)`. Before a binary was
available, 14 of these failures were observed; the other three require a binary
and fail while compiling their stale fixture, before executing Hesiod.

**Port catalogue:** `--check-port-links` reports ExportForest 15, ExportRocks 15,
ExportScene 2, ForestProcess 1, NodeIn 15, NodeOut 15, Preview 2, RocksProcess 1,
SeedForest 13, TerrainMerge 15, and TerrainTexture 14 mismatches. These checks use
unchanged upstream application code and documentation.

**GNode:** The initial full run exited 139 with a glibc mutex-owner assertion.
A full rerun passed all 58 tests, but repeating the suite exited 134 with
`std::system_error: Invalid argument` during
`ParallelGraphUpdate.GraphWorkerHoldAndResume`. Running that test alone with
`--gtest_repeat=20` exited 139 during iteration 8, after its test body reported
success. This is a reproducible intermittent lifecycle failure, not a clean
pass. The pinned GNode revision is `ae0e10b07c01bc8130c01d96fe1994a5f4a3a2a2`.

A bounded standalone ASan/UBSan Debug build subsequently reported
**stack-use-after-return** in `GraphWorker::start_next`, `graph_worker.cpp:390`:
a background thread writes `start_next_pending` in the expired test-local
`worker` object (`parallel_update.cpp:478`). Its creation stack passes through
`schedule_start_next` (:383), `request` (:371), and the test's request (:489).
The isolated test exited 134 during iteration 11 of at most 20, under a
120-second timeout. The diagnostic establishes this dependency's default-callback
test failure; it does not establish that the same path fails inside Hesiod.
No production fix was attempted. Exact commands, full sanitizer stack and UTC
timestamps are retained in the archive's `gnode-sanitizers/` directory and local
`output/validation/gnode-sanitizers/`.

**CLWrapper:** `DeviceManagerTest.SetDeviceTypeFilterAutoRediscoveryTest` requires
`CL_DEVICE_TYPE_CPU`; this host exposes only a GPU through its OpenCL ICD. The
exception is “No OpenCL devices matching the device type filter were found!”

**Fixture lint:** `Hesiod/data/bootstraps/stepping_shore.hsd` repeats
`10.output -> 17.input` in its UI edge list but contains it once in its model.
The remaining 270 fixtures pass the static consistency check. This does not
establish that every example computes correctly.

## Reproducing the checks

With the normal build dependencies installed, configure `build/assessment` with
`HESIOD_ENABLE_TESTS=ON`, `HESIOD_ENABLE_UI_TESTS=ON`,
`HESIOD_ENABLE_GENERATE_APP_IMAGE=OFF`, `HESIOD_ENABLE_TRACY=OFF`, and
`CMAKE_BUILD_TYPE=RelWithDebInfo`. This GCC build also used
`CMAKE_C_FLAGS=-std=gnu17` for the vendored Triangle C code. Nix required explicit
`OpenEXR_DIR` and `Imath_DIR` paths, recorded in the local CMake cache.

```sh
python3 -m unittest discover -s tests/profiling -v
HESIOD_BIN="$PWD/build/assessment/bin/hesiod" QT_QPA_PLATFORM=offscreen \
  python3 -m pytest tests -q
ctest --test-dir build/assessment --output-on-failure --timeout 180
(cd build/assessment/bin && QT_QPA_PLATFORM=offscreen ./hesiod_ui_palette_test)
(cd build/assessment/bin && QT_QPA_PLATFORM=offscreen ./hesiod --check-port-links)
(cd docs/profiling/baselines/2026-10-08 && sha256sum -c SHA256SUMS)
```

The Python suite requires `pytest` and `mdutils`. Runs used separate
`XDG_CONFIG_HOME` and `XDG_CACHE_HOME` directories beneath `output/validation`.

For dependency tests, local CMake project hooks enabled HighMap and Meta tests
without modifying their sources. The following reproduces that configuration:

```sh
mkdir -p build/assessment-validation
printf 'set(HIGHMAP_ENABLE_TESTS ON)\n' > build/assessment-validation/highmap.cmake
printf 'set(META_ENABLE_TESTS ON)\nset(META_ENABLE_UNITTESTS ON)\n' > build/assessment-validation/meta.cmake
cmake -S . -B build/assessment \
  -DCMAKE_PROJECT_highmap-root_INCLUDE="$PWD/build/assessment-validation/highmap.cmake" \
  -DCMAKE_PROJECT_meta-root_INCLUDE="$PWD/build/assessment-validation/meta.cmake" \
  -DGNODE_ENABLE_UNIT_TESTS=ON -DCLWRAPPER_ENABLE_TESTS=ON
QT_QPA_PLATFORM=offscreen cmake --build build/assessment --parallel 8 \
  --target highmap_tests gnode_unit_tests meta_unittests clwrapper_unit_tests
```

GoogleTest was supplied through `FETCHCONTENT_SOURCE_DIR_GOOGLETEST` from revision
`a46a6012a133c7c0929fdae55b321c2fe65f3c5f`. Run the built executables separately;
HighMap and GNode are not registered with CTest:

```sh
(cd build/assessment/bin && OMP_NUM_THREADS=1 ./highmap_tests)
(cd build/assessment/bin && QT_QPA_PLATFORM=offscreen ./meta_unittests)
(cd build/assessment/bin && ./clwrapper_unit_tests)
ulimit -c 0
OMP_NUM_THREADS=1 build/assessment/bin/gnode_unit_tests \
  --gtest_filter=ParallelGraphUpdate.GraphWorkerHoldAndResume --gtest_repeat=20
valgrind --tool=memcheck --error-exitcode=99 --leak-check=full \
  --show-leak-kinds=definite --errors-for-leak-kinds=definite \
  build/assessment/bin/profile_highmap
```

Valgrind version was 3.27.1. Dependency tests and native compilation finished
before the allocation study and resolution-scaling matrix. The later standalone
GNode diagnostic used two compiler workers from 00:31:56 to 00:33:10 UTC and ran
at 00:33:20 UTC on November 27, overlapping the native RSS matrix. This is
additional wall-time contention; the diagnostic does not alter the measured
Hesiod process's own RSS. Test runtimes are validation observations, not
performance benchmarks.

The sanitizer follow-up uses a separate build and leaves Hesiod untouched:

```sh
cmake -S external/GNode -B build/gnode-sanitizers -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DGNODE_ENABLE_UNIT_TESTS=ON \
  -DGNODE_ENABLE_TESTS=OFF -DGNODE_ENABLE_EXAMPLES=OFF \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/output/validation/googletest" \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'
cmake --build build/gnode-sanitizers --parallel 2 --target gnode_unit_tests
ulimit -c 0
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1:detect_stack_use_after_return=1:fast_unwind_on_malloc=0 \
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 OMP_NUM_THREADS=1 \
  timeout 120 build/gnode-sanitizers/bin/gnode_unit_tests \
  --gtest_filter=ParallelGraphUpdate.GraphWorkerHoldAndResume --gtest_repeat=20
```
