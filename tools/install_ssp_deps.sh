#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
VENV_DIR="${REPO_ROOT}/.venv-ssp"
REQUIREMENTS_FILE="${REPO_ROOT}/tools/requirements-ssp.txt"

if python3 -c 'import google.protobuf' >/dev/null 2>&1; then
  echo "google.protobuf is already available in this Python environment."
  echo "You can run:"
  echo "  python3 ${REPO_ROOT}/tools/ssp_sim.py --host 127.0.0.1 --port 8080 --count 10 --seed 12345"
  exit 0
fi

if [[ ! -d "${VENV_DIR}" ]]; then
  echo "Creating virtualenv at ${VENV_DIR}"
  if ! python3 -m venv "${VENV_DIR}"; then
    echo "python3 -m venv failed. Rebuild/re-enter the dev container so python3-venv is available, then rerun this script." >&2
    exit 1
  fi
fi

if ! "${VENV_DIR}/bin/python" -m pip --version >/dev/null 2>&1; then
  echo "Bootstrapping pip inside ${VENV_DIR}"
  if ! "${VENV_DIR}/bin/python" -m ensurepip --upgrade >/dev/null 2>&1; then
    echo "ensurepip failed inside ${VENV_DIR}. Rebuild/re-enter the dev container so python3-venv support is complete, then rerun this script." >&2
    exit 1
  fi
fi

"${VENV_DIR}/bin/python" -m pip install -r "${REQUIREMENTS_FILE}"

echo
echo "SSP dependencies installed in ${VENV_DIR}."
echo "Run the simulator with:"
echo "  ${VENV_DIR}/bin/python ${REPO_ROOT}/tools/ssp_sim.py --host 127.0.0.1 --port 8080 --count 10 --seed 12345"
