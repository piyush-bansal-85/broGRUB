
import sys
import os
import re
import shutil
import subprocess
from pathlib import Path


# Architecture check
if __import__("platform").machine().lower() not in ("x86_64", "amd64"):
    sys.exit("BroGRUB supports x86_64 only.")


# UEFI check
if not Path("/sys/firmware/efi").exists():
    sys.exit("BroGRUB requires UEFI boot mode.")


# Root check
if os.geteuid() != 0:
    sys.exit("Please run the uninstaller as root.")


# System paths
GRUB_PATH = Path("/boot/grub")
EFI_PATH = Path("/boot/efi")
EFI_BROGRUB = EFI_PATH / "EFI" / "BroGRUB"

GRUB_MODULE_PATH = GRUB_PATH / "x86_64-efi"
THEMES_PATH = GRUB_PATH / "themes" / "customWorld"

# BroGRUB files
FONT_FILE = GRUB_PATH / "BroGRUB-fonts.tar"
CONFIG_FILE = GRUB_PATH / "brogrub.cfg"

MODULES = (
    "bg_change.mod",
    "bro_test.mod",
    "carousel.mod",
)

EFI_FILES = (
    "grubx64.efi",
    "shimx64.efi",
    "mmx64.efi",
)


# Check GRUB initialization
if not (GRUB_PATH / "grub.cfg").is_file():
    sys.exit("GRUB does not appear to be initialized. Aborting.")

if not (GRUB_MODULE_PATH / "normal.mod").is_file():
    sys.exit("GRUB normal.mod not found. Aborting.")


# Check EFI mount
if not EFI_PATH.is_mount():
    sys.exit("EFI partition is not mounted at /boot/efi. Aborting.")


# Check all installed BroGRUB files before deleting anything
REQUIRED_FILES = [
    FONT_FILE,
    CONFIG_FILE,
    THEMES_PATH / "theme.txt",
    *(GRUB_MODULE_PATH / module for module in MODULES),
    *(EFI_BROGRUB / file for file in EFI_FILES),
]

for file in REQUIRED_FILES:
    if not file.is_file():
        sys.exit(f"BroGRUB installation incomplete or not found: {file}")


print("\033[1;93mBroGRUB installation detected.\033[0m")


# Check UEFI boot entries
if not shutil.which("efibootmgr"):
    sys.exit("efibootmgr is required to remove the BroGRUB boot entry.")

boot_info = subprocess.run(
    ["efibootmgr", "-v"],
    capture_output=True,
    text=True
)

if boot_info.returncode != 0:
    sys.exit("Could not read UEFI boot entries. Aborting.")


# Find BroGRUB boot entries
bro_entries = re.findall(
    r"^Boot([0-9A-Fa-f]{4})\*?\s+BroGRUB(?:\s|$)",
    boot_info.stdout,
    re.MULTILINE
)

# Check that another boot entry exists
all_entries = re.findall(
    r"^Boot([0-9A-Fa-f]{4})\*?\s+",
    boot_info.stdout,
    re.MULTILINE
)

other_entries = [
    entry for entry in all_entries
    if entry.upper() not in [item.upper() for item in bro_entries]
]

if bro_entries and not other_entries:
    sys.exit(
        "No other UEFI boot entry found. "
        "Refusing to remove your only boot option."
    )


# Ask for confirmation
print("\n\033[1;93mThe following BroGRUB files will be removed:\033[0m")
print(f"EFI directory: {EFI_BROGRUB}")
print(f"Configuration: {CONFIG_FILE}")
print(f"Fonts: {FONT_FILE}")
print(f"Theme: {THEMES_PATH}")
print(f"Modules: {', '.join(MODULES)}")
print(f"UEFI entries: {', '.join(bro_entries) if bro_entries else 'None'}")

confirm = input("\nAre you sure you want to uninstall BroGRUB? [y/N]: ")

if confirm.lower() != "y":
    sys.exit("Uninstallation cancelled.")


# Remove UEFI boot entries
print("\n\033[1;93mRemoving UEFI boot entries...\033[0m")

for entry in bro_entries:
    result = subprocess.run(
        ["efibootmgr", "-b", entry, "-B"]
    )

    if result.returncode != 0:
        sys.exit(f"Failed to remove UEFI entry Boot{entry}. Aborting.")


# Remove EFI directory
print("\033[1;93mRemoving BroGRUB EFI files...\033[0m")

shutil.rmtree(EFI_BROGRUB)


# Remove GRUB configuration and fonts
print("\033[1;93mRemoving BroGRUB configuration and fonts...\033[0m")

FONT_FILE.unlink()
CONFIG_FILE.unlink()


# Remove theme
print("\033[1;93mRemoving BroGRUB theme...\033[0m")

shutil.rmtree(THEMES_PATH)


# Remove custom modules
print("\033[1;93mRemoving BroGRUB modules...\033[0m")

for module in MODULES:
    (GRUB_MODULE_PATH / module).unlink()


print("\n\033[1;92mBroGRUB has been successfully uninstalled!\033[0m")
print("Other GRUB files and UEFI boot entries were preserved.")
print("Restart your computer to use the remaining boot option.")
