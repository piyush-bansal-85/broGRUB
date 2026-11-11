## This is the raw codebase for the GRUB, it is used to develop the shown interface.

BroGRUB is a project built around **GNU GRUB**, focused on making the boot experience more customizable, interactive and visually interesting.

It is not a bootloader written from scratch. It uses GRUB as its foundation and extends/customizes it with project-specific code, configurations, themes and supporting tools.

The goal is simple: make the time between turning on your computer and reaching your operating system a little less boring.

---

## What is BroGRUB?

BroGRUB is a customized GRUB environment that brings together:

- A custom GRUB theme and visual interface.
- Custom GRUB modules written in C.
- A configuration format called `.bro`.
- BroLang, a VS Code language extension for editing `.bro` files.
- Custom fonts, icons, backgrounds and sounds.
- Build scripts for generating the GRUB EFI environment.
- QEMU scripts for testing the generated EFI environment.

The project is currently focused on **x86_64 UEFI systems**.

---

## Project structure

Here is a simplified overview of the source project:

```text
BroGrub/
│
├── grub/
│   └── GRUB source code
│
├── src/
│   ├── broThemeCompiler.c
│   ├── broThemeCompiler.h
│   ├── carousel.c
│   ├── change_bg.c
│   ├── change_bg.h
│   ├── loading.c
│   ├── loading.h
│   └── test.c
│
├── broLang/
│   ├── extension.js
│   ├── package.json
│   ├── data/
│   └── syntaxes/
│
├── bro-efi/
│   └── EFI filesystem staging directory
│
├── bash/
│   ├── EFI_image.sh
│   └── QEMU.sh
│
├── scripts/
│   ├── font-convertor.py
│   ├── load_fonts.py
│   └── overwriteMakeFile.py
│
├── init.sh
└── .gitignore
```

Some generated files and directories are intentionally excluded from the repository.

The exact contents may change as development continues.

---

## Building BroGRUB

The main build script is `init.sh`.

It handles the preparation and compilation of the GRUB source and then runs the EFI image and QEMU scripts.

### Requirements

The build environment should have:

- Linux
- x86_64 architecture
- Bash
- GCC and the required build tools
- Make
- GRUB build dependencies
- Python 3
- QEMU with UEFI firmware support
- The required GRUB source and project files

The exact dependency packages depend on your Linux distribution.

### Build instructions

Clone the repository:

```bash
git clone https://github.com/YOUR_USERNAME/BroGRUB.git
```

Enter the raw source directory:

```bash
cd BroGRUB/BroGrub
```

Make the build script executable if needed:

```bash
chmod +x init.sh
```

Run the build:

```bash
./init.sh
```

By default, the build uses parallel compilation:

```bash
make -j"$(nproc)"
```

This allows Make to use the available CPU threads instead of compiling everything one job at a time.

### Sequential build mode

If you want the original sequential build with its output saved to `build.log`, run:

```bash
./init.sh log
```

This uses:

```bash
make -j1 2>&1 | tee build.log
```

Sequential compilation can be useful when investigating build errors or when parallel compilation causes resource problems.

### What does init.sh do?

The build process currently follows this sequence:

1. Moves into the BroGRUB source directory.
2. Runs `make distclean` on the GRUB source.
3. Removes the previous build directory.
4. Removes the previous custom source symlink.
5. Creates a new symlink connecting `src/` to `grub/grub-core/bro`.
6. Creates a fresh build directory.
7. Configures GRUB for the x86_64 EFI platform.
8. Compiles GRUB.
9. Runs `EFI_image.sh`.
10. Runs `QEMU.sh`.

This is a clean-build workflow. Previous build output is discarded rather than reused.

---

## Custom GRUB modules

BroGRUB contains custom modules written in C.

The current source includes:

| Source | Purpose |
|---|---|
| `broThemeCompiler.c/.h` | Custom BroGRUB theme compilation logic |
| `carousel.c` | Carousel-related functionality |
| `change_bg.c/.h` | Background-changing functionality |
| `loading.c/.h` | Loading-related functionality |
| `test.c` | Test module/source |

