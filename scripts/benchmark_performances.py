#!/usr/bin/env python3

import argparse
import csv
import ctypes
import glob
import os
import subprocess
import sys
import time


class GpuMonitor:
    # --- GPU monitoring via NVML

    def __init__(self):
        self.available = False
        self.nvml = None
        self.handle = None

        # define C structures for NVML
        class Memory(ctypes.Structure):
            _fields_ = [
                ("total", ctypes.c_ulonglong),
                ("free", ctypes.c_ulonglong),
                ("used", ctypes.c_ulonglong),
            ]

        self.memory_struct = Memory
        self._init_nvml()

    def _init_nvml(self):
        # try loading libnvidia-ml
        for lib_name in ["libnvidia-ml.so.1", "libnvidia-ml.so"]:
            try:
                self.nvml = ctypes.CDLL(lib_name)
                res = self.nvml.nvmlInit_v2()
                if res == 0:
                    self.handle = ctypes.c_void_p()
                    res_handle = self.nvml.nvmlDeviceGetHandleByIndex_v2(0, ctypes.byref(self.handle))
                    if res_handle == 0:
                        self.available = True
                        break
            except Exception:
                continue

    def get_used_memory_mb(self):
        # get current used GPU memory in MB
        if not self.available:
            return None
        try:
            mem = self.memory_struct()
            res = self.nvml.nvmlDeviceGetMemoryInfo(self.handle, ctypes.byref(mem))
            if res == 0:
                return mem.used / (1024.0 * 1024.0)
        except Exception:
            pass
        return None

    def close(self):
        # shutdown NVML
        if self.available and self.nvml:
            try:
                self.nvml.nvmlShutdown()
            except Exception:
                pass
            self.available = False


def parse_arguments():
    # --- Argument parsing

    parser = argparse.ArgumentParser(
        description="Benchmark Hesiod node execution times, CPU memory, and GPU memory usage across shapes."
    )
    parser.add_argument(
        "--binary",
        default="./bin/hesiod",
        help="Path to the hesiod binary (relative to build dir or absolute).",
    )
    parser.add_argument(
        "--build-dir",
        default="build",
        help="Working directory to execute the benchmark in.",
    )
    parser.add_argument(
        "--benchmarks-dir",
        default="Hesiod/data/benchmarks",
        help="Directory containing benchmark .hsd files.",
    )
    parser.add_argument(
        "--files",
        nargs="*",
        default=None,
        help="Specific .hsd files to execute (default: all in benchmarks-dir).",
    )
    parser.add_argument(
        "--tiling",
        default="1,1",
        help="Fixed global tiling parameter (e.g. 1,1).",
    )
    parser.add_argument(
        "--min-power",
        type=int,
        default=8,
        help="Minimum power of two for shape (2^8 = 256).",
    )
    parser.add_argument(
        "--max-power",
        type=int,
        default=18,
        help="Maximum power of two for shape (2^11 = 2048).",
    )
    parser.add_argument(
        "-n",
        "--runs",
        type=int,
        default=1,
        help="Number of executions per configuration for averaging.",
    )
    parser.add_argument(
        "--force-distributed",
        action="store_true",
        help="Force distributed computation mode for all compute modes (CPU and GPU).",
    )
    parser.add_argument(
        "--force-sequential",
        action="store_true",
        help="Force sequential computation mode for all compute modes (CPU and GPU).",
    )
    parser.add_argument(
        "--disk-cache",
        "--cache-data-on-disk",
        action="store_true",
        dest="disk_cache",
        help="Cache data on disk (storage mode: VA_DISK_LRU) instead of keeping in RAM.",
    )
    parser.add_argument(
        "-o",
        "--output",
        default="benchmark_results.csv",
        help="Path to output CSV file.",
    )

    return parser.parse_args()


def get_benchmark_files(benchmarks_dir, file_args):
    # --- Locate benchmark files

    if file_args:
        files = []
        for file_arg in file_args:
            if os.path.exists(file_arg):
                files.append(os.path.abspath(file_arg))
            else:
                candidate = os.path.join(benchmarks_dir, file_arg)
                if os.path.exists(candidate):
                    files.append(os.path.abspath(candidate))
                else:
                    print(f"Warning: file not found: {file_arg}", file=sys.stderr)
        return files

    pattern = os.path.join(benchmarks_dir, "*.hsd")
    files = sorted(glob.glob(pattern))
    return [os.path.abspath(f) for f in files]


