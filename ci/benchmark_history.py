#!/usr/bin/env python3

import argparse
import json
import os
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path


def benchmark_key(entry):
    # Preserve the stage history across the reduce_matches -> fold_matches
    # terminology change without rewriting retained artifact data.
    return (
        {"tool reduce matches": "tool fold matches"}.get(
            entry["name"], entry["name"]),
        entry.get("unit", ""),
    )


def run_libc(run):
    return run.get("libc", "musl")


def run_arch(run):
    return run.get("arch", "x86_64")


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
        if args.os != run.get("os"):
            continue

        if args.libc != run_libc(run):
            continue

        if args.arch != run_arch(run):
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


def update_history(args):
    history_path = Path(args.history)
    history_path.parent.mkdir(parents=True, exist_ok=True)

    current = json.loads(Path(args.current).read_text())
    history = (
        json.loads(history_path.read_text())
        if history_path.exists()
        else {"runs": []}
    )

    runs = [
        run for run in history.get("runs", [])
        if not (
            args.commit == run.get("commit")
            and args.os == run.get("os")
            and args.arch == run_arch(run)
            and args.libc == run_libc(run)
        )
    ]
    run = {
        "commit": args.commit,
        "os": args.os,
        "arch": args.arch,
        "libc": args.libc,
        "run_id": os.environ.get("GITHUB_RUN_ID", ""),
        "created_at": datetime.now(timezone.utc).isoformat(),
        "benchmarks": current,
    }
    if args.commit_title:
        run["commit_title"] = args.commit_title

    runs.append(run)
    history["runs"] = runs[-args.max_runs:]

    history_path.write_text(json.dumps(history, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description="Omnirefl benchmark history.")
    subcommands = parser.add_subparsers(dest="command", required=True)

    compare_parser = subcommands.add_parser("compare")
    compare_parser.add_argument("--current", required=True)
    compare_parser.add_argument("--history", required=True)
    compare_parser.add_argument("--os", required=True)
    compare_parser.add_argument("--arch", default="x86_64")
    compare_parser.add_argument("--libc", default="musl")
    compare_parser.add_argument("--threshold", type=float, default=1.2)
    compare_parser.add_argument("--min-delta-ms", type=float, default=500)
    compare_parser.add_argument("--count", type=int, default=5)
    compare_parser.set_defaults(func=compare_history)

    update_parser = subcommands.add_parser("update")
    update_parser.add_argument("--current", required=True)
    update_parser.add_argument("--history", required=True)
    update_parser.add_argument("--commit", required=True)
    update_parser.add_argument("--commit-title", default="")
    update_parser.add_argument("--os", required=True)
    update_parser.add_argument("--arch", default="x86_64")
    update_parser.add_argument("--libc", default="musl")
    update_parser.add_argument("--max-runs", type=int, default=200)
    update_parser.set_defaults(func=update_history)

    args = parser.parse_args()
    args.func(args)


if "__main__" == __name__:
    main()
