#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

is_container_runtime() {
    if [[ -f "/.dockerenv" ]]; then
        return 0
    fi

    if [[ -f "/run/.containerenv" ]]; then
        return 0
    fi

    if [[ -f "/proc/1/cgroup" ]] && grep -qaE '(docker|containerd|kubepods)' /proc/1/cgroup; then
        return 0
    fi

    return 1
}

if ! command -v docker >/dev/null 2>&1; then
    if is_container_runtime; then
        if [[ "$#" -gt 0 ]]; then
            exec "$@"
        fi

        exec "${SHELL:-/bin/bash}"
    fi

    echo "docker is not available on this machine. Run this script from the host, or invoke commands directly if you're already inside the dev container." >&2
    exit 1
fi

export HOST_UID="${HOST_UID:-$(id -u)}"
export HOST_GID="${HOST_GID:-$(id -g)}"

docker compose build dev
docker compose run --rm dev "$@"
