#!/usr/bin/env python3
from __future__ import annotations

import json
import tempfile
from pathlib import Path
import sys

REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools import ssp_sim


def main() -> int:
    summary = ssp_sim.summarize_latencies([100, 200, 300, 400])
    if summary["count"] != 4:
        return 1
    if summary["min_ns"] != 100 or summary["max_ns"] != 400:
        return 1
    if summary["p50_ns"] not in (200, 300):
        return 1

    with tempfile.TemporaryDirectory() as temp_dir:
        output_path = Path(temp_dir) / "ssp_benchmark.json"
        ssp_sim.write_benchmark_json(
            output_path,
            {
                "timestamp_utc": "2026-04-27T00:00:00Z",
                "host": "127.0.0.1",
                "port": 8080,
                "warmup": 2,
                "count": 4,
                "seed": 12345,
            },
            summary,
        )
        payload = json.loads(output_path.read_text(encoding="utf-8"))
        if payload["metadata"]["port"] != 8080 or payload["summary"]["count"] != 4:
            return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
