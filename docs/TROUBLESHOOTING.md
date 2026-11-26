# Troubleshooting

## `adb devices` shows `unauthorized`

The ADB key file on the phone must contain your host public key **and end with a newline**.
From TWRP:

```bash
cat ~/.android/adbkey.pub | adb shell 'cat > /data/misc/adb/adb_keys'
adb shell 'echo "" >> /data/misc/adb/adb_keys'
adb shell 'chmod 600 /data/misc/adb/adb_keys'
adb kill-server && adb start-server
```

Also verify `/data` is mounted (initramfs mounts it as f2fs) and that you accepted the
prompt on the host once.

## Phone boots to black screen / no logo

- Confirm `fastboot getvar current-slot` is `a` (or that the slot you flashed is active).
- Re-flash `boot/phase15-boot.img` to **both** slots:
  ```bash
  fastboot flash boot_a boot/phase15-boot.img
  fastboot flash boot_b boot/phase15-boot.img
  fastboot set_active a
  fastboot reboot
  ```
- Do not use `--disable-verity` flags on a device whose `vbmeta` is already stock-unlocked
  unless you know what you are doing.

## ADB never appears after native boot

Wait at least 60–90 s (OpenRC + USB gadget setup). Then check from TWRP:

```bash
adb shell 'cat /gentoo-pid1.txt'          # should contain PID1 cmdline (init [3])
adb shell 'cat /data/kernel-init-ran'     # stage-1 marker
adb shell 'cat /var/log/usb.log'          # gadget assembly log
adb shell 'cat /var/log/adbd.log'         # adbd log
adb shell 'cat /mnt/gentooboot.log'       # initramfs log (if present)
```

Common causes:
- `/data/misc/adb/adb_keys` missing/without trailing newline.
- TWRP `/system` runtime not copied into the rootfs (`adbd` or `libc.so` missing).
- `local` OpenRC service not enabled, so `/etc/local.d/*.start` never ran.

## `gentty` not on screen

Check `/var/log/gentty.log` and `/data/drmterm-status` (from TWRP or ADB):

```bash
adb shell 'cat /var/log/gentty.log'
adb shell 'cat /data/drmterm-status'
```

Expected final lines: `SETCRTC-OK`, `PLANES=10`, `SETPLANE 41 rc=0`, `KMSG-FD=...`,
`LOGIN-PID=...`. If `SETCRTC-FAIL errno=13`, another process holds DRM master — reboot.

## Reboot-to-fastboot stopped working

Ensure `04-control.start` is installed and executable and that the `local` OpenRC service
is enabled:

```bash
adb shell 'ls -l /etc/local.d/04-control.start'
adb shell 'rc-update show default | grep local'
```

The trigger is `adb shell 'echo 1 > /var/tmp/reboot-bootloader'`. If it does not fire,
hold **Power + Volume Up** to enter fastboot manually.

## Kernel log capture / pstore

The stock 4.14 kernel writes a console log to `console-ramoops` in pstore. Read it from TWRP:

```bash
adb pull /sys/fs/pstore/console-ramoops kernel-log.txt
grep -iE 'error|fail|panic|BUG' kernel-log.txt | head -40
```

Note: a subsequent boot overwrites the console record, so pull it immediately.

## DTB / display issues

Use the stock DTB (`boot/dtb-mt6781.dtb`) unmodified. Do not replace it with a mainline
6.18 DTB on the 4.14 kernel. The panel is initialized by the vendor `lcm`/`ddp` drivers.

## Restoring stock Android

```bash
adb reboot bootloader
fastboot flash boot_a boot_a.orig.img        # from step 1 of INSTALL.md
fastboot flash boot_b boot_b.orig.img
fastboot flash vbmeta_a vbmeta_a.orig.img
fastboot flash vbmeta_b vbmeta_b.orig.img
fastboot set_active a
fastboot reboot
```

## Device-specific facts

- Device: Poco M4 Pro 4G / Redmi Note 11S, codename `fleur`.
- SoC: MediaTek Helio G96 (MT6781), 2× Cortex-A76 + 6× Cortex-A55, Mali-G57 MC2.
- Display: 1080×2400 AMOLED, USB UDC name `musb-hdrc`, userdata `sdc59` (259,43).
- ADB serial observed: `FMIZIRT4YP5TTORG`.
- Root password: `gentoo`.
- Boot image page size 2048, header v0, 64 MiB, kernel load address `0x40080000`,
  ramdisk `0x47c80000`, tags `0x4bc80000`.
