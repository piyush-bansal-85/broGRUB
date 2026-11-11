# BroGRUB

> A customized GRUB boot environment. Built with curiosity, a lot of experimentation, and the desire to make booting your computer a little less boring.

**BroGRUB** is a customized boot environment built on top of GNU GRUB. It brings together a custom boot interface, themes, configuration tools, custom GRUB modules and a separate compiled release for installation.

This project isn't a bootloader written from scratch. It takes GRUB as its foundation and modifies and extends the environment to create something more personal, customizable and interactive.

The idea is simple: your bootloader doesn't have to look like something nobody has touched since 2005.

---

## What's inside?

This repository contains two parts of the BroGRUB project:

| Directory | What it contains |
|---|---|
| `BroGrub/` | Raw source code, GRUB source, custom modules, build scripts, themes and development tools. |
| `BroGRUB_compiled/` | Compiled EFI binaries, GRUB modules, theme assets, installation scripts and Secure Boot certificate material. |

You can either build BroGRUB yourself or use the prepared compiled release.

---

## Repository structure

```text
BroGRUB/
│
├── BroGrub/
│   ├── grub/                 # GRUB source code
│   ├── src/                  # Custom C modules and source files
│   ├── broLang/              # VS Code language extension
│   ├── bro-efi/              # EFI filesystem staging files
│   ├── bash/                 # EFI image and QEMU scripts
│   ├── scripts/              # Supporting Python scripts
│   └── init.sh               # Main build script
│
├── BroGRUB_compiled/
│   ├── efis/                 # Compiled EFI binaries
│   ├── brogrub-modules/      # Compiled GRUB modules
│   ├── customWorld/          # Theme and visual assets
│   ├── broLang/              # VS Code extension
│   ├── secure_boot/          # Public certificate and SBAT file
│   ├── scripts/              # Installer and uninstaller logic
│   ├── install.sh
│   └── uninstall.sh
│
└── README.md
```

The structure above is a simplified overview. Individual files and components may change as development continues.

---

# Part 1 — BroGrub (Source)

`BroGrub/` is the development side of the project.

It contains the GRUB source tree, BroGRUB-specific C code, configuration files, build scripts and supporting tools.

If you want to understand how BroGRUB is built, modify its implementation or experiment with its components, this is where you should start.

### Main components

**GRUB source**

The `grub/` directory contains the GRUB source code used as the foundation for BroGRUB.

**Custom modules**

The `src/` directory contains project-specific C code, including:

- `broThemeCompiler.c/.h`
- `carousel.c`
- `change_bg.c/.h`
- `loading.c/.h`
- `test.c`

These components provide additional functionality to the GRUB environment.

**BroLang**

`broLang/` is a VS Code language extension for editing BroGRUB `.bro` configuration files.

Its intended features include syntax highlighting, comments, hover documentation and context-aware autocomplete.

**Themes and assets**

The `bro-efi/` directory contains the EFI staging environment, including theme configurations, icons, fonts and other assets.

---

## Building from source

The main build script is:

```bash
./init.sh
```

### Requirements

The development environment requires:

- Linux
- x86_64 architecture
- Bash
- GCC and GRUB build dependencies
- GNU Make
- Python 3
- QEMU and suitable UEFI firmware for testing

### Build instructions

Clone the repository:

```bash
git clone https://github.com/YOUR_USERNAME/BroGRUB.git
```

Enter the source directory:

```bash
cd BroGRUB/BroGrub
```

Make the script executable:

```bash
chmod +x init.sh
```

Run the build:

```bash
./init.sh
```

By default, compilation uses parallel jobs:

```bash
make -j"$(nproc)"
```

This allows Make to use the available CPU threads.

### Sequential build mode

For sequential compilation with build output saved to `build.log`:

```bash
./init.sh log
```

This uses:

```bash
make -j1 2>&1 | tee build.log
```

This mode can be useful when investigating compilation errors or when parallel builds cause resource problems.

### What does init.sh do?

The current build process:

1. Cleans the previous GRUB configuration.
2. Removes the old build directory.
3. Recreates the custom source symlink.
4. Creates a fresh build directory.
5. Configures GRUB for x86_64 EFI.
6. Compiles GRUB.
7. Generates the EFI test image.
8. Launches the QEMU testing script.

