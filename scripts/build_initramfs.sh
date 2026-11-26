#!/bin/bash
set -e
cd "$(dirname "$0")"
rm -rf initramfs
cp -a boot_a/rd initramfs
rm -f initramfs/init
cp init.sh initramfs/init
chmod 755 initramfs/init
# A couple of symlinks the script relies on
[ -e initramfs/sbin ] || ln -s /system/bin initramfs/sbin
cd initramfs
find . -print0 | cpio -0 -o -H newc 2>/dev/null | gzip -9 -n > ../ramdisk-gentoo.cpio.gz
cd ..
ls -lh ramdisk-gentoo.cpio.gz
