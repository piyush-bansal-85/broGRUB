#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_UNINSTALLER="$SCRIPT_DIR/scripts/uninstall.py"

if [[ ! -f "$PYTHON_UNINSTALLER" ]]; then
    echo "[BroGRUB] ERROR: Missing scripts/uninstall.py" >&2
    exit 1
fi

if [[ $EUID -ne 0 ]]; then
    exec sudo -- python3 "$PYTHON_UNINSTALLER" "$@"
fi

command -v python3 >/dev/null 2>&1 || {
    echo "[BroGRUB] ERROR: Python 3 is required." >&2
    exit 1
}
exec python3 "$PYTHON_UNINSTALLER" "$@"