The build directory is generated from scratch, so previous build output is not required in the repository.

---

# Part 2 — BroGRUB_compiled (Compiled Release)

`BroGRUB_compiled/` contains the prepared release files.

This is for people who want to install BroGRUB without manually building the GRUB source.

### What's included?

**EFI binaries**

The `efis/` directory contains:

- `grubx64.efi`
- `mmx64.efi`
- `shimx64.efi`

**Custom GRUB modules**

The `brogrub-modules/` directory contains:

- `bg_change.mod`
- `bro_test.mod`
- `carousel.mod`

**Theme and assets**

The `customWorld/` directory contains:

- `.bro` theme configuration files
- OS icons
- Interface sounds
- Theme configuration

**Secure Boot material**

The `secure_boot/` directory contains:

- `BroGRUB.der` — public certificate
- `brogrub.sbat` — SBAT-related metadata

**Installation scripts**

The `scripts/` directory contains the Python installation and uninstallation logic.

The top-level `install.sh` and `uninstall.sh` scripts provide the command-line entry points.

---

## Installation

### Requirements

The current installer targets:

- x86_64 architecture
- UEFI boot mode
- Linux
- Python 3
- An EFI System Partition mounted at `/boot/efi`
- Required UEFI management tools

The installation scripts use utilities such as:

- `efibootmgr`
- `findmnt`
- `lsblk`
- `mokutil`

### Install BroGRUB

Enter the compiled directory:

```bash
cd BroGRUB/BroGRUB_compiled
```

Make the scripts executable:

```bash
chmod +x install.sh uninstall.sh
```

Run the installer:

```bash
./install.sh
```

The installer requests administrator privileges when necessary.

The current installation process handles:

1. System and architecture checks.
2. UEFI boot-mode checks.
3. ESP mount checks.
4. EFI binary installation.
5. Font installation.
6. Theme installation.
7. Custom GRUB module installation.
8. BroGRUB configuration installation.
9. Secure Boot state and MOK certificate checks.
10. BroGRUB UEFI boot entry creation or detection.
11. BootOrder modification.

After successful installation, restart your computer and select BroGRUB if necessary.

### Secure Boot

BroGRUB includes EFI binaries prepared for its Secure Boot setup.

If the required certificate isn't enrolled on the target machine, the installer can request MOK enrollment.

The enrollment process may require a reboot and confirmation through the MOK Manager screen.

Secure Boot compatibility depends on the system firmware, shim, signatures, enrolled certificates and SBAT policies.

A successful test on one machine does not guarantee identical behavior on every machine.

**Never distribute your private signing key.** The included `.der` file is public certificate material.

---

## Uninstallation

BroGRUB includes an uninstaller.

From the compiled directory, run:

```bash
sudo ./uninstall.sh
```

The uninstaller checks the existing installation and requests confirmation before removing BroGRUB components.

It is designed to remove BroGRUB-specific files and its UEFI boot entry without deleting unrelated operating-system boot files.

**Important:** Uninstalling BroGRUB does not automatically remove an enrolled MOK certificate or restore the previous UEFI BootOrder.

Check your boot configuration before restarting.

---

# Testing and compatibility

The current project focuses on:

- Architecture: x86_64
- Firmware: UEFI
- Base: GNU GRUB
- Testing: QEMU and physical hardware

The source project includes scripts for generating EFI test images and launching QEMU.

Physical hardware testing is also important because firmware implementations and Secure Boot policies can differ.

Compatibility with other architectures, distributions and firmware configurations has not been established universally.

---

# Development and contributions

There is room for improvements in the boot interface, custom modules, configuration system, build process and compatibility.

If you want to contribute:

1. Fork the repository.
2. Create a branch for your changes.
3. Make your modifications.
4. Test the relevant functionality.
5. Submit a pull request explaining what you changed.

Bug reports, suggestions and improvements are welcome.

---

# Credits and licensing

BroGRUB is built on GNU GRUB and includes third-party components and assets.

Please retain the original copyright notices and licenses of third-party projects and themes.

Individual components may have different licenses. Check their respective license files before redistributing or modifying them.

