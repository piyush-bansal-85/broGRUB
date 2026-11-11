#!/usr/bin/env bash

set -Eeuo pipefail

# Architecture check
if [[ "$(uname -m)" != "x86_64" ]]; then
    echo "[BroGRUB] ERROR: BroGRUB currently supports x86_64 only." >&2
    exit 1
fi

# UEFI check
if [[ ! -d /sys/firmware/efi ]]; then
    echo "[BroGRUB] ERROR: BroGRUB requires UEFI boot mode." >&2
    exit 1
fi

# Python check
if ! command -v python3 >/dev/null 2>&1; then
    echo "[BroGRUB] ERROR: Python 3 is required." >&2
    exit 1
fi

# Root privileges
if [[ $EUID -ne 0 ]]; then
    echo "[BroGRUB] Requesting administrator privileges..."
    exec sudo -- bash "$0" "$@"
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_INSTALLER="$SCRIPT_DIR/scripts/install.py"

echo -e "\033[1;95m       BroGRUB Installer\033[0m"

# Check installer
if [[ ! -f "$PYTHON_INSTALLER" ]]; then
    echo "[BroGRUB] ERROR: Missing scripts/install.py" >&2
    exit 1
fi


# Required UEFI tools
for cmd in efibootmgr findmnt lsblk; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "[BroGRUB] ERROR: Required command '$cmd' not found." >&2
        exit 1
    fi
done

# ESP mount check
ESP_PATH="/boot/efi"

if ! mountpoint -q "$ESP_PATH"; then
    echo "[BroGRUB] ERROR: ESP is not mounted at $ESP_PATH." >&2
    exit 1
fi

# Run Python installer
echo -e "\n\033[1;93m[BroGRUB] Installing files...\033[0m"

python3 "$PYTHON_INSTALLER" "$@"

# Detect ESP device
ESP_DEVICE="$(findmnt -nro SOURCE --target "$ESP_PATH")"
ESP_DEVICE="$(readlink -f "$ESP_DEVICE")"

DISK_NAME="$(lsblk -no PKNAME "$ESP_DEVICE" | head -n 1)"
PART_NUMBER="$(lsblk -no PARTN "$ESP_DEVICE" | head -n 1)"

if [[ -z "$DISK_NAME" || -z "$PART_NUMBER" ]]; then
    echo "[BroGRUB] ERROR: Could not identify the ESP disk and partition." >&2
    exit 1
fi

DISK="/dev/$DISK_NAME"

# Check for an existing BroGRUB entry
BOOT_ENTRY="$(efibootmgr | grep -i 'BroGRUB' | head -n 1 || true)"

if [[ -n "$BOOT_ENTRY" ]]; then
    BOOT_ID="$(echo "$BOOT_ENTRY" | grep -oE '[0-9A-Fa-f]{4}' | head -n 1)"
    echo "[BroGRUB] Existing UEFI entry found: $BOOT_ID"
else
    # Create UEFI entry
    echo -e "\n\033[1;93m[BroGRUB] Creating UEFI boot entry...\033[0m"

    efibootmgr --create \
        --disk "$DISK" \
        --part "$PART_NUMBER" \
        --label "BroGRUB" \
        --loader '\EFI\BroGRUB\shimx64.efi'

    BOOT_ENTRY="$(efibootmgr | grep -i 'BroGRUB' | head -n 1)"
    BOOT_ID="$(echo "$BOOT_ENTRY" | grep -oE '[0-9A-Fa-f]{4}' | head -n 1)"

    if [[ -z "$BOOT_ID" ]]; then
        echo "[BroGRUB] ERROR: Failed to obtain the new boot entry." >&2
        exit 1
    fi
fi

# Move BroGRUB to the front of BootOrder
CURRENT_ORDER="$(efibootmgr | grep '^BootOrder:' | cut -d ':' -f 2 | tr -d ' ')"

if [[ -z "$CURRENT_ORDER" ]]; then
    echo "[BroGRUB] ERROR: Could not read BootOrder." >&2
    exit 1
fi

NEW_ORDER="$BOOT_ID"

IFS=',' read -ra ENTRIES <<< "$CURRENT_ORDER"

for entry in "${ENTRIES[@]}"; do
    if [[ "${entry^^}" != "${BOOT_ID^^}" ]]; then
        NEW_ORDER+=",${entry}"
    fi
done

efibootmgr --bootorder "$NEW_ORDER"

echo -e "\n\033[1;92m[BroGRUB] Installation completed!\033[0m"
echo "[BroGRUB] Boot entry: $BOOT_ID"
echo "[BroGRUB] BootOrder: $NEW_ORDER"
echo "[BroGRUB] BroGRUB will be the first boot option."
echo "[BroGRUB] Restart your computer to launch BroGRUB."
