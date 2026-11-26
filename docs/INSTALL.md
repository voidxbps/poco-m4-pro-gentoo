# Full installation guide — Poco M4 Pro 4G (`fleur`)

This is a **destructive** procedure: the phone's internal storage will be repartitioned/formatted
for Gentoo. **Back up anything you need first.** The steps below have been used successfully on
a Poco M4 Pro 4G / Redmi Note 11S with an unlocked bootloader.

---

## 0. Prerequisites

On the host PC:

```bash
sudo apt install adb fastboot        # Debian/Ubuntu
# or install android-tools on your distro
```

Verify the phone is visible:

```bash
adb devices
# FMIZIRT4YP5TTORG    device      (Android/TWRP)
# FMIZIRT4YP5TTORG    fastboot    (bootloader)
```

You need:
- An unlocked bootloader (`fastboot flashing unlock` / OEM unlocking enabled).
- TWRP available (this project ships a known-good TWRP-based recovery image used during setup).
- USB ADB authorization on the host (accept the prompt once; see step 7 for the key file).

**Terminology:**
- `boot_a` / `boot_b` — the two boot slots; the kernel + initramfs live here (64 MB each).
- `userdata` (partition `sdc59`) — internal storage. In the **proven** setup it holds a
  12 GB ext4 image at `/data/gentoo.img` mounted by the initramfs.

---

## 1. Back up the stock partitions

Enter fastboot and save the original images. If anything goes wrong, these let you restore.

```bash
adb reboot bootloader
fastboot devices

# Identify the dynamic slots first
fastboot getvar current-slot

# Read back the stock boot/vbmeta/dtbo/vendor_boot from both slots.
# (Use your preferred method, e.g. dd from TWRP, or fastboot fetch where supported.)
# At minimum, keep a copy of the ORIGINAL boot_a and vbmeta_a.
```

From TWRP you can also do:

```bash
adb shell 'dd if=/dev/block/by-name/boot_a of=/data/boot_a.orig.img'
adb shell 'dd if=/dev/block/by-name/vbmeta_a of=/data/vbmeta_a.orig.img'
adb pull /data/boot_a.orig.img
adb pull /data/vbmeta_a.orig.img
```

---

## 2. Boot into TWRP (for setup only)

TWRP is required only for the one-time creation of the Gentoo rootfs. It is **not** used at
runtime. If you already have TWRP installed, boot to recovery. Otherwise:

```bash
fastboot flash boot_a recovery.img       # any working TWRP for fleur
fastboot set_active a
fastboot reboot recovery
adb devices                              # ... recovery
```

---

## 3. Prepare the Gentoo rootfs image on internal storage

The proven configuration keeps a 12 GB ext4 image at `/data/gentoo.img` on the f2fs
`userdata` partition.

```bash
# On the phone, in TWRP:
adb shell 'sh /data/device_setup.sh'
```

What `scripts/device_setup.sh` does, step by step:

1. Creates a **sparse** 12 GB file: `dd if=/dev/zero of=/data/gentoo.img bs=1 count=0 seek=12G`
2. Formats it ext4: `mke2fs -q -t ext4 -F -b 4096 /data/gentoo.img`
3. Attaches `/dev/block/loop0` and mounts it on `/mnt/gentoo`.
4. Extracts a Gentoo **arm64 OpenRC stage3** (`stage3-nohard.tar`) — get the current
   `stage3-arm64-openrc` from `https://www.gentoo.org/downloads/` and rename it accordingly,
   or reassemble the copy in `rootfs-parts/` (see `rootfs-parts/README.md`).
5. Copies TWRP's `/system` runtime into the rootfs — this provides the static `adbd`,
   `linker64` and shared libraries used by the Gentoo userspace for ADB.
6. Removes `udev`/`eudev` from the `sysinit` runlevel (there is no devtmpfs in this kernel;
   our static `/dev` nodes are used instead).
7. Writes an empty `/etc/fstab` (all mounts are done by the initramfs).
8. Sets hostname `pocogentoo` and the root password hash (password: **`gentoo`**).
9. Installs `99-bootmark.start` into `/etc/local.d/`.

If you prefer to start from the **prebuilt** rootfs instead of a stage3:

```bash
# On the host: reassemble and extract the prebuilt rootfs tarball
cd rootfs-parts
cat rootfs.tar.gz.part* > /tmp/rootfs.tar.gz
cd ..
adb push /tmp/rootfs.tar.gz /data/rootfs.tar.gz
adb shell 'mkdir -p /mnt/gentoo; mount -t f2fs /dev/block/sdc59 /data; \
           mke2fs -q -t ext4 -F -b 4096 /data/gentoo.img; \
           losetup /dev/block/loop0 /data/gentoo.img; \
           mount -t ext4 /dev/block/loop0 /mnt/gentoo; \
           tar xpf /data/rootfs.tar.gz -C /mnt/gentoo'
```

---

## 4. Install the Gentoo-side overlay and helpers

Copy the files from `rootfs-overlay/` into the rootfs. If you built from a stage3:

```bash
# Host side
cd rootfs-overlay
tar -cf - . | adb shell 'cd /mnt/gentoo && tar -xf -'
cd ..
adb shell 'chmod 755 /mnt/gentoo/etc/local.d/*.start /mnt/gentoo/sbin/gentty /mnt/gentoo/sbin/rebootboot'
```

Key pieces:

- `04-control.start` — creates `/dev/dri/card0`, `/dev/kmsg`, `/dev/ptmx`, mounts `devpts`,
  launches `/sbin/gentty` after 12 s, and runs the **reboot-to-fastboot** control loop.
- `00-usb.start` + `01-adbd.start` + `05-udc.start` — build the USB ADB gadget from configfs
  and start `adbd`.
- `sbin/rebootboot` — static helper calling `reboot(RESTART2, "bootloader")`.
- `sbin/gentty` — the on-screen framebuffer TTY.

Enable OpenRC's `local` service (it runs `/etc/local.d/*.start`):

```bash
adb shell 'chroot /mnt/gentoo rc-update add local default 2>/dev/null || \
           ln -sf /etc/init.d/local /mnt/gentoo/etc/runlevels/default/local'
```

---

## 5. ADB authorization

ADB on the Gentoo side uses the key file at `/data/misc/adb/adb_keys` on **userdata**.
It **must end with a newline**, otherwise ADB shows `unauthorized`.

```bash
# From TWRP, copy your host public key(s) to the phone:
cat ~/.android/adbkey.pub | adb shell 'cat > /data/misc/adb/adb_keys'
adb shell 'echo "" >> /data/misc/adb/adb_keys'      # ensure trailing newline
adb shell 'chmod 600 /data/misc/adb/adb_keys'
```

---

## 6. Flash the native boot image

```bash
adb reboot bootloader
fastflash() { fastboot flash "$1" "$2"; }
fastboot flash boot_a boot/phase15-boot.img
fastboot flash boot_b boot/phase15-boot.img
fastboot set_active a
```

If you replaced `vbmeta` or disabled verification, make sure AVB is disabled:

```bash
fastboot --disable-verity --disable-verification flash vbmeta_a vbmeta.img   # if needed
```

Do **not** flash TWRP to `boot_a` after this — that would replace the kernel.

---

## 7. First native boot

```bash
fastboot reboot
# Wait 60-90 seconds. The phone does NOT show the Google/TWRP UI; it boots Gentoo.
adb devices
# FMIZIRT4YP5TTORG    device
adb shell 'cat /etc/gentoo-release; echo; uname -a; id; mount | head -5'
```

Expected:
```
NAME='Gentoo'
Linux pocogentoo 4.14.186-perf-g53519c057ebf ... aarch64 GNU/Linux
uid=0(root) gid=0(root) ...
/dev/loop0 on / type ext4 (rw,...)
```

---

## 8. Verify the on-screen TTY

Look at the phone screen. After a few seconds you should see:

1. Real kernel log lines from `/dev/kmsg` (they stop when boot finishes).
2. `pocogentoo login:`
3. After login: `root@pocogentoo ~ #`

Log in from the host:

```bash
cd scripts
printf 'root\n'   | ./tty-send
printf 'gentoo\n' | ./tty-send
echo 'id; uname -a' | ./tty-send
./tty-read
```

---

## 9. Rebooting back to fastboot

```bash
adb shell 'echo 1 > /var/tmp/reboot-bootloader'
# phone reboots into fastboot within a few seconds
```

This must keep working — the control loop in `04-control.start` watches this file.

---

## 10. Restoring stock (if needed)

Reboot to fastboot and flash the original images you backed up in step 1:

```bash
fastboot flash boot_a boot_a.orig.img
fastboot flash vbmeta_a vbmeta_a.orig.img
fastboot set_active a
fastboot reboot
```