def parse_batch_log(log_path):
    # parse batch.log data by node type
    node_times = {}
    if not os.path.exists(log_path):
        return None

    try:
        with open(log_path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                parts = line.split("|")
                if len(parts) < 3:
                    continue
                # parts: [id, node_type, update_time]
                node_type = parts[1].strip()
                time_str = parts[-1].strip().replace(",", ".")
                try:
                    exec_time = float(time_str)
                except ValueError:
                    continue

                if node_type not in node_times:
                    node_times[node_type] = 0.0
                node_times[node_type] += exec_time
        return node_times
    except Exception as e:
        print(f"Error reading {log_path}: {e}", file=sys.stderr)
        return None


def execute_benchmark_run(
    binary_path,
    build_dir,
    hsd_file,
    tiling,
    shape_str,
    gpu_monitor,
    force_distributed=False,
    force_sequential=False,
    disk_cache=False,
):
    # execute one run with hesiod cli and monitor peak host and GPU memory
    batch_log_path = os.path.join(build_dir, "batch.log")
    if os.path.exists(batch_log_path):
        try:
            os.remove(batch_log_path)
        except OSError:
            pass

    args_list = [
        binary_path,
        "-b",
        hsd_file,
        f"--tiling={tiling}",
        f"--shape={shape_str}",
    ]
    if force_distributed:
        args_list.append("--force-distributed")
    if force_sequential:
        args_list.append("--force-sequential")
    if disk_cache:
        args_list.append("--cache-data-on-disk")

    try:
        proc = subprocess.Popen(
            args_list,
            cwd=build_dir,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

        peak_host_rss_kb = 0
        peak_gpu_mem_mb = 0.0 if gpu_monitor.available else None
        status_file = f"/proc/{proc.pid}/status"

        # sample host and GPU memory while process is executing
        while proc.poll() is None:
            if gpu_monitor.available:
                gpu_used = gpu_monitor.get_used_memory_mb()
                if gpu_used is not None and gpu_used > peak_gpu_mem_mb:
                    peak_gpu_mem_mb = gpu_used

            try:
                with open(status_file, "r") as f:
                    for line in f:
                        if line.startswith("VmHWM:"):
                            peak_host_rss_kb = max(peak_host_rss_kb, int(line.split()[1]))
                            break
            except Exception:
                pass

            time.sleep(0.002)

        exit_code = proc.wait()
        peak_host_mb = peak_host_rss_kb / 1024.0 if peak_host_rss_kb > 0 else None

        if exit_code != 0:
            if exit_code < 0 or exit_code >= 128:
                print(f"Process killed/terminated (exit code {exit_code})")
            else:
                print(f"Process failed (exit code {exit_code})")
            if not os.path.exists(batch_log_path):
                return None, peak_host_mb, peak_gpu_mem_mb

        if not os.path.exists(batch_log_path):
            print("batch.log was not generated")
            return None, peak_host_mb, peak_gpu_mem_mb

        node_times = parse_batch_log(batch_log_path)
        return node_times, peak_host_mb, peak_gpu_mem_mb

    except Exception as e:
        print(f"Execution error: {e}", file=sys.stderr)
        return None, None, None


def run_benchmarks(args):
    # --- Benchmark execution loop

    build_dir = os.path.abspath(args.build_dir)
    if not os.path.exists(build_dir):
        print(f"Error: Build directory '{build_dir}' does not exist.", file=sys.stderr)
        sys.exit(1)

    binary_path = args.binary
    if not os.path.isabs(binary_path):
        binary_path = os.path.abspath(os.path.join(build_dir, binary_path))
    if not os.path.exists(binary_path):
        print(f"Error: Hesiod binary '{binary_path}' not found.", file=sys.stderr)
        sys.exit(1)

    benchmarks_dir = os.path.abspath(args.benchmarks_dir)
    benchmark_files = get_benchmark_files(benchmarks_dir, args.files)

    if not benchmark_files:
        print(f"No .hsd files found in '{benchmarks_dir}'.", file=sys.stderr)
        sys.exit(1)

    gpu_monitor = GpuMonitor()
    if gpu_monitor.available:
        print("GPU monitoring enabled (NVIDIA NVML active).")
    else:
        print("GPU monitoring not available (NVML not detected).")

    print(f"Found {len(benchmark_files)} benchmark file(s):")
    for f in benchmark_files:
        print(f"  - {os.path.basename(f)}")

    shapes = [2**p for p in range(args.min_power, args.max_power + 1)]
    shape_labels = [f"{s}x{s}" for s in shapes]

    # results structure: { (file_basename, node_type_or_metric): { shape_label: value } }
    results = {}
    node_types_per_file = {}

    try:
        for hsd_file in benchmark_files:
            file_name = os.path.basename(hsd_file)
            print(f"\n==========================================")
            print(f"Benchmarking: {file_name}")
            print(f"==========================================")

            file_node_types = set()

            for shape in shapes:
                shape_str = f"{shape},{shape}"
                shape_label = f"{shape}x{shape}"
                print(f"\n--- Shape: {shape_label} (Tiling: {args.tiling}, Runs: {args.runs}) ---")

                run_timings = {}  # { node_type: [time_run1, time_run2, ...] }
                host_memories = []  # [mem_run1, mem_run2, ...]
                gpu_memories = []  # [mem_run1, mem_run2, ...]
                successful_runs = 0
                killed = False

                for run_idx in range(1, args.runs + 1):
                    print(f"  Run {run_idx}/{args.runs}...", end=" ", flush=True)
                    node_times, peak_host_mb, peak_gpu_mb = execute_benchmark_run(
                        binary_path=binary_path,
                        build_dir=build_dir,
                        hsd_file=hsd_file,
                        tiling=args.tiling,
                        shape_str=shape_str,
                        gpu_monitor=gpu_monitor,
                        force_distributed=args.force_distributed,
                        force_sequential=args.force_sequential,
                        disk_cache=args.disk_cache,
                    )

                    if peak_host_mb is not None:
                        host_memories.append(peak_host_mb)
                    if peak_gpu_mb is not None:
                        gpu_memories.append(peak_gpu_mb)

                    if node_times is None:
                        print("FAILED / KILLED (batch.log unavailable)")
                        killed = True
                        break
                    else:
                        host_str = f"Host RSS: {peak_host_mb:.1f} MB" if peak_host_mb else ""
                        gpu_str = f"GPU Mem: {peak_gpu_mb:.1f} MB" if peak_gpu_mb else ""
                        metrics_info = ", ".join(filter(None, [host_str, gpu_str]))
                        info_suffix = f" ({metrics_info})" if metrics_info else ""
                        print(f"OK{info_suffix}")

                        successful_runs += 1
                        for node_type, t in node_times.items():
                            file_node_types.add(node_type)
                            if node_type not in run_timings:
                                run_timings[node_type] = []
                            run_timings[node_type].append(t)

                for node_type in file_node_types:
                    key = (file_name, node_type)
                    if key not in results:
                        results[key] = {}

                    if killed or successful_runs == 0:
                        results[key][shape_label] = "KILLED"
                    else:
                        times = run_timings.get(node_type, [])
                        if times:
                            avg_time = sum(times) / len(times)
                            results[key][shape_label] = round(avg_time, 4)
                        else:
                            results[key][shape_label] = "N/A"

                # record host peak memory
                host_mem_key = (file_name, "Peak Host Memory (MB)")
                if host_mem_key not in results:
                    results[host_mem_key] = {}
                if host_memories:
                    avg_host = sum(host_memories) / len(host_memories)
                    results[host_mem_key][shape_label] = round(avg_host, 2)
                else:
                    results[host_mem_key][shape_label] = "N/A"

                # record GPU peak memory
                if gpu_monitor.available:
                    gpu_mem_key = (file_name, "Peak GPU Memory (MB)")
                    if gpu_mem_key not in results:
                        results[gpu_mem_key] = {}
                    if gpu_memories:
                        avg_gpu = sum(gpu_memories) / len(gpu_memories)
                        results[gpu_mem_key][shape_label] = round(avg_gpu, 2)
                    else:
                        results[gpu_mem_key][shape_label] = "N/A"

            node_types_per_file[file_name] = sorted(list(file_node_types))

    finally:
        gpu_monitor.close()

    return results, shape_labels, node_types_per_file


def write_csv_output(output_path, results, shape_labels):
    # --- Write results to CSV

    fieldnames = ["file", "node_type"] + shape_labels

    with open(output_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()

        for (file_name, node_type), shape_data in sorted(results.items()):
            row = {"file": file_name, "node_type": node_type}
            for shape_label in shape_labels:
                row[shape_label] = shape_data.get(shape_label, "N/A")
            writer.writerow(row)

    print(f"\nBenchmark results written to: {output_path}")


def main():
    args = parse_arguments()
    results, shape_labels, _ = run_benchmarks(args)
    write_csv_output(args.output, results, shape_labels)


if __name__ == "__main__":
    main()


