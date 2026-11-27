# Quilting memory fixes: isolated verification

Baseline: unchanged HighMap d73e05adee1a04cc1e955d3f3162398a37f1c17d archive copied to ../baseline/libhighmap.a before shared integration rebuilds. New binaries link only the two changed quilting/stitching-helper object files before that baseline archive. build.py derives original compiler and linker commands from /tmp/783-profile-commands.txt (read-only Ninja command listing); changed sources use their exact original -Ofast/-ffast-math flags. Other dependency libraries are unchanged. The production shape validation was added after heap captures; it performs no allocation on valid inputs.

compare.cpp emits raw dimensions and every float in the primary output, possibly aliased guide, and every secondary output. baseline.bin and new.bin are byte-identical across 3,240 cases: three square/rectangular/non-divisible shapes, 0/4/20 channels, all eight transform combinations, overlap 0/.5/.9, core quilting with two distinct guide arrays, Shuffle, Expand keep-shape ratio2/1.5 and resize ratio2, independent/duplicate/guide-aliased secondary pointers. A separate strict IEEE audit found zero nonfinite values in 79,868,656 serialized float cells. The matrix includes 1,544 empty arrays out of 32,400 serialized arrays, preserved identically from the baseline; these come from preexisting floor-rounded wrapper coverage and are not evidence of correct nonempty output for those cases. filter_width_ratio=.5 avoids the independently reproduced preexisting radius-zero smoothing bug.

The four heaptrack captures use 1024² input, 20 channels, patch307², overlap.9, transformations enabled, filter_width_ratio=.5, seed123. Method1=Shuffle; method2=Expand keep-shape ratio2. Every saved full output matches the baseline byte-for-byte. Peak intercepted live heap (decimal MB as printed by heaptrack):

| Method | Baseline MB | Fixed MB | Reduction |
| --- | ---: | ---: | ---: |
| Shuffle | 302.15 | 223.96 | 25.9% |
| Expand | 334.09 | 243.10 | 27.2% |

These standalone-library peaks include inputs and stream buffers; they are not Hesiod process RSS and must not be compared directly to previous app peaks. Parent integration compilation was concurrent, so elapsed times are not reliable performance evidence. New workspace retains guide RNG states and seam masks only for the current row, replaying one secondary strip/patch at a time; output moves avoid duplicate final buffers. Sources remain unchanged until all outputs exist, preserving aliases.

Durable tests are in external/HighMap/tests/src/test_quilting.cpp. tests-new.log records 13/13 passing tests, including complete secondary/guide alignment, alias preservation, wrapper alignment, direct helper transformed patch ownership and mismatched-shape rejection without mutation. No production algorithm is copied into tests.

repro.cpp and the *repro*.log files document two preexisting bugs separately: mismatched secondary shapes crash in quilting after get_random_patch skips the channel; the new precondition check handles it. smooth_cpulse(array,0) yields all nonfinite values, and small quilting smoothing radii expose it; this numerical bug remains unchanged.

manifest.json hashes full comparison artifacts. Raw heaptrack captures are small and retained alongside portable symbolized summaries. Executables, objects and hundreds of MiB of duplicate output arrays need not be committed.
