#!/bin/bash

cd ~/BroOS/BroGrub || exit 1

echo -e "\033[91mCleaning previous GRUB configuration\033[0m"
cd grub
make distclean
cd ..

echo -e "\033[91mRemoving old build directory\033[0m"
rm -rf build

echo -e "\033[93mRemoving old src symlink\033[0m"
rm -rf grub/grub-core/bro

echo -e "\033[93mCreating new src symlink\033[0m"
ln -s ../../src grub/grub-core/bro

# echo -e "\033[93mUpdating bro_main source files\033[0m"

# python3 -u "scripts/overwriteMakeFile.py"

# echo -e "\033[92mSuccessfully Overwritten \033[3mMakefile.core.def\033[0m"

echo -e "\033[93mCreating new build directory\033[0m"
mkdir build

cd build || exit 1

echo -e "\033[96mConfiguring GRUB\033[0m"
../grub/configure --target=x86_64 --with-platform=efi || exit 1

echo -e "\033[92mBuilding GRUB\033[0m"

if [[ "$1" == "log" ]]; then
    echo -e "\033[2;1;39m[BroGRUB] Using sequential build mode...\033[0m"
    make -j1 2>&1 | tee build.log
    test "${PIPESTATUS[0]}" -eq 0 || exit 1
else
    echo -e "\033[2;1;39m[BroGRUB] Using parallel build mode...\033[0m"
    make -j"$(nproc)" || exit 1
fi

echo -e "\033[1;92mBuild completed successfully!\033[0m"

cd ..

./bash/EFI_image.sh
./bash/QEMU.sh