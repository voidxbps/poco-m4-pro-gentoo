#!/bin/busybox sh
# Phase-2 embedded initramfs: boot Gentoo from /data/gentoo.img as PID 1.
export PATH=/bin:/sbin:/usr/bin:/usr/sbin
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mkdir -p /dev/block /data /newroot /proc /sys
mknod /dev/null c 1 3 2>/dev/null
mknod /dev/console c 5 1 2>/dev/null
mknod /dev/kmsg c 1 11 2>/dev/null
mknod /dev/block/sdc59 b 259 43 2>/dev/null
mknod /dev/loop-control c 10 237 2>/dev/null
mknod /dev/block/loop0 b 7 0 2>/dev/null

mount -t f2fs /dev/block/sdc59 /data || exit 1
echo "K2: data mounted" > /data/kernel-init-ran
losetup /dev/loop0 /data/gentoo.img || exit 1
mount -t ext4 -o rw /dev/loop0 /newroot || exit 1
echo "K2: rootfs mounted" >> /data/kernel-init-ran

exec /bin/busybox switch_root /newroot /sbin/init
