#!/usr/bin/env python3
"""
Build Time Profiler and Aggregation Tool for Hesiod.

Analyzes:
1. Micro level: -ftime-trace JSON outputs (translation units, header parse times, template instantiations)
2. Macro level (optional): build logs if available
"""

import argparse
import glob
import json
import os
import sys
from collections import defaultdict


def parse_build_log(build_dir, top_n=20):
    """Parse .hesiod_build_times.log or .ninja_log and return sorted list of longest compiling targets."""
    # 1. Check custom wrapper log (.hesiod_build_times.log)
    wrapper_log = os.path.join(build_dir, ".hesiod_build_times.log")
    if os.path.exists(wrapper_log):
        targets = []
        with open(wrapper_log, "r", encoding="utf-8", errors="replace") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                parts = line.split("\t")
                if len(parts) >= 2:
                    try:
                        dur_ms = int(parts[0])
                        src_path = parts[1]
                        targets.append((dur_ms / 1000.0, src_path))
                    except ValueError:
                        continue
        targets.sort(key=lambda x: x[0], reverse=True)
        return targets[:top_n], "compiler launcher log"

    # 2. Check .ninja_log
    ninja_log = os.path.join(build_dir, ".ninja_log")
    if os.path.exists(ninja_log):
        targets = []
        with open(ninja_log, "r", encoding="utf-8", errors="replace") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                parts = line.split("\t")
                if len(parts) >= 4:
                    try:
                        start_ms = int(parts[0])
                        end_ms = int(parts[1])
                        targets.append(((end_ms - start_ms) / 1000.0, parts[3]))
                    except ValueError:
                        continue
        targets.sort(key=lambda x: x[0], reverse=True)
        return targets[:top_n], ".ninja_log"

    return None, None


def analyze_time_traces(build_dir, top_n=20):
    """Find and aggregate all -ftime-trace JSON trace files in build_dir."""
    trace_files = glob.glob(f"{build_dir}/**/*.json", recursive=True)
    valid_traces = []
    tu_durations = []
    header_times = defaultdict(lambda: {"total_us": 0, "count": 0})
    template_times = defaultdict(lambda: {"total_us": 0, "count": 0})

    for path in trace_files:
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as f:
                data = json.load(f)
                if not isinstance(data, dict) or "traceEvents" not in data:
                    continue
                valid_traces.append(path)
                events = data.get("traceEvents", [])

                tu_total_us = 0
                for ev in events:
                    name = ev.get("name", "")
                    dur = ev.get("dur", 0)
                    args = ev.get("args", {})
                    detail = args.get("detail", "")

                    if name == "ExecuteCompiler":
                        tu_total_us = max(tu_total_us, dur)
                    elif name == "Source":
                        # Header / Source file parsing
                        header_name = detail if detail else args.get("name", "")
                        if header_name:
                            header_times[header_name]["total_us"] += dur
                            header_times[header_name]["count"] += 1
                    elif name == "InstantiateFunction" or name == "InstantiateClass":
                        tmpl_name = detail if detail else name
                        if tmpl_name:
                            template_times[tmpl_name]["total_us"] += dur
                            template_times[tmpl_name]["count"] += 1

                if tu_total_us > 0:
                    tu_durations.append((tu_total_us / 1000000.0, path))
        except Exception:
            continue

    tu_durations.sort(key=lambda x: x[0], reverse=True)

    sorted_headers = sorted(
        header_times.items(),
        key=lambda item: item[1]["total_us"],
        reverse=True
    )[:top_n]

    sorted_templates = sorted(
        template_times.items(),
        key=lambda item: item[1]["total_us"],
        reverse=True
    )[:top_n]

    return {
        "num_traces": len(valid_traces),
        "top_tus": tu_durations[:top_n],
        "top_headers": sorted_headers,
        "top_templates": sorted_templates,
    }


