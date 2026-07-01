#!/usr/bin/env python3

import argparse
import json
import os
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path


def benchmark_key(entry):
    return (entry["name"], entry.get("unit", ""))


def github_escape(value):
    return str(value).replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")


def format_value(value, unit):
    return f"{value:.3f} {unit}".rstrip()


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

        for entry in run.get("benchmarks", []):
            series_by_key[benchmark_key(entry)].append(float(entry["value"]))

    if not series_by_key:
        print(
            "::notice title=Benchmark baseline::"
            "benchmark history has no usable baseline entries")
        return

    regressions = []
    missing_baseline = []
    for entry in current:
        key = benchmark_key(entry)
        previous = series_by_key.get(key, [])[-args.count:]
        if not previous:
            missing_baseline.append(entry["name"])
            continue

        baseline = sum(previous) / len(previous)
        current_value = float(entry["value"])
        if 0 >= baseline or current_value <= baseline * args.threshold:
            continue

        regressions.append({
            "name": entry["name"],
            "unit": entry.get("unit", ""),
            "current": current_value,
            "baseline": baseline,
            "count": len(previous),
            "change": 100 * (current_value / baseline - 1),
        })

    for r in regressions:
        message = (
            f"{r['name']}: {format_value(r['current'], r['unit'])} is "
            f"{r['change']:.1f}% above avg(last {r['count']}) "
            f"{format_value(r['baseline'], r['unit'])}")
        print(f"::warning title=Benchmark regression::{github_escape(message)}")

    if missing_baseline:
        print(
            "::notice title=Benchmark baseline::"
            f"{github_escape('no baseline for: ' + ', '.join(missing_baseline))}")

    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary and regressions:
        with Path(summary).open("a") as f:
            f.write("## Benchmark Regressions\n\n")
            f.write("| metric | current | avg previous | change |\n")
            f.write("| --- | ---: | ---: | ---: |\n")
            for r in regressions:
                f.write(
                    f"| {r['name']} | {format_value(r['current'], r['unit'])} | "
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
        )
    ]
    runs.append({
        "commit": args.commit,
        "os": args.os,
        "run_id": os.environ.get("GITHUB_RUN_ID", ""),
        "created_at": datetime.now(timezone.utc).isoformat(),
        "benchmarks": current,
    })
    history["runs"] = runs[-args.max_runs:]

    history_path.write_text(json.dumps(history, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description="Omnirefl benchmark history.")
    subcommands = parser.add_subparsers(dest="command", required=True)

    compare_parser = subcommands.add_parser("compare")
    compare_parser.add_argument("--current", required=True)
    compare_parser.add_argument("--history", required=True)
    compare_parser.add_argument("--os", required=True)
    compare_parser.add_argument("--threshold", type=float, default=1.2)
    compare_parser.add_argument("--count", type=int, default=5)
    compare_parser.set_defaults(func=compare_history)

    update_parser = subcommands.add_parser("update")
    update_parser.add_argument("--current", required=True)
    update_parser.add_argument("--history", required=True)
    update_parser.add_argument("--commit", required=True)
    update_parser.add_argument("--os", required=True)
    update_parser.add_argument("--max-runs", type=int, default=200)
    update_parser.set_defaults(func=update_history)

    args = parser.parse_args()
    args.func(args)


if "__main__" == __name__:
    main()
