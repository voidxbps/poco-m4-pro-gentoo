# Experimental: Linux 6.18 LTS (work in progress)

**This is not usable as a daily driver.** The stable, documented install is the stock
4.14.186 kernel described in `README.md` and `INSTALL.md`. This page records the 6.18 LTS
port effort so others can continue it.

## Goal

- Real Linux **6.18.65 LTS** kernel (kernel.org longterm, EOL Dec 2028).
- Full MT6781 hardware support (UFS storage, display, USB gadget, clocks, pinctrl).
- Root on a **raw UFS partition** (no loop image): format `sdc59` directly as ext4 and
  `switch_root` into Gentoo.

## Why it is hard

Mainline Linux has **no MT6781 driver support**. Upstream only carries older/related MediaTek
SoCs (MT6779, MT6795/97, MT8173, MT8183, MT8192/95, MT6893). The Helio G96 (MT6781) and
G99 (MT6789) families were never upstreamed. The only 6.x trees with MT6781 drivers are
vendor/GKI trees:

| Base | Source | Relevance |
|------|--------|-----------|
| 6.6 | `Mayuri-Chan/MTK_kernel_device_modules_6.6`, Motorola/LineageOS MTK module trees | Has real `mt6781/` drivers |
| 6.12 | `iasnezhkov/android_kernel_ikko_mindone` (MT6789/ Helio G99 sibling) | Good porting reference |
| 6.18 | Google `android17-6.18` GKI | Core only; no MT6781 device modules |

## Work completed in `/mnt/Users/Lana/fleur-work/`

- `linux-6.18.65.tar.xz` extracted to `linux-6.18.65/` (1.7 GB).
- `Mayuri-Chan/MTK_kernel_device_modules_6.6.git` cloned to `kernel66/` (337 MB).
- MT6781 **clock + pinctrl** drivers vendored into `linux-6.18.65/drivers/mtk6781/` as a
  self-consistent block with prefixed Kconfig symbols (`MTKP_*`) to avoid collisions with
  mainline MediaTek drivers. Roughly 40 API differences were fixed (headers, gpio callbacks,
  `remove` signatures, etc.). `drivers/mtk6781/` compiles cleanly.
- DTB patched from the phone's own stock DTB:
  - UFS controller `mediatek,ufshci` → `mediatek,mt8183-ufshci` (mainline UFS host binds)
  - UFS PHY `mediatek,ufs_mphy` → `mediatek,mt8183-ufsphy`
  - Clocks `mediatek,topckgen` → `mediatek,mt6781-topckgen`, `infracfg_ao` → `mt6781-infracfg`,
    `apmixed` → `mt6781-apmixed`
  - `chosen/bootargs` set to a minimal console command line.
- Config: `ARCH_MEDIATEK`, `SCSI_UFSHCD`, `SCSI_UFS_MEDIATEK`, `PHY_MTK_UFS`, `EXT4_FS`,
  `BLK_DEV_INITRD`, `PSTORE_CONSOLE`, `PSTORE_RAM`, `USB_CONFIGFS`, `USB_DWC3`.
- `build618boot.py` / `build618-orig-layout.py` build a 64 MB header-v0 boot image with a
  dynamic kernel slot and the DTB at the original fixed offset (`47525952`).
- `gentoo-init-raw.sh` initramfs script: hardcodes `ROOTDEV=/dev/block/sdc59`, mounts it
  ext4 and `switch_root`s into `/sbin/init`.

## Current blocker

The 6.18 kernel image flashes successfully but the phone does **not** come back on USB after
boot (no ADB, no fastboot) and no 6.18 console output appears in pstore. The 4.14 recovery
kernel overwrites `console-ramoops` on the next boot, so early-boot diagnosis requires either
a serial console or an alternative non-destructive log channel. Likely causes under
investigation:

- MT6781-specific early init (reserved-memory nodes, clock/pinctrl probe) not yet correct.
- USB gadget/DWC3 glue for `mediatek,mt6781-usb20` missing (mainline has no binding).
- Bootloader loading constraints for a kernel larger than the original 11.38 MB slot (a
  slimmed 7.6 MB build was produced but not yet verified).

## Notes for continuing

- Keep the working 4.14 `phase15-boot.img` handy; always restore it before stopping.
- To read logs: boot 6.18, then force fastboot (Power+Volume Up), flash TWRP, and immediately
  `adb pull /sys/fs/pstore/console-ramoops`.
- Enable `PSTORE_CONSOLE`, `PSTORE_RAM` and pass
  `ramoops.mem_address=0x4d010000 ramoops.mem_size=0xe0000 ramoops.console_size=0x40000`.
- The bootloader appends its own `root=/dev/ram` **after** our cmdline, so hardcode the root
  device in the initramfs script rather than relying on `root=`.
- Prefer `fastboot flash boot_a` over `fastboot boot` — the latter fails on this bootloader
  with `usb_read failed: No such device`.
