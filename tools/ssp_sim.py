#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import random
import socket
import struct
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


TOOLS_DIR = Path(__file__).resolve().parent
GENERATED_DIR = TOOLS_DIR / "generated"
if str(GENERATED_DIR) not in sys.path:
    sys.path.insert(0, str(GENERATED_DIR))

COUNTRIES = ("US", "US", "US", "CA")
DEVICE_TYPES = ("desktop", "desktop", "mobile", "tablet")
AD_SLOTS = ("homepage-top", "homepage-top", "sports-sidebar")


def load_proto_module():
    try:
        import rtb_pb2
    except ModuleNotFoundError as exc:
        raise SystemExit(
            "Missing generated protobuf module. Run `tools/gen_ssp_proto.sh` first."
        ) from exc

    return rtb_pb2


def build_request(request_index: int, rng: random.Random, rtb_pb2) -> "rtb_pb2.BidRequest":
    request = rtb_pb2.BidRequest()
    request.request_id = f"ssp-req-{request_index:06d}"
    request.tmax_ms = rng.randint(50, 200)
    request.country = rng.choice(COUNTRIES)
    request.device_type = rng.choice(DEVICE_TYPES)
    request.ad_slot = rng.choice(AD_SLOTS)
    request.auction_type = rtb_pb2.AUCTION_TYPE_FIRST_PRICE
    return request


def frame_message(payload: bytes) -> bytes:
    return struct.pack("!I", len(payload)) + payload


def recv_exact(sock: socket.socket, byte_count: int) -> bytes:
    chunks: list[bytes] = []
    remaining = byte_count
    while remaining > 0:
        chunk = sock.recv(remaining)
        if not chunk:
            raise RuntimeError(f"connection closed while reading {byte_count} bytes")
        chunks.append(chunk)
        remaining -= len(chunk)
    return b"".join(chunks)


def recv_framed_response(sock: socket.socket, rtb_pb2) -> "rtb_pb2.BidResponse":
    header = recv_exact(sock, 4)
    (payload_length,) = struct.unpack("!I", header)
    if payload_length == 0:
        raise RuntimeError("received invalid zero-length response frame")

    payload = recv_exact(sock, payload_length)
    response = rtb_pb2.BidResponse()
    response.ParseFromString(payload)
    return response


def print_response(response, rtb_pb2) -> None:
    status_name = rtb_pb2.BidStatus.Name(response.status)
    if response.status == rtb_pb2.BID_STATUS_BID:
        print(
            f"request_id={response.request_id} status={status_name} "
            f"campaign_id={response.campaign_id} creative_id={response.creative_id} "
            f"price={response.price:.6f}"
        )
        return

    print(f"request_id={response.request_id} status={status_name}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Basic SSP simulator for the RTB engine.")
    parser.add_argument("--host", default="127.0.0.1", help="RTB engine host")
    parser.add_argument("--port", type=int, required=True, help="RTB engine port")
    parser.add_argument("--count", type=int, default=10, help="number of requests to send")
    parser.add_argument("--warmup", type=int, default=0, help="number of warmup requests before benchmarking")
    parser.add_argument("--interval-ms", type=int, default=0, help="delay between requests")
    parser.add_argument("--seed", type=int, default=12345, help="random seed")
    parser.add_argument("--benchmark", action="store_true", help="measure request RTTs and print benchmark summary")
    parser.add_argument("--json-out", help="optional JSON output path for benchmark summary")
    parser.add_argument("--quiet", action="store_true", help="suppress per-request response printing")
    return parser.parse_args()


def summarize_latencies(latencies_ns: list[int]) -> dict[str, float | int]:
    if not latencies_ns:
        return {
            "count": 0,
            "min_ns": 0,
            "max_ns": 0,
            "avg_ns": 0.0,
            "p50_ns": 0,
            "p95_ns": 0,
            "p99_ns": 0,
        }

    ordered = sorted(latencies_ns)

    def percentile(percent: float) -> int:
        index = int(percent * (len(ordered) - 1))
        return ordered[index]

    return {
        "count": len(ordered),
        "min_ns": ordered[0],
        "max_ns": ordered[-1],
        "avg_ns": sum(ordered) / len(ordered),
        "p50_ns": percentile(0.50),
        "p95_ns": percentile(0.95),
        "p99_ns": percentile(0.99),
    }


def print_benchmark_summary(summary: dict[str, float | int]) -> None:
    print("SSP RTT benchmark summary")
    print(
        "count={count} avg={avg_ns:.2f}ns p50={p50_ns}ns p95={p95_ns}ns "
        "p99={p99_ns}ns max={max_ns}ns".format(**summary)
    )


def write_benchmark_json(path: Path, metadata: dict[str, object], summary: dict[str, float | int]) -> None:
    payload = {
        "metadata": metadata,
        "summary": summary,
    }
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    args = parse_args()
    rng = random.Random(args.seed)
    rtb_pb2 = load_proto_module()
    benchmark_mode = args.benchmark or args.json_out is not None
    quiet_mode = args.quiet or benchmark_mode
    rtt_latencies_ns: list[int] = []

    try:
        with socket.create_connection((args.host, args.port)) as sock:
            request_index = 1

            for _ in range(args.warmup):
                request = build_request(request_index, rng, rtb_pb2)
                request_index += 1
                payload = request.SerializeToString()
                sock.sendall(frame_message(payload))
                recv_framed_response(sock, rtb_pb2)

            for _ in range(args.count):
                request = build_request(request_index, rng, rtb_pb2)
                request_index += 1
                payload = request.SerializeToString()
                start_ns = time.perf_counter_ns()
                sock.sendall(frame_message(payload))
                response = recv_framed_response(sock, rtb_pb2)
                end_ns = time.perf_counter_ns()

                if benchmark_mode:
                    rtt_latencies_ns.append(end_ns - start_ns)

                if not quiet_mode:
                    print_response(response, rtb_pb2)

                if args.interval_ms > 0 and request_index != args.warmup + args.count + 1:
                    time.sleep(args.interval_ms / 1000.0)
    except ConnectionRefusedError:
        print(
            f"failed to connect to RTB engine at {args.host}:{args.port}: connection refused",
            file=sys.stderr,
        )
        return 1
    except OSError as exc:
        print(f"socket error: {exc}", file=sys.stderr)
        return 1
    except RuntimeError as exc:
        print(f"protocol error: {exc}", file=sys.stderr)
        return 1

    if benchmark_mode:
        summary = summarize_latencies(rtt_latencies_ns)
        print_benchmark_summary(summary)
        if args.json_out is not None:
            write_benchmark_json(
                Path(args.json_out),
                {
                    "timestamp_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
                    "host": args.host,
                    "port": args.port,
                    "warmup": args.warmup,
                    "count": args.count,
                    "seed": args.seed,
                },
                summary,
            )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
