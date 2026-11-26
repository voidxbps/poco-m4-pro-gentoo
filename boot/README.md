# Boot image (`phase15-boot.img`)

This is the **tested, working** native-boot image for `fleur`. Flash it to `boot_a` and
`boot_b`, set the active slot, and reboot. It does not touch the GPT or any Android partition
other than `boot_*`.

```bash
fastboot flash boot_a phase15-boot.img
fastboot flash boot_b phase15-boot.img
fastboot set_active a
fastboot reboot
```

## Format

MediaTek header v0, 64 MiB, page 2048:

| Field | Value |
|-------|-------|
| kernel size | 11,380,162 bytes (gzip, ~39 MB uncompressed) |
| kernel address | `0x40080000` |
| ramdisk size | 36,068,701 bytes |
| ramdisk address | `0x47c80000` |
| tags address | `0x4bc80000` |
| cmdline | `bootopt=64S3,32N2,64N2 twrpfastboot=1 buildvariant=eng` (stock bootloader appends its own) |
| DTB | stock MT6781 DTB at file offset `47525952` |
| tail | original MediaTek/AVB bytes preserved (19 MB) |

## Contents

- **kernel** — stock 4.14.186-perf vendor kernel built from the `fleur`/MT6781 OSS tree.
  It includes the MediaTek DRM/KMS, UFS, clock, pinctrl, musb/USB gadget and pstore drivers.
- **ramdisk** — gzip cpio containing:
  - `/init` — static ELF stage-1 (see `../initramfs/mkinit2.c`)
  - `/gentoo-init.sh` — stage-2 shell script
  - `/system/bin/sh`, `/system/bin/busybox-gentoo`, `/system/bin/loopattach`,
    `/system/lib64/libc.so` — tools copied from TWRP
  - core directories (`/dev`, `/proc`, `/sys`, `/data`, `/newroot`)
- **dtb-mt6781.dtb** — extracted for reference; the boot image uses the original bytes.

## Verification

```bash
python3 ../tools/unpack_boot.py phase15-boot.img /tmp/check
sha256sum phase15-boot.img kernel-4.14.186.gz ramdisk-4.14.cpio.gz dtb-mt6781.dtb
```

Do not repack with a different page size or kernel address — the bootloader is strict about
the MediaTek header layout.
