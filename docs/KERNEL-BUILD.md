# Rebuilding from source

You do **not** need to rebuild anything to install — `boot/phase15-boot.img` is the tested
image. These notes are for developers who want to modify the kernel, initramfs or rootfs.

## 1. Unpack the boot image

`tools/unpack_boot.py` reads the MediaTek header-v0 image and extracts kernel, ramdisk and DT:

```bash
python3 tools/unpack_boot.py boot/phase15-boot.img build/unpacked
# header_version=0 page=2048
# kernel: size=11380162 addr=0x40080000
# ramdisk: size=36068701 addr=0x47c80000
```

## 2. Kernel source

The stock 4.14.186 vendor source is **not** redistributed here (3.7 GB, vendor-licensed
components). Obtain it from the device vendor/Xiaomi GPL release or a community kernel tree
for `fleur`/MT6781, then build with the `fleur_defconfig`. The exact compiler used for the
prebuilt image was `aarch64-linux-gnu-gcc 14.2.0` (Debian).

The boot image stores the kernel gzip-compressed; the decompressed size is ~39 MB.

## 3. Initramfs

Sources are in `initramfs/`:

```bash
# Compile the static stage-1 PID1 helper
aarch64-linux-gnu-gcc -static -Os -o build/initramfs/init initramfs/mkinit2.c
aarch64-linux-gnu-gcc -static -Os -o build/initramfs/sbin/loopattach initramfs/loopattach.c

# Assemble the cpio (must contain init, /system/bin/sh, busybox, helpers)
cd build/initramfs
find . -print0 | cpio -0 -o -H newc | gzip -9 -n > ramdisk.cpio.gz
```

Add the prebuilt `system/` tree from TWRP (adbd, linker64, libc.so, sh, busybox) before
packing — the stage-2 script execs `/system/bin/sh`.

## 4. Repack the boot image

`tools/repack_boot.py` preserves the 64 MB layout and the original tail bytes:

```bash
python3 tools/repack_boot.py \
    boot/phase15-boot.img build/new-kernel.gz build/new-ramdisk.cpio.gz build/new-boot.img
```

If the ramdisk shrinks it is padded to the original size so every later offset (including
the DTB and AVB footer) stays identical.

## 5. Framebuffer TTY (`gentty`)

```bash
aarch64-linux-gnu-gcc -static -Os -o build/gentty src/gentty.c
# install as /sbin/gentty in the rootfs
```

The font is generated from a real Terminus 8×16 PSF2 file into `src/font8x16.h`.
`src/realtty.c` is the variant that streams `/dev/kmsg` and runs `/bin/login` on a pty;
`src/gentty.c` is the shipped simpler build. Both use raw DRM ioctls, no libdrm.

## 6. Rootfs

Start from a Gentoo arm64 OpenRC stage3, then overlay `rootfs-overlay/` and follow
`INSTALL.md` steps 3–4. To use the prebuilt rootfs instead, see `rootfs-parts/README.md`.

Reassemble the split tarball:

```bash
cd rootfs-parts
cat rootfs.tar.gz.part* > /tmp/rootfs-live.tar.gz
```

## 7. Reboot-to-fastboot helper

```bash
aarch64-linux-gnu-gcc -static -Os -o build/rebootboot src/rebootboot.c
# install as /sbin/rebootboot
```

It calls `reboot(RESTART2, "bootloader")`. The control loop in `04-control.start` invokes it
when `/var/tmp/reboot-bootloader` exists.

## 8. Testing safely

Always keep a working image:

```bash
cp boot/phase15-boot.img /tmp/good.img
fastboot flash boot_a build/new-boot.img
fastboot set_active a
fastboot reboot
# if broken: fastboot flash boot_a /tmp/good.img; fastboot set_active a; fastboot reboot
```

Read the console log from pstore in TWRP if a boot fails (see TROUBLESHOOTING.md).