def print_report(build_dir, build_top, log_source, trace_data, top_n):
    """Print a clean terminal report of build profile results."""
    print("=" * 80)
    print(" HESIOD BUILD TIME PROFILING REPORT")
    print("=" * 80)
    print(f"Build Directory: {build_dir}\n")

    if build_top:
        print(f"--- Top {len(build_top)} Slowest Translation Units ({log_source}) ---")
        print(f"{'Duration (s)':>12} | Source / Target")
        print("-" * 80)
        for dur, target in build_top:
            rel = os.path.relpath(target, os.getcwd()) if os.path.isabs(target) else target
            print(f"{dur:11.2f}s | {rel}")
        print()

    if trace_data and trace_data["num_traces"] > 0:
        print(f"--- Top {len(trace_data['top_tus'])} Slowest Translation Units (-ftime-trace) ---")
        print(f"{'Duration (s)':>12} | Trace File")
        print("-" * 80)
        for dur, trace_path in trace_data["top_tus"]:
            rel_path = os.path.relpath(trace_path, build_dir)
            print(f"{dur:11.2f}s | {rel_path}")
        print()

        print(f"--- Top {len(trace_data['top_headers'])} Most Expensive Headers (Cumulative Parse Time) ---")
        print(f"{'Total (s)':>10} | {'Count':>7} | {'Avg (ms)':>9} | Header")
        print("-" * 80)
        for header, stats in trace_data["top_headers"]:
            total_s = stats["total_us"] / 1000000.0
            count = stats["count"]
            avg_ms = (stats["total_us"] / count) / 1000.0 if count > 0 else 0
            print(f"{total_s:9.2f}s | {count:7d} | {avg_ms:8.2f}ms | {header}")
        print()

        print(f"--- Top {len(trace_data['top_templates'])} Most Expensive Template Instantiations ---")
        print(f"{'Total (s)':>10} | {'Count':>7} | Template")
        print("-" * 80)
        for tmpl, stats in trace_data["top_templates"]:
            total_s = stats["total_us"] / 1000000.0
            count = stats["count"]
            print(f"{total_s:9.2f}s | {count:7d} | {tmpl[:55]}")
        print()
    elif not build_top:
        print("(No build timing logs or -ftime-trace JSON files found)\n")

    print("=" * 80)


def main():
    parser = argparse.ArgumentParser(description="Hesiod Build Time Profiler & Aggregator")
    parser.add_argument("build_dir", nargs="?", default="build", help="Path to build directory (default: build)")
    parser.add_argument("-n", "--top", type=int, default=20, help="Number of top entries to display (default: 20)")
    parser.add_argument("--json-out", type=str, default=None, help="Save aggregated results to a JSON file")

    args = parser.parse_args()
    build_dir = os.path.abspath(args.build_dir)

    if not os.path.isdir(build_dir):
        print(f"Error: Build directory '{build_dir}' does not exist.", file=sys.stderr)
        sys.exit(1)

    build_top, log_source = parse_build_log(build_dir, top_n=args.top)
    trace_data = analyze_time_traces(build_dir, top_n=args.top)

    print_report(build_dir, build_top, log_source, trace_data, args.top)

    if args.json_out:
        out_data = {
            "top_targets": [{"duration_s": d, "target": t} for d, t in (build_top or [])],
            "trace_data": {
                "num_traces": trace_data.get("num_traces", 0),
                "top_tus": [{"duration_s": d, "path": p} for d, p in trace_data.get("top_tus", [])],
                "top_headers": [
                    {"header": h, "total_s": s["total_us"] / 1e6, "count": s["count"]}
                    for h, s in trace_data.get("top_headers", [])
                ],
                "top_templates": [
                    {"template": t, "total_s": s["total_us"] / 1e6, "count": s["count"]}
                    for t, s in trace_data.get("top_templates", [])
                ],
            }
        }
        with open(args.json_out, "w", encoding="utf-8") as f:
            json.dump(out_data, f, indent=2)
        print(f"Saved aggregated JSON report to: {args.json_out}")


if __name__ == "__main__":
    main()
