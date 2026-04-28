# rtb-engine

Linux-first development environment for a low-latency C++ RTB engine.

The current project includes:
- a TCP RTB engine server built in C++
- a simple Python SSP simulator client
- a hybrid benchmark flow with:
  - client-observed RTT from the SSP
  - engine-internal phase timings through response staging

## Development Environment

This project is designed to be developed and run inside Linux because the runtime path depends on Linux facilities such as `epoll` and `SO_REUSEPORT`.

Recommended workflow:
- open the repo in the dev container from Cursor or VS Code
- let the editor reopen the workspace inside the container

That is the cleanest setup because:
- CMake runs in the target Linux environment
- `clangd` reads the container-generated `compile_commands.json`
- the server and simulator run in the same environment you are building for

If you do not want to use the editor dev container, you can still start the same environment from a host terminal with:

```bash
./start_container.sh
```

`start_container.sh` is a convenience wrapper around the Docker dev environment. If you are already inside the container, you do not need it again; just run commands directly.

## Build

Run these inside the container:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

The repo is mounted at `/workspace` inside the container.

## What Runs Where

The engine is the TCP server.

The Python SSP is the TCP client.

For end-to-end testing, run both from the same container environment. If you start multiple one-off containers, `127.0.0.1` inside one container does not point at processes running in a different container.

## Normal End-To-End Run

### 1. Build everything

Inside the container:

```bash
cmake -S . -B build -G Ninja
cmake --build build
./tools/gen_ssp_proto.sh
./tools/install_ssp_deps.sh
```

The SSP dependencies install into a repo-local virtualenv at `.venv-ssp`.

### 2. Start the engine

In shell 1 inside the same container:

```bash
./build/src/rtb_engine --port 8080 --workers 1
```

### 3. Start the SSP client

In shell 2 inside the same container:

```bash
./.venv-ssp/bin/python tools/ssp_sim.py \
  --host 127.0.0.1 \
  --port 8080 \
  --count 10 \
  --seed 12345
```

What happens:
- the SSP sends framed protobuf `BidRequest` messages
- the engine parses, normalizes, retrieves, decides, builds a `BidResponse`, and sends it back
- the SSP prints the returned bid or no-bid response

Stop the engine with `Ctrl-C`.

## Benchmark Run

The benchmark flow gives you two latency views:
- SSP RTT: total client-observed `send -> receive` latency
- engine internal timings: parse, normalize/validate, retrieve, decide, response build, response stage, and total internal latency

### 1. Build everything

Inside the container:

```bash
cmake -S . -B build -G Ninja
cmake --build build
./tools/gen_ssp_proto.sh
./tools/install_ssp_deps.sh
```

### 2. Start the engine in benchmark mode

In shell 1:

```bash
./build/src/rtb_engine \
  --port 8080 \
  --workers 1 \
  --benchmark \
  --benchmark-requests 100100 \
  --benchmark-json /tmp/engine-benchmark.json
```

Why `100100`?
- the SSP benchmark example below uses `warmup=100`
- it then measures `count=100000`
- the engine currently records every handled request, including warmup

So:
- `100 + 100000 = 100100`

When the request limit is reached, the engine exits, prints the internal benchmark summary, and writes `/tmp/engine-benchmark.json`.

### 3. Run the SSP benchmark

In shell 2:

```bash
./.venv-ssp/bin/python tools/ssp_sim.py \
  --host 127.0.0.1 \
  --port 8080 \
  --benchmark \
  --warmup 100 \
  --count 100000 \
  --seed 12345 \
  --json-out /tmp/ssp-benchmark.json
```

This prints an RTT summary and writes `/tmp/ssp-benchmark.json`.

### 4. Inspect results

```bash
cat /tmp/ssp-benchmark.json
cat /tmp/engine-benchmark.json
```

Interpretation:
- SSP JSON tells you what the client experienced over the socket
- engine JSON tells you how much time the engine consumed before the response was staged
- the gap between them is the cost of transport/runtime overhead outside the measured internal engine phases

## Internal Microbenchmark

There is also an in-process internal benchmark that avoids TCP/socket noise:

```bash
./build/bench/rtb_inproc_bench --warmup 1000 --count 10000 --seed 12345
```

This is useful when you want a cleaner engine-only latency signal.

## Useful Commands

Show engine CLI help:

```bash
./build/src/rtb_engine --help
```

Run the engine on a different port:

```bash
./build/src/rtb_engine --port 9090 --workers 2 --campaign-data-path data/sample_campaigns.csv
```

## Important Files

- [`include/rtb/config.h`](/Users/akulsharma/Documents/projects/rtb-engine/include/rtb/config.h)
  - shared runtime and benchmark configuration constants
  - includes things like log enablement, bid range, and timing-related defaults

- [`src/main.cc`](/Users/akulsharma/Documents/projects/rtb-engine/src/main.cc)
  - binary entrypoint
  - parses CLI args, initializes protobuf, and starts the server bootstrap

- [`src/server.cc`](/Users/akulsharma/Documents/projects/rtb-engine/src/server.cc)
  - server bootstrap and process-level orchestration
  - handles worker startup, signal/shutdown handling, and benchmark-mode setup/output

- [`src/worker_runtime.cc`](/Users/akulsharma/Documents/projects/rtb-engine/src/worker_runtime.cc)
  - core runtime loop
  - owns the `epoll` flow, connection lifecycle, request parsing, response staging, and internal request timing capture

- [`src/handle_request.cc`](/Users/akulsharma/Documents/projects/rtb-engine/src/handle_request.cc)
  - synchronous request pipeline after protobuf parse
  - builds `RequestContext`, validates it, retrieves candidates, decides, and builds the `BidResponse`

- [`tools/ssp_sim.py`](/Users/akulsharma/Documents/projects/rtb-engine/tools/ssp_sim.py)
  - Python SSP client and RTT benchmark driver
  - useful for both normal end-to-end testing and client-observed latency benchmarking

## Notes

- The container runs as a non-root `builder` user so files created in the mounted workspace are owned by your host user.
- If you prefer a long-lived container from the host, `docker compose up -d dev` and `docker compose exec dev bash` also work.
- If your current container is stale and Python tooling is missing, exit it and start a fresh one from the host with `./start_container.sh`.
- For serious latency measurements, consider setting [`kEnableLogs`](./include/rtb/config.h) to `false` before benchmarking so logging overhead does not pollute the numbers.
