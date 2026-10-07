#!/usr/bin/env bash
# Prepare (or reuse) a Python virtualenv for the telemetry reader, activate it,
# and install pyserial only if it is missing.
#
#   ./setup.sh                 # set up, then run read_telemetry.py
#   ./setup.sh -p /dev/ttyACM0 # extra args are forwarded to read_telemetry.py
#   source setup.sh            # set up and leave the venv activated in this shell
#
# Creating a venv on first run and reusing it afterwards keeps the reader's
# dependency out of the system Python.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="${SCRIPT_DIR}/.venv"

# Create the virtualenv on first use.
if [[ ! -x "${VENV_DIR}/bin/python" ]]; then
    echo "[setup] creating virtualenv in ${VENV_DIR}"
    python3 -m venv "${VENV_DIR}"
fi

# Activate it. venv's activate script is not nounset-clean on every platform.
set +u
# shellcheck disable=SC1091
source "${VENV_DIR}/bin/activate"
set -u

# Install pyserial only if the venv does not already provide it.
if ! python -c "import serial" >/dev/null 2>&1; then
    echo "[setup] installing pyserial"
    python -m pip install --quiet --upgrade pip
    python -m pip install --quiet pyserial
fi

# Sourced: leave the caller with the venv active. Executed: run the reader.
if [[ "${BASH_SOURCE[0]}" != "${0}" ]]; then
    echo "[setup] virtualenv activated; run: python read_telemetry.py"
    return 0
fi

exec python "${SCRIPT_DIR}/read_telemetry.py" "$@"
