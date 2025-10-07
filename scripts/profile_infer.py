#!/usr/bin/env python3
"""Benchmark Kolibri inference across beam/depth grid."""

from __future__ import annotations

import argparse
import subprocess
import time
from pathlib import Path
from statistics import mean

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "apps" / "kolibri_infer"


def run_once(q: int, beam: int, depth: int) -> float:
    start = time.perf_counter()
    subprocess.run([str(APP), "--q", str(q), "--beam", str(beam), "--depth", str(depth)], check=True, stdout=subprocess.DEVNULL)
    return (time.perf_counter() - start) * 1000.0


def main() -> int:
    parser = argparse.ArgumentParser(description="Kolibri inference profiler")
    parser.add_argument("--q", type=int, default=42, help="input q")
    parser.add_argument("--beam", nargs="*", type=int, default=[8, 12, 16, 32], help="beam values")
    parser.add_argument("--depth", nargs="*", type=int, default=[4, 8, 16], help="depth values")
    parser.add_argument("--runs", type=int, default=5, help="runs per configuration")
    args = parser.parse_args()

    if not APP.exists():
        raise SystemExit("kolibri_infer binary not built. Run cmake --build build first.")

    for beam in args.beam:
        for depth in args.depth:
            timings = [run_once(args.q, beam, depth) for _ in range(args.runs)]
            print(f"beam={beam:3d} depth={depth:3d} mean={mean(timings):7.2f}ms max={max(timings):7.2f}ms")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
