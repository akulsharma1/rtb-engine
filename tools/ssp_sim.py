#!/usr/bin/env python3
from __future__ import annotations

import argparse
import random
import socket
import struct
import sys
import time
from pathlib import Path


TOOLS_DIR = Path(__file__).resolve().parent
GENERATED_DIR = TOOLS_DIR / "generated"
if str(GENERATED_DIR) not in sys.path:
    sys.path.insert(0, str(GENERATED_DIR))

try:
    import rtb_pb2
except ModuleNotFoundError as exc:
    raise SystemExit(
        "Missing generated protobuf module. Run `tools/gen_ssp_proto.sh` first."
    ) from exc


COUNTRIES = ("US", "US", "US", "CA")
DEVICE_TYPES = ("desktop", "desktop", "mobile", "tablet")
AD_SLOTS = ("homepage-top", "homepage-top", "sports-sidebar")


def build_request(request_index: int, rng: random.Random) -> "rtb_pb2.BidRequest":
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


def recv_framed_response(sock: socket.socket) -> "rtb_pb2.BidResponse":
    header = recv_exact(sock, 4)
    (payload_length,) = struct.unpack("!I", header)
    if payload_length == 0:
        raise RuntimeError("received invalid zero-length response frame")

    payload = recv_exact(sock, payload_length)
    response = rtb_pb2.BidResponse()
    response.ParseFromString(payload)
    return response


def print_response(response: "rtb_pb2.BidResponse") -> None:
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
    parser.add_argument("--interval-ms", type=int, default=0, help="delay between requests")
    parser.add_argument("--seed", type=int, default=12345, help="random seed")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    rng = random.Random(args.seed)

    try:
        with socket.create_connection((args.host, args.port)) as sock:
            for request_index in range(1, args.count + 1):
                request = build_request(request_index, rng)
                payload = request.SerializeToString()
                sock.sendall(frame_message(payload))
                response = recv_framed_response(sock)
                print_response(response)

                if args.interval_ms > 0 and request_index != args.count:
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

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
