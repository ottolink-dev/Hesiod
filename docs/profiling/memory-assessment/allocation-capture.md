# Reproducing allocation attribution

Use a Linux RelWithDebInfo build, heaptrack and heaptrack_print. Run native RSS
measurements separately: heaptrack changes both resident memory and runtime.
The capture runner uses the same deterministic full/minimal input fixtures,
CPU policy and isolated application configuration as `memory_study.py`.
Sequential tile processing and one OpenMP thread do not serialize independent
graph source nodes; the graph scheduler can still run those concurrently.

After preparing a native study, use its frozen binary so both series have the
same executable and application data. For example:

```sh
python scripts/profiling/capture_allocations.py \
  --binary output/memory-study/bin/hesiod \
  --output output/allocations/ExportAsset-full-512-t1 \
  --case ExportAsset --variant full --size 512 --tiles 1 --overlap 0 \
  --policy ram-sequential --timeout 900 --max-rss-mib 8192
```

Supply `--heaptrack /path/to/heaptrack --printer /path/to/heaptrack_print` if those
tools are not on PATH. Use a new output directory for every capture. Swap
`--case` to `TextureQuiltingShuffle` or `TextureQuiltingExpand`, `--variant` to
`minimal`, and `--size` to 1024 for the focused attribution matrix. Compare
`--tiles 4 --overlap 0` without changing the quilting algorithm's patch settings.

Each directory retains the exact fixture, application log, result/provenance,
script snapshot and raw heaptrack capture. It validates unique expected node entries with strictly positive completed-worker
runtimes in `batch.log`, error-free application logs, and effective configuration; ExportAsset also requires a structurally
valid GLB and PNG dimensions. Quilting numerical correctness is not established
by memory captures. A timeout, resource stop, signal interruption, missing output
or heaptrack analysis error is a failed capture, never a zero-memory result.
The RSS guard is sampled process-group residency, not a kernel-enforced cap.

`analysis/allocations.txt` is generated with **`--merge-backtraces=0`**. Heaptrack's
CLI warns that merged peak consumption is incorrect; merged reports are not
used for quantitative attribution here. Per-stack high-water marks can occur at
different times and must not be added to derive a global peak.

`analysis/peak-stacks.txt` is generated with **`--flamegraph-cost-type=peak`**.
This contains exclusive allocation bytes still live at the highest total
intercepted heap consumption. The JSON counterpart preserves every stack and
byte count. Summing these exclusive counts gives
`intercepted_heap_at_global_peak_bytes`; grouping stacks by call path can explain
which operations own memory at that same instant. Different grouping totals
must not overlap. Inlined source locations are available in the detailed report
when debug information survived linking.

These measurements cover intercepted allocations, not all mappings, allocator
metadata/fragmentation, device allocations or GPU VRAM. An application frame
on an allocation stack establishes the call path that allocated the buffer;
it does not automatically mean that the current function still owns that buffer
or that changing that function will lower RSS by the same number of bytes.
Use the source lifetime audit to interpret those distinctions.
