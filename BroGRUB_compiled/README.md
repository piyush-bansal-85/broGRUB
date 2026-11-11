# BroGRUB — Compiled Release

> The ready-to-install BroGRUB package. Built from the BroGRUB source project and prepared for deployment on supported x86_64 UEFI systems.

This directory contains the compiled and packaged files required to install BroGRUB.

Unlike the raw source project, this package is intended for users who want to install and use the prepared BroGRUB environment without building GRUB themselves.

---

## System requirements

The current package is intended for:

- x86_64 computers.
- UEFI firmware.
- A Linux installation.
- Python 3.
- An EFI System Partition (ESP).
- The required UEFI management utilities.

The installer currently expects the ESP to be mounted at:

```text
/boot/efi
```

The following utilities are used by the installation process:

- `efibootmgr`
- `findmnt`
- `lsblk`
- `mokutil`
- `python3`

Make sure the required utilities are installed before proceeding.

---

## Installation

**Important: Installing a bootloader changes your system's boot configuration.**

Before installing Back up important data.

### 1. Download the package

Clone the repository:

```bash
git clone https://github.com/YOUR_USERNAME/BroGRUB.git
```

Enter the compiled directory:

```bash
cd BroGRUB/BroGRUB_compiled
```

### 2. Make the scripts executable

```bash
chmod +x install.sh uninstall.sh
```

### 3. Install

Run:

```bash
./install.sh
```

The installer checks the system requirements and requests administrator privileges when necessary.

The Python installation script handles the file installation and Secure Boot certificate checks.

### 4. Secure Boot certificate enrollment

BroGRUB includes public certificate material under:

```text
secure_boot/BroGRUB.der
```

If the required certificate is not enrolled, the installer can request its enrollment through MOK.

The enrollment process may require a reboot and confirmation through the firmware/MOK management screen.

Follow the instructions displayed by the installer and your system.

### 5. Reboot

After installation completes, restart your computer.

BroGRUB should appear as a UEFI boot option.

---

## What does the installer do?

The installation scripts are responsible for deploying the compiled BroGRUB files.

The current installation design includes:

1. Checking the architecture.
2. Checking that the system is booted in UEFI mode.
3. Checking Python and required system utilities.
4. Checking the ESP mount point.
5. Copying the EFI binaries into:

   ```text
   /boot/efi/EFI/BroGRUB/
   ```

6. Installing the font archive under `/boot/grub`.
7. Installing the custom theme under:

   ```text
   /boot/grub/themes/customWorld/
   ```

8. Installing the custom GRUB modules under:

   ```text
   /boot/grub/x86_64-efi/
   ```

9. Installing the BroGRUB configuration.
10. Checking Secure Boot state and certificate enrollment.
11. Creating or locating the BroGRUB UEFI boot entry.
12. Placing the BroGRUB entry first in the boot order.

The exact behavior is defined by the current `install.sh` and `scripts/install.py` implementations.

---

## Secure Boot

BroGRUB includes EFI binaries prepared for its Secure Boot setup.

The package contains:

| File | Purpose |
|---|---|
| `grubx64.efi` | BroGRUB GRUB EFI executable |
| `shimx64.efi` | Shim EFI executable |
| `mmx64.efi` | MOK Manager EFI executable |
| `BroGRUB.der` | Public certificate used for MOK enrollment |
| `brogrub.sbat` | SBAT-related metadata |

Secure Boot depends on the trust chain between firmware, shim, enrolled certificates and the EFI binaries.

If the certificate is not enrolled on the target machine, the MOK enrollment process may be required.

A working Secure Boot configuration on the developer's computer does not automatically mean every other computer will accept the same binaries.

Firmware behavior and existing Secure Boot policies can differ.

---

## Themes and assets

The `customWorld/` directory contains the BroGRUB theme files.

```text
customWorld/
├── bro-themes/
│   ├── broBg.bro
│   ├── broFg.bro
│   ├── broLoadingConfig.bro
│   └── carouselConfig.bro
│
├── icons/
│   └── OS and interface icons
│
├── sounds/
│   └── Interface sounds
│
└── theme.txt
```

The `.bro` files contain BroGRUB-specific configuration.

The theme uses icons, backgrounds, fonts and sounds to customize the boot interface.

The exact behavior of individual theme settings depends on the corresponding BroGRUB implementation.

---

## BroLang

BroLang is the VS Code language extension for BroGRUB `.bro` configuration files.

It is included to make editing theme configuration more convenient.

Its intended features include:

- Syntax highlighting.
- `#` comments.
- Hover documentation.
- Context-aware autocomplete.

For more details, check:

```text
broLang/README.md
```

---

## Uninstallation

BroGRUB includes an uninstaller.

From this directory, run:

```bash
sudo ./uninstall.sh
```

The uninstaller checks the existing installation and asks for confirmation before removing BroGRUB files.

The intended removal includes:

- The BroGRUB UEFI boot entry.
- BroGRUB EFI files.
- BroGRUB configuration.
- BroGRUB theme files.
- BroGRUB-specific modules.
- The installed BroGRUB font archive.

It is designed to leave unrelated operating-system boot files alone.

### Important Secure Boot note

Uninstalling BroGRUB does not necessarily remove its enrolled MOK certificate from firmware.

Certificate removal is a separate firmware/MOK management operation.

Also, removing the BroGRUB boot entry does not automatically restore your previous UEFI BootOrder.

Check your available boot options before rebooting.

---

## Troubleshooting

### BroGRUB does not appear in the boot menu

Check:

- Whether the installation completed successfully.
- Whether the ESP is mounted at `/boot/efi`.
- Whether the BroGRUB UEFI entry exists.
- Whether your firmware allows selecting the BroGRUB entry.

You can inspect UEFI entries with:

```bash
sudo efibootmgr -v
```

### Secure Boot rejects BroGRUB

Check:

- Whether Secure Boot is enabled.
- Whether the required MOK certificate has been enrolled.
- Whether the EFI binaries are signed as expected.
- Whether the firmware or shim reports a signature or SBAT rejection.

Do not disable Secure Boot as a blind troubleshooting step.

### Theme or icons are missing

Check that the installation contains:

```text
/boot/grub/themes/customWorld/
```

Also verify that the theme configuration refers to the correct image paths.

### Custom modules are missing

Check:

```text
/boot/grub/x86_64-efi/
```

The installed modules must match the GRUB version and target platform.

---

## Known scope and limitations

The current release is focused on x86_64 UEFI Linux systems.

It is not intended to be a universal installer for every Linux distribution, architecture or firmware configuration.

Before using it on another system, verify:

- ESP mount configuration.
- GRUB version compatibility.
- EFI binary compatibility.
- Secure Boot trust requirements.
- Existing bootloader configuration.

The release has been tested in the developer's environment. Compatibility with other hardware and distributions should be verified separately.

---

## Source project

The raw source, build scripts and development files are maintained in the `BroGrub/` directory of the BroGRUB repository.

If you want to build BroGRUB yourself, start there.

---

## Credits and licensing

BroGRUB is built on GNU GRUB and includes third-party assets.

Please retain original copyright notices and license files for third-party components.

Individual components may have different licenses. Check their respective license files before redistribution.
