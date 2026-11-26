# Gentoo Linux — Native Boot for Xiaomi Poco M4 Pro 4G / Redmi Note 11S (`fleur`)

This repository contains a **complete, working native-boot port of Gentoo Linux** for the
Poco M4 Pro 4G (Redmi Note 11S, codename **`fleur`**, MediaTek Helio G96 / MT6781).

No Android, no custom ROM, no TWRP at runtime. The phone boots **straight into Gentoo/OpenRC**
from the stock bootloader, with a working **on-screen TTY** (real kernel log + real
`/bin/login` + real shell) and **ADB access in a single boot**.

Everything here is based on the **stock 4.14.186 vendor kernel** (the only kernel with full
MT6781 hardware support). A separate, experimental Linux 6.18 LTS port is documented in
`docs/KERNEL-6.18.md` — it is a work in progress and not for daily use.

---

## Table of contents

1. [What this is / what works](#what-this-is--what-works)
2. [Quick start (for the impatient)](#quick-start)
3. [Full installation guide](docs/INSTALL.md)
4. [Architecture: how the boot chain works](docs/ARCHITECTURE.md)
5. [Using the phone after boot](#using-the-phone-after-boot)
6. [Troubleshooting](docs/TROUBLESHOOTING.md)
7. [Hardware notes / known issues](docs/HARDWARE-NOTES.md)
8. [Rebuilding everything from source](docs/KERNEL-BUILD.md)
9. [Experimental Linux 6.18 port](docs/KERNEL-6.18.md)
10. [Repository layout](#repository-layout)

---

## What this is / what works

- **Native boot** — `fastboot flash boot_a phase15-boot.img`, choose slot `a`, reboot. No TWRP needed after setup.
- **Both slots** supported (flash the same image to `boot_a` and `boot_b`).
- **Gentoo rootfs** on `/data/gentoo.img` (12 GB loop-mounted ext4) — this is the proven configuration.
  An experimental raw-UFS-root variant is described in `docs/KERNEL-6.18.md`.
- **PID 1 is real OpenRC** (`init [3]`), root is `/dev/loop0 on / type ext4`.
- **ADB in one boot** — gadget built from scratch with configfs; authorization via
  `/data/misc/adb/adb_keys` (must end with a newline).
- **Reboot to fastboot from Gentoo** — `adb shell 'echo 1 > /var/tmp/reboot-bootloader'`
  (never removed, never blocked).
- **On-screen framebuffer TTY** — real Terminus 8×16 console font, live kernel log,
  real `/bin/login` (`root` / `gentoo`), real bash shell. Type commands over ADB with
  `printf 'uname -a\n' > /var/tmp/tty-input`; read the reply from `/var/tmp/tty-output`.

### Known limitations

- Mali GPU / TEEI proprietary sources are missing, so there is **no GPU acceleration**.
  The framebuffer TTY is drawn directly on the DRM scanout buffer with raw ioctls.
- No Wi-Fi/Bluetooth/camera in the current setup (vendor firmware/HALs not ported).
- The vendor kernel is 4.14.186 (EOL); the 6.18 LTS path is experimental.

---

## Quick start

**Prerequisites:** unlocked bootloader, `adb` + `fastboot` on the host, TWRP available in
fastboot (for initial setup only), a microSD/USB or internal storage with the 12 GB image.

```bash
# 1. Enter fastboot (from Android/TWRP/Gentoo)
adb reboot bootloader          # or: adb shell 'echo 1 > /var/tmp/reboot-bootloader'

# 2. Flash the prebuilt working boot image
fastboot flash boot_a boot/phase15-boot.img
fastboot flash boot_b boot/phase15-boot.img
fastboot set_active a

# 3. First boot needs the Gentoo rootfs on /data/gentoo.img.
#    See docs/INSTALL.md — this is the one-time setup step.

# 4. Reboot and wait ~60 s
fastboot reboot
adb devices                    # FMIZIRT4YP5TTORG device
adb shell 'uname -a'
```

If you start from a completely empty phone, follow **[docs/INSTALL.md](docs/INSTALL.md)**
from step 1 — it covers building the rootfs, installing adb keys and flashing the image.

---

## Using the phone after boot

```bash
# ADB shell (normal)
adb shell
# uname -a
Linux pocogentoo 4.14.186-perf-g53519c057ebf #1 SMP PREEMPT ... aarch64 GNU/Linux

# Reboot from Gentoo straight into fastboot
adb shell 'echo 1 > /var/tmp/reboot-bootloader'

# Type into the on-screen TTY (login: root / password: gentoo)
cd scripts
printf 'root\n'   | ./tty-send
printf 'gentoo\n' | ./tty-send
echo 'uname -a'   | ./tty-send
./tty-read                  # shows the output captured from the framebuffer console
```

---

## Repository layout

```
phase15-boot.img            Prebuilt, working native-boot image (64 MB, MediaTek header v0)
boot/                       Final boot image and its extracted pieces
  phase15-boot.img          Same image as above
  kernel-4.14.186.gz        Stock vendor kernel (gzip)
  ramdisk-4.14.cpio.gz      Initramfs (gzip cpio) — stage-1 /init + stage-2 script
  dtb-mt6781.dtb            MT6781 device tree blob (stock)
initramfs/                  Sources used to build the initramfs
  mkinit2.c                 Minimal ELF PID1: mounts /data, logs, execs shell script
  loopattach.c              loop device attach helper (raw syscalls)
  init.sh / init2.sh        Earlier-generation init scripts (reference)
  gentoo-init.sh            Final stage-2 script used in the image
rootfs-overlay/             Files copied on top of a Gentoo stage3
  etc/local.d/00-usb.start  configfs USB gadget (adb)
  etc/local.d/01-adbd.start Starts adbd with TWRP libs
  etc/local.d/04-control.start  DRM node + TTY + reboot-to-fastboot loop
  etc/local.d/05-udc.start  UDC bind (delayed)
  etc/local.d/99-bootmark.start
  sbin/gentty               On-screen TTY binary (prebuilt)
  sbin/rebootboot           reboot(RESTART2, "bootloader") helper (prebuilt)
src/                        C sources for the helpers
  gentty.c / realtty.c      Framebuffer console + pty/login implementation
  font8x16.h                Real Terminus 8×16 console font
  rebootboot.c
scripts/                    Host/phone helper scripts (tty-send, tty-read, setup)
tools/                      Boot image unpack/repack and ramdisk builders
rootfs-parts/               Prebuilt Gentoo rootfs tarball, split into 40 MB parts
configs/                    Kernel config and boot cmdline
docs/                       Detailed documentation
```

---

## Credits / licensing

- Kernel and vendor drivers: GPL-2.0 (MediaTek/Xiaomi). See `LICENSES/`.
- Gentoo base system: Gentoo Linux (GPL/LGPL per package).
- Boot image layout follows the MediaTek header-v0 format used by the device stock ROM.
- Reverse-engineered and assembled by the community; **not affiliated with Xiaomi or MediaTek**.
