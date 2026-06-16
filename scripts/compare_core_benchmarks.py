#!/usr/bin/env python3
"""
Compare Vix Core benchmark JSON reports.

Expected input:
  - a single JSON report file, or
  - a directory containing many JSON report files produced by run_core_benchmarks.sh

Default metric:
  median_ops_per_sec

Higher is better for ops/sec metrics.
Lower is better for *_ms metrics.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


DEFAULT_WARN_PERCENT = 5.0
DEFAULT_FAIL_PERCENT = 10.0


@dataclass(frozen=True)
class BenchPoint:
    suite: str
    name: str
    metric: str
    value: float
    report_path: Path

    @property
    def key(self) -> str:
        return f"{self.suite}/{self.name}"


@dataclass(frozen=True)
class Comparison:
    key: str
    baseline: float | None
    current: float | None
    change_percent: float | None
    status: str
    metric: str


def load_json_file(path: Path) -> dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8") as f:
            data = json.load(f)
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"invalid JSON file: {path}: {exc}") from exc
    except OSError as exc:
        raise RuntimeError(f"failed to read file: {path}: {exc}") from exc

    if not isinstance(data, dict):
        raise RuntimeError(f"benchmark report must be a JSON object: {path}")

    return data


def discover_json_files(path: Path) -> list[Path]:
    if path.is_file():
        return [path]

    if path.is_dir():
        files = sorted(p for p in path.rglob("*.json") if p.is_file())

        if not files:
            raise RuntimeError(f"no JSON files found in directory: {path}")

        return files

    raise RuntimeError(f"path does not exist: {path}")


def read_points(path: Path, metric: str) -> dict[str, BenchPoint]:
    points: dict[str, BenchPoint] = {}

    for file_path in discover_json_files(path):
        report = load_json_file(file_path)

        suite = str(report.get("suite", "unknown"))
        results = report.get("results")

        if not isinstance(results, list):
            raise RuntimeError(f"missing or invalid results array: {file_path}")

        for item in results:
            if not isinstance(item, dict):
                raise RuntimeError(f"invalid benchmark result item in: {file_path}")

            name = item.get("name")

            if not isinstance(name, str) or not name:
                raise RuntimeError(f"benchmark result is missing a valid name in: {file_path}")

            raw_value = item.get(metric)

            if raw_value is None:
                raise RuntimeError(
                    f"benchmark result {suite}/{name} is missing metric {metric}: {file_path}"
                )

            try:
                value = float(raw_value)
            except (TypeError, ValueError) as exc:
                raise RuntimeError(
                    f"benchmark result {suite}/{name} has non-numeric metric {metric}: {raw_value}"
                ) from exc

            if not math.isfinite(value):
                raise RuntimeError(
                    f"benchmark result {suite}/{name} has invalid metric {metric}: {raw_value}"
                )

            point = BenchPoint(
                suite=suite,
                name=name,
                metric=metric,
                value=value,
                report_path=file_path,
            )

            if point.key in points:
                raise RuntimeError(
                    f"duplicate benchmark key {point.key}; first seen in "
                    f"{points[point.key].report_path}, again in {file_path}"
                )

            points[point.key] = point

    return points


def higher_is_better(metric: str) -> bool:
    metric = metric.lower()

    if "ops_per_sec" in metric:
        return True

    if metric.endswith("_ms") or metric == "elapsed_ms":
        return False

    return True


def percent_change(
    baseline: float,
    current: float,
    *,
    higher_better: bool,
) -> float:
    if baseline == 0.0:
        if current == 0.0:
            return 0.0

        return 100.0 if higher_better else -100.0

    raw = ((current - baseline) / baseline) * 100.0

    if higher_better:
        return raw

    return -raw


def classify_change(
    change_percent: float,
    *,
    warn_percent: float,
    fail_percent: float,
) -> str:
    if change_percent <= -fail_percent:
        return "FAIL"

    if change_percent <= -warn_percent:
        return "WARN"

    return "OK"


def compare_points(
    baseline: dict[str, BenchPoint],
    current: dict[str, BenchPoint],
    *,
    metric: str,
    warn_percent: float,
    fail_percent: float,
) -> list[Comparison]:
    keys = sorted(set(baseline.keys()) | set(current.keys()))
    better_high = higher_is_better(metric)

    comparisons: list[Comparison] = []

    for key in keys:
        base = baseline.get(key)
        cur = current.get(key)

        if base is None:
            comparisons.append(
                Comparison(
                    key=key,
                    baseline=None,
                    current=cur.value if cur else None,
                    change_percent=None,
                    status="NEW",
                    metric=metric,
                )
            )
            continue

        if cur is None:
            comparisons.append(
                Comparison(
                    key=key,
                    baseline=base.value,
                    current=None,
                    change_percent=None,
                    status="MISSING",
                    metric=metric,
                )
            )
            continue

        change = percent_change(
            base.value,
            cur.value,
            higher_better=better_high,
        )

        comparisons.append(
            Comparison(
                key=key,
                baseline=base.value,
                current=cur.value,
                change_percent=change,
                status=classify_change(
                    change,
                    warn_percent=warn_percent,
                    fail_percent=fail_percent,
                ),
                metric=metric,
            )
        )

    return comparisons


def format_number(value: float | None) -> str:
    if value is None:
        return "-"

    if abs(value) >= 1000.0:
        return f"{value:,.2f}"

    return f"{value:.4f}"


def format_change(value: float | None) -> str:
    if value is None:
        return "-"

    sign = "+" if value >= 0.0 else ""

    return f"{sign}{value:.2f}%"


def print_table(comparisons: list[Comparison]) -> None:
    headers = [
        "status",
        "change",
        "baseline",
        "current",
        "benchmark",
    ]

    rows = [
        [
            item.status,
            format_change(item.change_percent),
            format_number(item.baseline),
            format_number(item.current),
            item.key,
        ]
        for item in comparisons
    ]

    widths = [
        max(len(headers[i]), *(len(row[i]) for row in rows))
        for i in range(len(headers))
    ]

    print(
        "  ".join(
            headers[i].ljust(widths[i])
            for i in range(len(headers))
        )
    )

    print(
        "  ".join(
            "-" * widths[i]
            for i in range(len(headers))
        )
    )

    for row in rows:
        print(
            "  ".join(
                row[i].ljust(widths[i])
                for i in range(len(row))
            )
        )


def write_json_report(
    path: Path,
    comparisons: list[Comparison],
    *,
    metric: str,
    warn_percent: float,
    fail_percent: float,
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)

    data = {
        "metric": metric,
        "warn_percent": warn_percent,
        "fail_percent": fail_percent,
        "summary": {
            "ok": sum(1 for item in comparisons if item.status == "OK"),
            "warn": sum(1 for item in comparisons if item.status == "WARN"),
            "fail": sum(1 for item in comparisons if item.status == "FAIL"),
            "new": sum(1 for item in comparisons if item.status == "NEW"),
            "missing": sum(1 for item in comparisons if item.status == "MISSING"),
            "total": len(comparisons),
        },
        "results": [
            {
                "benchmark": item.key,
                "status": item.status,
                "baseline": item.baseline,
                "current": item.current,
                "change_percent": item.change_percent,
            }
            for item in comparisons
        ],
    }

    with path.open("w", encoding="utf-8") as f:
      json.dump(data, f, indent=2)
      f.write("\n")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Compare Vix Core benchmark reports against a baseline."
    )

    parser.add_argument(
        "baseline",
        type=Path,
        help="Baseline JSON file or directory.",
    )

    parser.add_argument(
        "current",
        type=Path,
        help="Current JSON file or directory.",
    )

    parser.add_argument(
        "--metric",
        default="median_ops_per_sec",
        help="Metric to compare. Default: median_ops_per_sec",
    )

    parser.add_argument(
        "--warn",
        type=float,
        default=DEFAULT_WARN_PERCENT,
        help=f"Warning threshold in percent. Default: {DEFAULT_WARN_PERCENT}",
    )

    parser.add_argument(
        "--fail",
        type=float,
        default=DEFAULT_FAIL_PERCENT,
        help=f"Failure threshold in percent. Default: {DEFAULT_FAIL_PERCENT}",
    )

    parser.add_argument(
        "--json-out",
        type=Path,
        default=None,
        help="Optional path to write comparison JSON.",
    )

    parser.add_argument(
        "--allow-new",
        action="store_true",
        help="Do not fail when current results contain benchmarks missing from baseline.",
    )

    parser.add_argument(
        "--allow-missing",
        action="store_true",
        help="Do not fail when baseline benchmarks are missing from current results.",
    )

    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if args.warn < 0.0:
        print("[core/benchmarks] --warn must be >= 0", file=sys.stderr)
        return 2

    if args.fail < 0.0:
        print("[core/benchmarks] --fail must be >= 0", file=sys.stderr)
        return 2

    if args.warn > args.fail:
        print("[core/benchmarks] --warn must be <= --fail", file=sys.stderr)
        return 2

    try:
        baseline = read_points(args.baseline, args.metric)
        current = read_points(args.current, args.metric)

        comparisons = compare_points(
            baseline,
            current,
            metric=args.metric,
            warn_percent=args.warn,
            fail_percent=args.fail,
        )
    except RuntimeError as exc:
        print(f"[core/benchmarks] {exc}", file=sys.stderr)
        return 2

    print_table(comparisons)

    if args.json_out is not None:
        write_json_report(
            args.json_out,
            comparisons,
            metric=args.metric,
            warn_percent=args.warn,
            fail_percent=args.fail,
        )

    fail_count = sum(1 for item in comparisons if item.status == "FAIL")
    warn_count = sum(1 for item in comparisons if item.status == "WARN")
    new_count = sum(1 for item in comparisons if item.status == "NEW")
    missing_count = sum(1 for item in comparisons if item.status == "MISSING")

    print()
    print(
        "[core/benchmarks] summary: "
        f"ok={sum(1 for item in comparisons if item.status == 'OK')} "
        f"warn={warn_count} "
        f"fail={fail_count} "
        f"new={new_count} "
        f"missing={missing_count} "
        f"total={len(comparisons)}"
    )

    should_fail = False

    if fail_count > 0:
        should_fail = True

    if new_count > 0 and not args.allow_new:
        should_fail = True

    if missing_count > 0 and not args.allow_missing:
        should_fail = True

    return 1 if should_fail else 0


if __name__ == "__main__":
    raise SystemExit(main())
