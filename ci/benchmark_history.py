#!/usr/bin/env python3

import argparse
import json
import os
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path

# TODO: Add regression tests for four-axis filtering, rerun deduplication, and
# per-series retention.


def benchmark_key(entry):
    # Preserve the stage history across the reduce_matches -> fold_matches
    # terminology change without rewriting retained history data.
    return (
        {"tool reduce matches": "tool fold matches"}.get(
            entry["name"], entry["name"]),
        entry.get("unit", ""),
    )


def run_identity(run):
    return (
        run["system"],
        run["architecture"],
        run["libc"],
        run["toolchain"],
    )


def validate_run(run):
    # The CI benchmark matrix is the single source of truth for valid systems,
    # architectures, libc values, and system-specific toolchains. History only
    # requires every selected axis to be recorded.
    for axis in ("system", "architecture", "libc", "toolchain"):
        value = run.get(axis)
        if not isinstance(value, str) or not value:
            raise ValueError(f"benchmark run has no {axis}")


def github_escape(value):
    return str(value).replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")


def format_value(value, unit):
    return f"{value:.3f} {unit}".rstrip()


def format_delta(delta, unit):
    if "ms" == unit:
        if 1000 <= abs(delta):
            return f"{delta / 1000:.3f} s"

        return f"{delta:.3f} ms"

    if "%" == unit:
        return f"{delta:.3f} percentage points"

    return format_value(delta, unit)


def compare_history(args):
    current = json.loads(Path(args.current).read_text())
    history_path = Path(args.history)
    history_exists = history_path.exists()
    runs = (
        json.loads(history_path.read_text()).get("runs", [])
        if history_exists
        else []
    )
    if not history_exists:
        print(
            "::notice title=Benchmark baseline::"
            f"{github_escape(f'benchmark history does not exist: {history_path}')}")

    series_by_key = defaultdict(list)
    for run in runs:
        validate_run(run)
        if (
            args.system,
            args.architecture,
            args.libc,
            args.toolchain,
        ) != run_identity(run):
            continue

        for entry in run.get("benchmarks", []):
            series_by_key[benchmark_key(entry)].append(float(entry["value"]))

    if not series_by_key:
        print(
            "::notice title=Benchmark baseline::"
            "benchmark history has no usable baseline entries")
        return

    warning_metrics = {
        ("reflection wall", "ms"),
        ("tool/build", "%"),
    }
    regressions = []
    increased_details = []
    missing_baseline = []
    for entry in current:
        key = benchmark_key(entry)
        previous = series_by_key.get(key, [])[-args.count:]
        if not previous:
            missing_baseline.append(entry["name"])
            continue

        baseline = sum(previous) / len(previous)
        current_value = float(entry["value"])
        if 0 >= baseline or current_value <= baseline:
            continue

        change = 100 * (current_value / baseline - 1)
        delta = current_value - baseline
        result = {
            "name": entry["name"],
            "unit": entry.get("unit", ""),
            "current": current_value,
            "baseline": baseline,
            "count": len(previous),
            "change": change,
            "delta": delta,
        }

        if key not in warning_metrics:
            increased_details.append(result)
            continue

        if current_value <= baseline * args.threshold:
            continue

        if "ms" == entry.get("unit", "") and delta < args.min_delta_ms:
            increased_details.append(result)
            continue

        regressions.append(result)

    for r in regressions:
        message = (
            f"{r['name']}: +{format_delta(r['delta'], r['unit'])} "
            f"(+{r['change']:.1f}%) over avg(last {r['count']}) "
            f"{format_value(r['baseline'], r['unit'])}; current "
            f"{format_value(r['current'], r['unit'])}")
        print(f"::warning title=Benchmark regression::{github_escape(message)}")

    for r in increased_details:
        message = (
            f"{r['name']}: +{format_delta(r['delta'], r['unit'])} "
            f"(+{r['change']:.1f}%) over avg(last {r['count']}) "
            f"{format_value(r['baseline'], r['unit'])}; current "
            f"{format_value(r['current'], r['unit'])}")
        print(f"::notice title=Benchmark detail::{github_escape(message)}")

    if missing_baseline:
        print(
            "::notice title=Benchmark baseline::"
            f"{github_escape('no baseline for: ' + ', '.join(missing_baseline))}")

    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary and (regressions or increased_details):
        with Path(summary).open("a") as f:
            if regressions:
                f.write("## Benchmark Regressions\n\n")
                f.write("| metric | delta | current | avg previous | change |\n")
                f.write("| --- | ---: | ---: | ---: | ---: |\n")
                for r in regressions:
                    f.write(
                        f"| {r['name']} | +{format_delta(r['delta'], r['unit'])} | "
                        f"{format_value(r['current'], r['unit'])} | "
                        f"{format_value(r['baseline'], r['unit'])} | "
                        f"+{r['change']:.1f}% |\n")

            if increased_details:
                f.write("\n## Benchmark Details\n\n")
                f.write("| metric | delta | current | avg previous | change |\n")
                f.write("| --- | ---: | ---: | ---: | ---: |\n")
                for r in increased_details:
                    f.write(
                        f"| {r['name']} | +{format_delta(r['delta'], r['unit'])} | "
                        f"{format_value(r['current'], r['unit'])} | "
                        f"{format_value(r['baseline'], r['unit'])} | "
                        f"+{r['change']:.1f}% |\n")


