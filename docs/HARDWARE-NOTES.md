# Hardware notes / known issues (kernel 4.14.186)

## Working

| Subsystem | Status | Notes |
|-----------|--------|-------|
| CPU / SMP | OK | 2× A76 + 6× A55, stock vendor scheduler |
| UFS storage | OK | 4.14 vendor UFS driver, rootfs on `/data/gentoo.img` |
| DRM scanout | OK (manual) | KMS modeset via raw ioctls; no GPU acceleration |
| USB gadget (ADB) | OK | configfs + functionfs; UDC `musb-hdrc` |
| Serial console | OK | `ttyS0,921600n1` (used for debugging) |
| Pstore/ramoops | OK | console log at 0x4d010000 size 0xe0000 |
| Clock / pinctrl | OK | vendor MT6781 drivers |
| OpenRC userspace | OK | PID1 `init [3]` |

## Not working / not enabled

| Subsystem | Reason |
|-----------|--------|
| Mali GPU acceleration | Proprietary `mali`/TEEI kernel sources missing; upstream `panfrost` does not support this Mali revision on this SoC |
| Hardware video decode | Vendor `omx`/`vdec` firmware + HALs not ported |
| Wi-Fi / Bluetooth | `mt76`/`mt6625` firmware and HALs not installed; no `wpa_supplicant` setup |
| Camera | ISP firmware + HALs not ported |
| Fingerprint | Goodix/Egis vendor HAL not ported |
| Cellular modem | RIL/vendor daemon not ported |
| Audio | ALSA UCM + vendor HALs not ported |
| Battery/charging UI | Kernel fuel gauge works, no userspace desktop |
| Framebuffer console (`fbcon`) | Vendor kernel's simplefb/fbcon path produced black screen; replaced with manual DRM console |

## Display specifics

- Panel: 1080×2400, 60 Hz; mode timings synthesized from `/dev/fb0` var info
  (pixel clock 322629 kHz, hsync 1100/1110/1120, vsync 2410/2415/2420).
- The `gentty` TTY uses the **XRGB8888** scanout buffer directly. Text is drawn with a
  Terminus 8×16 PSF2 font converted to `font8x16.h`.
- Legacy DRM `SETCRTC` succeeds only with the correct CRTC/connector (typically CRTC 63,
  connector 28). Atomic commits return `EACCES` because `03-drmterm.start` historically
  held DRM master — it was removed; `04-control.start` creates the node fresh.

## Why no custom kernel (yet)

The OSS 4.14 vendor kernel boots to the POCO logo then black: the display pipeline needs
proprietary Mali/TEEI sources. Using the stock kernel plus a custom initramfs avoided this.
A genuine Linux 6.18 LTS port is in progress (see `KERNEL-6.18.md`) but mainline has no
MT6781 drivers, so it cannot yet drive storage/display/USB.

## Chipset identification from DTB

- `compatible = "mediatek,MT6781"`
- UFS controller: `mediatek,ufshci` at `0x11270000`
- Pinctrl: `mediatek,mt6781-pinctrl`
- USB: `mediatek,mt6781-usb20` at `0x11200000`, UDC `musb-hdrc`

## Partition map (stock, 4096-byte sectors)

```
 34  boot_a      64 MB
 35  vendor_boot_a 64 MB
 36  dtbo_a       8 MB
 ...
 58  super        8.5 GB
 59  userdata     107 GB   (f2fs; holds /data/gentoo.img)
 60  flashinfo    16 MB
```

Do **not** modify the GPT casually: `sgdisk` on this device reports a misleading
"main partition table overlaps first partition by 26 blocks" because the GPT header uses
512-byte-sector semantics while the disk reports 4096. The proven setup leaves the GPT
untouched.
