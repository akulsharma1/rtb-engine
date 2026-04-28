# rtb-engine

Linux-first development environment for a low-latency C++ RTB engine.

## Why Docker first

The runtime path we want to build depends on Linux facilities such as `epoll` and `SO_REUSEPORT`.
That makes a Linux container the simplest way to get a repeatable development environment from macOS.

The container includes:

- `clang`
- `clangd`
- `cmake`
- `ninja`
- `gdb`
- `protobuf` compiler and headers
- `git`

## Quick start

Recommended: open the repo in the devcontainer from Cursor or VS Code. That gives you the easiest setup and makes sure CMake and `clangd` run inside Linux instead of on macOS.

1. Open this folder in Cursor or VS Code.
2. Choose "Reopen in Container" when prompted, or run the Dev Containers command manually.
3. If you changed the container image and want a fresh rebuild, run this on the host first:

```bash
HOST_UID=$(id -u) HOST_GID=$(id -g) docker compose build
```

Then reopen the folder in the container. The devcontainer `postCreateCommand` configures CMake in `/workspace/build`, which is where `clangd` reads `compile_commands.json` for autocomplete and inline diagnostics.

If you prefer to work without the editor devcontainer, you can run the container manually from a host terminal instead:

Build the image:

```bash
HOST_UID=$(id -u) HOST_GID=$(id -g) docker compose build
```

If Docker reports that it cannot connect to `docker.sock`, start Docker Desktop first and rerun the build.

Start an interactive shell in the Linux container:

```bash
HOST_UID=$(id -u) HOST_GID=$(id -g) docker compose run --rm dev
```

Or use the helper script from the repo root:

```bash
./start_container.sh
```

Inside the container, the repo is mounted at `/workspace`:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/src/rtb_engine --port 8080 --workers 1
```

## SSP Simulator

A basic Python SSP simulator is available under `tools/ssp_sim.py`. It connects to the RTB engine over TCP, sends framed protobuf `BidRequest` messages, and prints the returned `BidResponse` details.

Install the Python protobuf runtime:

```bash
./tools/install_ssp_deps.sh
```

Generate the Python protobuf module:

```bash
./tools/gen_ssp_proto.sh
```

Run the engine in one shell, then run the simulator in another:

```bash
./build/src/rtb_engine --port 8080 --workers 1
python3 tools/ssp_sim.py --port 8080 --count 10 --seed 12345
```

For RTT benchmarking from the Python SSP:

```bash
./build/src/rtb_engine --port 8080 --workers 1 --benchmark --benchmark-json /tmp/engine-benchmark.json
./.venv-ssp/bin/python tools/ssp_sim.py --host 127.0.0.1 --port 8080 --benchmark --warmup 100 --count 1000 --seed 12345 --json-out /tmp/ssp-benchmark.json
```

That gives you:
- client-observed RTT percentiles from the SSP
- engine-internal phase timings through response staging from the server

An internal in-process microbenchmark is also available:

```bash
./build/bench/rtb_inproc_bench --warmup 1000 --count 10000 --seed 12345
```

The SSP dependencies install into a repo-local virtualenv at `.venv-ssp`, so they do not touch the container's system Python.

If your current container was created before Python tooling was added and `python3 -m venv` is missing, rebuild/re-enter it from the host:

```bash
exit
./start_container.sh
./tools/install_ssp_deps.sh
```

Use `Ctrl-C` to stop the server cleanly. The binary also supports:

```bash
./build/src/rtb_engine --help
./build/src/rtb_engine --port 9090 --workers 2 --campaign-data-path data/sample_campaigns.csv
```

## Notes

- The container runs as a non-root `builder` user so files created in the mounted workspace are owned by your host user.
- Pass `HOST_UID` and `HOST_GID` when building or running so the container user matches your host account.
- `docker compose up -d dev` also works if you want to keep the container running in the background and `docker compose exec dev bash` into it later.
- This setup is intentionally Linux-only because the low-latency networking path will target Linux from the start.
- The devcontainer uses a compose override to keep the `dev` service alive for editor attach without changing the normal CLI workflow.