The corresponding compiled modules are distributed separately in the compiled project.

These modules are intended to extend the GRUB environment with BroGRUB-specific functionality.

They must be compiled for the appropriate GRUB version and target platform.

---

## BroLang

BroLang is a VS Code language extension developed for BroGRUB's `.bro` configuration files.

Its purpose is to make editing BroGRUB configuration easier and more comfortable.

### Intended features

- Syntax highlighting.
- `#` line comments.
- Hover documentation for recognized keys.
- Context-aware autocomplete.
- Suggestions for operating-system/menu class names.

### Example `.bro` configuration

```text
# OS-to-image mappings
ubuntu:icons/Ubuntu_GRUB_blur.png
windows:icons/Window_GRUB_blur.png
others:icons/others_blur.png
```

The key represents an OS/menu class recognized by BroGRUB's configuration logic.

The value represents the image path associated with that key.

**Important:** The actual supported keys and path behavior depend on BroGRUB's implementation. BroLang's suggestions do not automatically detect installed operating systems or guarantee that a particular GRUB menu class exists.

For more information about the extension, see:

`broLang/README.md`

---

## Themes and customization

BroGRUB includes its own theme configuration and assets.

The theme contains:

- `.bro` configuration files.
- OS icons.
- Background images.
- Loading-related assets.
- Sounds.
- Theme configuration.

The main theme assets are stored in the GRUB EFI staging directory under:

```text
bro-efi/boot/grub/themes/
```

The `customWorld` directory contains the BroGRUB theme.

The project also includes the Starfield theme and its associated assets.

### Fonts

BroGRUB uses GRUB-compatible `.pf2` font files.

Font conversion and loading scripts are available under:

```text
scripts/
```

The font files used by the EFI staging environment are stored under:

```text
bro-efi/boot/grub/fonts/
```

---

## Testing

BroGRUB includes scripts for creating an EFI test image and launching QEMU.

```text
bash/
├── EFI_image.sh
└── QEMU.sh
```

These scripts are executed by `init.sh` after compilation.

QEMU testing provides a way to inspect the generated boot environment without repeatedly rebooting the physical machine.

Actual hardware testing is still important, especially for firmware compatibility and Secure Boot behavior.

---

## Secure Boot

BroGRUB has a Secure Boot setup intended for its supported UEFI environment.

The compiled release includes EFI binaries and public certificate material.

Secure Boot behavior depends on the firmware, shim, signatures, enrolled certificates and the binaries being loaded.

A successful test on one machine does not guarantee identical behavior on every machine.

Do not disable Secure Boot or modify your existing boot configuration without understanding the consequences.

---

## Current scope

The current development target is:

- Architecture: x86_64
- Firmware: UEFI
- Boot environment: GNU GRUB-based
- Testing: QEMU and physical hardware

Other architectures and firmware configurations are outside the current release scope.

---

## Important notes

- BroGRUB is based on GNU GRUB.
- The `build/` directory is generated by `init.sh` and is not required to be committed.
- EFI test images and other generated build artifacts are not the source of truth.
- Custom modules must match the GRUB version and target platform.
- The compiled release is maintained separately in `broGRUB_compiled/`.
- Do not publish private Secure Boot signing keys.

---

## Contributing

Suggestions, bug reports and improvements are welcome.

If you want to contribute:

1. Fork the repository.
2. Create a branch for your changes.
3. Make your changes.
4. Test the relevant build or functionality.
5. Submit a pull request with a description of what changed.

Please keep changes focused and explain any GRUB-specific modifications.

---

## Credits and licensing

BroGRUB builds on GNU GRUB and includes third-party assets.

Please retain the original copyright notices and licenses for third-party components and themes.

The licensing of individual components may differ. Check their respective license files before redistributing or modifying them.

---

## Final words

BroGRUB started as an attempt to make the GRUB boot experience something more personal.

It is a learning project, an experiment and an ongoing development effort.

There is still plenty to improve, but that's part of the fun.

**Boot different. Boot BroGRUB.**
