#!/system/bin/sh
# Gentoo native-boot initramfs init for Xiaomi fleur (Poco M4 Pro 4G)
export PATH=/system/bin:/system/sbin:/sbin:/bin
mkdir -p /dev/block /dev/pts /mnt/data /newroot /config /dev/usb-ffs/adb
MARKDEV=/dev/block/sdc1
mknod $MARKDEV b 8 33 2>/dev/null
mark() { echo "$*" | dd of=$MARKDEV bs=4096 seek=2 conv=notrunc 2>/dev/null; }
mark "STEP 00: init started $(date)"

mknod /dev/null c 1 3 2>/dev/null
mknod /dev/zero c 1 5 2>/dev/null
mknod /dev/console c 5 1 2>/dev/null
mknod /dev/tty c 5 0 2>/dev/null
mknod /dev/ptmx c 5 2 2>/dev/null
mknod /dev/kmsg c 1 11 2>/dev/null
mknod /dev/block/sdc59 b 259 43 2>/dev/null
mknod /dev/block/sdc58 b 259 42 2>/dev/null
mknod /dev/loop-control c 10 237 2>/dev/null
for i in 0 1 2 3 4 5 6 7; do mknod /dev/block/loop$i b 7 $i 2>/dev/null; done
mark "STEP 01: nodes created"

mount -t proc proc /proc 2>/dev/null
mount -t sysfs sysfs /sys 2>/dev/null
mount -t devpts devpts /dev/pts 2>/dev/null
mark "STEP 02: proc/sys/pts mounted"

i=0
while [ $i -lt 15 ]; do
  mount -t f2fs /dev/block/sdc59 /mnt/data && break
  i=$((i+1))
  sleep 1
done
mark "STEP 03: data mount attempt $i rc=$?"
if ! grep -q " /mnt/data " /proc/mounts; then
  mark "STEP 03 FATAL: no /mnt/data mount"
  exit 1
fi

LOG=/mnt/data/gentoo-boot.log
echo "init started $(date)" > $LOG
mark "STEP 04: log file created"

losetup /dev/block/loop0 /mnt/data/gentoo.img >>$LOG 2>&1
mark "STEP 05: losetup rc=$?"
mount -t ext4 -o rw /dev/block/loop0 /newroot >>$LOG 2>&1
mark "STEP 06: rootfs mount rc=$?"
if [ ! -x /newroot/sbin/init ]; then
  mark "STEP 06 FATAL: no /newroot/sbin/init"
  exit 1
fi
mark "STEP 07: rootfs ready"

mount -t configfs none /config 2>/dev/null
mkdir -p /config/usb_gadget/g1/strings/0x409 /config/usb_gadget/g1/configs/b.1/strings/0x409 /config/usb_gadget/g1/functions/ffs.adb 2>/dev/null
echo 0x0223 > /config/usb_gadget/g1/bcdDevice 2>/dev/null
echo 0x0200 > /config/usb_gadget/g1/bcdUSB 2>/dev/null
echo 1 > /config/usb_gadget/g1/os_desc/use 2>/dev/null
echo FMIZIRT4YP5TTORG > /config/usb_gadget/g1/strings/0x409/serialnumber 2>/dev/null
echo xiaomi > /config/usb_gadget/g1/strings/0x409/manufacturer 2>/dev/null
echo fleur > /config/usb_gadget/g1/strings/0x409/product 2>/dev/null
mount -t functionfs adb /dev/usb-ffs/adb uid=2000,gid=2000 2>/dev/null
ln -sf /config/usb_gadget/g1/configs/b.1 /config/usb_gadget/g1/os_desc/b.1 2>/dev/null
ln -sf /config/usb_gadget/g1/functions/ffs.adb /config/usb_gadget/g1/configs/b.1/f1 2>/dev/null
echo 0x18d1 > /config/usb_gadget/g1/idVendor 2>/dev/null
echo 0xd001 > /config/usb_gadget/g1/idProduct 2>/dev/null
echo musb-hdrc > /config/usb_gadget/g1/UDC 2>/dev/null
mark "STEP 08: usb gadget set up"

chroot /newroot /system/bin/adbd >/mnt/data/adbd.log 2>&1 &
mark "STEP 09: adbd launched"

mount --bind /dev /newroot/dev
mount --bind /proc /newroot/proc
mount --bind /sys /newroot/sys
mount --bind /config /newroot/config 2>/dev/null
mark "STEP 10: switching root"
exec /system/bin/chroot /newroot /sbin/init >/dev/kmsg 2>&1
