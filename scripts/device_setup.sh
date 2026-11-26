#!/system/bin/sh
# Run on the phone inside TWRP. Builds the Gentoo ext4 rootfs image on /data.
set -e
say() { echo "[setup] $*"; }

say "creating 12G sparse rootfs image"
rm -f /data/gentoo.img
dd if=/dev/zero of=/data/gentoo.img bs=1 count=0 seek=12G

say "formatting ext4"
mke2fs -q -t ext4 -F -b 4096 /data/gentoo.img

say "attaching loop device"
losetup /dev/block/loop0 /data/gentoo.img
losetup -a

mkdir -p /mnt/gentoo
mount -t ext4 /dev/block/loop0 /mnt/gentoo

say "extracting Gentoo stage3 (this takes a minute)"
tar xpf /data/stage3-nohard.tar -C /mnt/gentoo

say "copying TWRP /system runtime (adbd + libs + linker) into rootfs"
cp -a /system /mnt/gentoo/system

say "configuring Gentoo"
# No devtmpfs in this kernel; eudev would fight our static /dev.
rm -f /mnt/gentoo/etc/runlevels/sysinit/udev
rm -f /mnt/gentoo/etc/runlevels/sysinit/eudev
cat > /mnt/gentoo/etc/fstab <<'EOF'
# Managed by native-boot setup. Filesystems are mounted by the initramfs.
EOF
echo "pocogentoo" > /mnt/gentoo/etc/hostname
# root password: gentoo
sed -i '1s|^root:[^:]*:|root:$6$gentoosalta$fGjtoKXIa5mp1j4vhiHHpsFfhv.E1vyiAnyFp/l7r/bzJc94ZfaXGfuCt2J2bZvcNho2wy7QiO.srdcDug590/:|' /mnt/gentoo/etc/shadow
mkdir -p /mnt/gentoo/etc/local.d
cat > /mnt/gentoo/etc/local.d/99-bootmark.start <<'EOF'
#!/bin/sh
echo " gentoo userspace up $(date)" >> /mnt/gentoo-root-up
EOF
chmod 755 /mnt/gentoo/etc/local.d/99-bootmark.start

say "verifying"
ls -l /mnt/gentoo/sbin/init /mnt/gentoo/system/bin/adbd /mnt/gentoo/system/bin/linker64
head -1 /mnt/gentoo/etc/shadow
du -sh /mnt/gentoo
say "DONE"
