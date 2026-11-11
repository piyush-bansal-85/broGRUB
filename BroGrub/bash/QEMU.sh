#!/bin/bash

echo -e "\033[1;38;5;208mLaunching QEMU!\033[0m"

/usr/bin/qemu-system-x86_64 \
  -machine q35 \
  -m 512M \
  -drive format=raw,file=bro-efi.img \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=bro-efi-vars.fd