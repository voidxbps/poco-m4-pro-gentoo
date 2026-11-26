# Architecture — how native boot works on `fleur`

## Boot chain (slots)

The phone uses A/B slots with a **MediaTek header-v0** boot image (64 MiB, page 2048).
Only `boot_a` / `boot_b` are modified; `vbmeta*`, `dtbo*`, `vendor_boot*` can be left stock.

```
bootloader (lock state: unlocked)
  └─ boot_a or boot_b (phase15-boot.img)
       ├─ kernel       gzip-compressed stock 4.14.186 vendor kernel
       ├─ ramdisk      gzip cpio (stage-1 ELF /init + stage-2 shell script + tools)
       ├─ DTB          MT6781 device tree (stock, no changes required)
       └─ tail         original MediaTek/AVB data preserved byte-for-byte
```

## Initramfs stages

Because this kernel has no working framebuffer console of its own, the initramfs is minimal
and does the few things needed to reach Gentoo:

1. **Stage 1 — `/init`** (`initramfs/mkinit2.c`, a static ELF):
   - creates `/dev/block/userdata` (major 259, minor 43 = `sdc59`)
   - mounts `/data` as f2fs
   - writes a marker to `/dev/kmsg` and `/data/kernel-init-ran`
   - `execve("/system/bin/sh", ["/system/bin/sh", "/gentoo-init.sh"])`
2. **Stage 2 — `/gentoo-init.sh`** (inside the ramdisk):
   - mounts `/proc`, `/sys`
   - `loopattach` attaches `/data/gentoo.img` to `/dev/loop0` (raw `LOOP_SET_FD` ioctl)
   - mounts the ext4 loop device on `/newroot`
   - `mount --move /data /newroot/data` so internal storage is available inside Gentoo
   - `exec switch_root /newroot /sbin/init`

PID 1 is therefore Gentoo's real OpenRC init; `/proc/1/cmdline` shows `init [3]`.

## Why loop-mounted `/data/gentoo.img`?

MT6781 vendor drivers are only available for the 4.14 kernel. The proven setup makes no
changes to the GPT/partition layout: the loop image is a file on the existing f2fs
`userdata` partition. This makes installation reversible — restoring stock is just flashing
back the original `boot*` images.

An experimental variant that formats `sdc59` directly as ext4 (no loop device) is described
in `KERNEL-6.18.md` — it is not the recommended configuration.

## USB ADB gadget

The vendor kernel has the musb/ffs drivers but Android's `init` normally assembles the
gadget. In Gentoo we do it ourselves from configfs:

- `00-usb.start` — mounts configfs, creates `/config/usb_gadget/g1`, sets VID/PID
  `0x18d1:0xd001`, creates `ffs.adb`, mounts functionfs on `/dev/usb-ffs/adb`, binds UDC.
- `01-adbd.start` — starts `/system/bin/adbd` from TWRP's runtime copied into the rootfs
  (`LD_LIBRARY_PATH=/system/lib64`).
- `05-udc.start` — delayed `echo musb-hdrc > /config/usb_gadget/g1/UDC`.
- `/data/misc/adb/adb_keys` authorizes the host key (trailing newline required).

## Framebuffer TTY

Stock kernel + missing Mali sources means no `fbcon` text console. Instead:

- `04-control.start` creates `/dev/dri/card0` (226,0) and `/dev/kmsg`, mounts devpts.
- After 12 s, `gentty`:
  - opens `/dev/dri/card0`, becomes DRM master, sets a 1080×2400 mode via raw
    `DRM_IOCTL_MODE_SETCRTC`, binds the FB to the primary plane with `DRM_IOCTL_MODE_SETPLANE`.
  - draws text with a real **Terminus 8×16** font at native resolution into the mmap'd
    scanout buffer.
  - streams real `/dev/kmsg` lines, then forks `/bin/login` on a pty.
  - input is passed through `/var/tmp/tty-input`; output is mirrored to `/var/tmp/tty-output`.
- The same control loop watches `/var/tmp/reboot-bootloader` and calls `/sbin/rebootboot`.

## Reboot-to-fastboot

`rebootboot.c` is a tiny static ELF calling `reboot(RESTART2, "bootloader")`. It is invoked by
the control loop whenever `/var/tmp/reboot-bootloader` appears. It is deliberately **not**
a blocking helper, so the trigger keeps working indefinitely.

## File/partition summary

| Path | Role |
|------|------|
| `boot_a`/`boot_b` | kernel + initramfs (phase15-boot.img) |
| `userdata` (`sdc59`, f2fs) | `/data`, holds `gentoo.img` and ADB keys |
| `/data/gentoo.img` | 12 GB ext4 loop image = Gentoo `/` |
| `/system` (copied from TWRP) | static `adbd`, `linker64`, libs used by Gentoo userspace |
| `/var/tmp/tty-input` | host → on-screen TTY |
| `/var/tmp/tty-output` | on-screen TTY → host |
| `/var/tmp/reboot-bootloader` | trigger to reboot into fastboot |
