#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
OUTPUT_DIR="${REPO_ROOT}/tools/generated"

mkdir -p "${OUTPUT_DIR}"
protoc \
  --proto_path="${REPO_ROOT}/proto" \
  --python_out="${OUTPUT_DIR}" \
  "${REPO_ROOT}/proto/rtb.proto"