def record_run(args):
    current = json.loads(Path(args.current).read_text())
    run = {
        "commit": args.commit,
        "system": args.system,
        "architecture": args.architecture,
        "libc": args.libc,
        "toolchain": args.toolchain,
        "run_id": os.environ.get("GITHUB_RUN_ID", ""),
        "created_at": datetime.now(timezone.utc).isoformat(),
        "benchmarks": current,
    }
    if args.commit_title:
        run["commit_title"] = args.commit_title
    validate_run(run)

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(run, indent=2) + "\n")


def merge_history(args):
    history_path = Path(args.history)
    history_path.parent.mkdir(parents=True, exist_ok=True)
    history = (
        json.loads(history_path.read_text())
        if history_path.exists()
        else {"runs": []}
    )
    for run in history.get("runs", []):
        validate_run(run)

    incoming_by_key = {}
    for path in args.run:
        run = json.loads(Path(path).read_text())
        validate_run(run)
        key = (run["commit"], *run_identity(run))
        previous = incoming_by_key.get(key)
        if previous is None or previous["created_at"] < run["created_at"]:
            incoming_by_key[key] = run

    incoming = list(incoming_by_key.values())
    incoming.sort(key=lambda run: (*run_identity(run), run["commit"]))
    replaced = {(run["commit"], *run_identity(run)) for run in incoming}
    runs = [
        run for run in history.get("runs", [])
        if (run["commit"], *run_identity(run)) not in replaced
    ]
    runs.extend(incoming)

    retained = []
    series_counts = defaultdict(int)
    for run in reversed(runs):
        identity = run_identity(run)
        if args.max_runs <= series_counts[identity]:
            continue

        series_counts[identity] += 1
        retained.append(run)

    history["runs"] = list(reversed(retained))

    history_path.write_text(json.dumps(history, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description="Omnirefl benchmark history.")
    subcommands = parser.add_subparsers(dest="command", required=True)

    compare_parser = subcommands.add_parser("compare")
    compare_parser.add_argument("--current", required=True)
    compare_parser.add_argument("--history", required=True)
    compare_parser.add_argument("--system", required=True)
    compare_parser.add_argument("--architecture", required=True)
    compare_parser.add_argument("--libc", required=True)
    compare_parser.add_argument("--toolchain", required=True)
    compare_parser.add_argument("--threshold", type=float, default=1.2)
    compare_parser.add_argument("--min-delta-ms", type=float, default=500)
    compare_parser.add_argument("--count", type=int, default=5)
    compare_parser.set_defaults(func=compare_history)

    record_parser = subcommands.add_parser("record")
    record_parser.add_argument("--current", required=True)
    record_parser.add_argument("--output", required=True)
    record_parser.add_argument("--commit", required=True)
    record_parser.add_argument("--commit-title", default="")
    record_parser.add_argument("--system", required=True)
    record_parser.add_argument("--architecture", required=True)
    record_parser.add_argument("--libc", required=True)
    record_parser.add_argument("--toolchain", required=True)
    record_parser.set_defaults(func=record_run)

    merge_parser = subcommands.add_parser("merge")
    merge_parser.add_argument("--history", required=True)
    merge_parser.add_argument("--run", required=True, nargs="+")
    merge_parser.add_argument("--max-runs", type=int, default=200)
    merge_parser.set_defaults(func=merge_history)

    args = parser.parse_args()
    args.func(args)


if "__main__" == __name__:
    main()
