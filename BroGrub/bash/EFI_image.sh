#!/bin/bash
set -euo pipefail

cd "/home/flyking/BroOS/BroGrub" || exit 1

echo -e "\033[96mCreating EFI image\033[0m"

if [ ! -f "/tmp/brogrub.cfg" ]; then
    echo "File does not exist, creating it..."
    cat > "/tmp/brogrub.cfg" <<'EOF'
set root=(hd0,gpt1)
set prefix=($root)/boot/grub
EOF
fi

build/grub-mkimage \
  -O x86_64-efi \
  -o bro-efi/EFI/BOOT/BOOTX64.EFI \
  -p /boot/grub \
  -c /tmp/brogrub.cfg \
  -d "$PWD/build/grub-core" \
  part_gpt fat ext2 normal configfile echo \
  terminal efi_gop \
  video gfxmenu font gfxterm_background png \
  gfxterm play linux \
  bro_test carousel bg_change

if [ ! -s bro-efi/EFI/BOOT/BOOTX64.EFI ]; then
    echo -e "\033[1;91mERROR: BOOTX64.EFI was not created correctly!\033[0m"
    exit 1
fi

echo -e "\033[1;92mEFI image created successfully!\033[0m"

echo -e "\033[1;94mUpdating EFI disk image\033[0m"

LOOPDEV=$(sudo losetup --find --show --partscan bro-efi.img)

if [ -z "$LOOPDEV" ]; then
    echo -e "\033[1;91mFailed to create loop device!\033[0m"
    exit 1
fi

echo -e "\033[96mUsing loop device: $LOOPDEV\033[0m"

sudo mount "${LOOPDEV}p1" /mnt/bro-efi

if [ $? -ne 0 ]; then
    echo -e "\033[1;91mFailed to mount EFI partition!\033[0m"
    sudo losetup -d "$LOOPDEV"
    exit 1
fi

echo -e "\033[93mCopying EFI files...\033[0m"

sudo mkdir -p /mnt/bro-efi/EFI
sudo cp -r bro-efi/EFI/. /mnt/bro-efi/EFI/

echo -e "\033[93mCopying GRUB files...\033[0m"

sudo mkdir -p /mnt/bro-efi/boot/grub
sudo cp -r bro-efi/boot/grub/. /mnt/bro-efi/boot/grub/

sudo umount /mnt/bro-efi
sudo losetup -d "$LOOPDEV"

echo -e "\033[1;92mEFI image updated successfully!\033[0m"