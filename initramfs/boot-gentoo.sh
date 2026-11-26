#!/system/bin/sh
# Run from TWRP: sh /data/boot-gentoo.sh
export PATH=/system/bin:/system/sbin:/sbin:/bin
mkdir -p /mnt/gentoo
losetup /dev/block/loop0 /data/gentoo.img
mount -t ext4 /dev/block/loop0 /mnt/gentoo
mount --bind /dev /mnt/gentoo/dev
mount --bind /proc /mnt/gentoo/proc
mount --bind /sys /mnt/gentoo/sys
echo "Entering Gentoo. root password: gentoo"
chroot /mnt/gentoo /bin/bash -l
