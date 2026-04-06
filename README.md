# rtb-engine

Linux-first development environment for a low-latency C++ RTB engine.

## Why Docker first

The runtime path we want to build depends on Linux facilities such as `epoll` and `SO_REUSEPORT`.
That makes a Linux container the simplest way to get a repeatable development environment from macOS.

The container includes:

- `clang`
- `cmake`
- `ninja`
- `gdb`
- `protobuf` compiler and headers
- `git`

## Quick start

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
./build/src/rtb_engine
```

## Notes

- The container runs as a non-root `builder` user so files created in the mounted workspace are owned by your host user.
- Pass `HOST_UID` and `HOST_GID` when building or running so the container user matches your host account.
- `docker compose up -d dev` also works if you want to keep the container running in the background and `docker compose exec dev bash` into it later.
- This setup is intentionally Linux-only because the low-latency networking path will target Linux from the start.
