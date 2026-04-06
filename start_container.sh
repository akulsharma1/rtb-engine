#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

export HOST_UID="${HOST_UID:-$(id -u)}"
export HOST_GID="${HOST_GID:-$(id -g)}"

docker compose build dev
docker compose run --rm dev "$@"
