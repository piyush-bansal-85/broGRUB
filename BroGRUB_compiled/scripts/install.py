import sys
import shutil
import subprocess
from pathlib import Path

# Project paths
CURRENT_DIR = Path(__file__).resolve().parent.parent

GRUB_PATH = Path("/boot/grub")
EFI_PATH = Path("/boot/efi")

FONT_NAME = "BroGRUB-fonts.tar"
THEME_NAME = "customWorld"

EFI_NAME_PATH = CURRENT_DIR / "efis"
DEST_EFI_NAME_PATH = EFI_PATH / "EFI" / "BroGRUB"

MODULES_PATH = CURRENT_DIR / "brogrub-modules"
GRUB_MODULE_PATH = GRUB_PATH / "x86_64-efi"

CERTIFICATE = CURRENT_DIR / "secure_boot" / "BroGRUB.der"


# Check required directories and files
if not EFI_PATH.is_mount():
    sys.exit("\033[95mEFI partition is not mounted at /boot/efi.\033[0m")

if not GRUB_PATH.exists():
    sys.exit("\033[95m/boot/grub directory not found.\033[0m")

if not GRUB_MODULE_PATH.exists():
    sys.exit("\033[95mx86_64-efi module directory not found.\033[0m")

for file in (
    CURRENT_DIR / FONT_NAME,
    EFI_NAME_PATH / "grubx64.efi",
    EFI_NAME_PATH / "shimx64.efi",
    EFI_NAME_PATH / "mmx64.efi",
    CERTIFICATE,
):
    if not file.is_file():
        sys.exit(f"\033[95mRequired file not found: {file}\033[0m")


# Copy EFI files
print("\033[1;93mInstalling EFI files...\033[0m")

DEST_EFI_NAME_PATH.mkdir(parents=True, exist_ok=True)

for file in EFI_NAME_PATH.glob("*.efi"):
    shutil.copy2(file, DEST_EFI_NAME_PATH / file.name)

print("\033[1;92mEFI files installed!\033[0m")


# Install fonts
print("\033[1;93mInstalling fonts...\033[0m")

shutil.copy2(CURRENT_DIR / FONT_NAME, GRUB_PATH)
shutil.copy2(CURRENT_DIR / "brogrub.cfg", GRUB_PATH)

print("\033[1;92mFonts installed!\033[0m")


# Install theme
print("\033[1;93mInstalling theme...\033[0m")

THEMES_PATH = GRUB_PATH / "themes"
THEMES_PATH.mkdir(parents=True, exist_ok=True)

shutil.copytree(
    CURRENT_DIR / THEME_NAME,
    THEMES_PATH / THEME_NAME,
    dirs_exist_ok=True
)

print("\033[1;92mTheme installed!\033[0m")


# Install custom GRUB modules
print("\033[1;93mInstalling custom modules...\033[0m")

for broMod in MODULES_PATH.glob("*.mod"):
    shutil.copy2(broMod, GRUB_MODULE_PATH / broMod.name)

print("\033[1;92mAll .mod files copied!\033[0m")


# Secure Boot / MOK enrollment
print("\033[1;93mChecking Secure Boot certificate...\033[0m")

if shutil.which("mokutil"):
    secure_boot = subprocess.run(
        ["mokutil", "--sb-state"],
        capture_output=True,
        text=True
    )

    if "enabled" in secure_boot.stdout.lower():
        enrolled = subprocess.run(
            ["mokutil", "--test-key", str(CERTIFICATE)],
            capture_output=True,
            text=True
        )

        if enrolled.returncode != 0:
            print("\033[1;93mRequesting MOK enrollment...\033[0m")
            subprocess.run(["mokutil", "--import", str(CERTIFICATE)])
        else: print("\033[1;92mBroGRUB certificate already enrolled!\033[0m")
    else: print("Secure Boot is disabled. Skipping MOK enrollment.")
else:
    print("mokutil not found. Skipping MOK enrollment.")


print("\n\033[1;92mBroGRUB installation files copied successfully!\033[0m")
print("\033[2;1;93mIf MOK enrollment was requested, complete it after reboot in MokManager.\033[0m")