# Prebuilt artifacts (split for GitHub)

GitHub rejects single files larger than 100 MB, so the large artifacts are split into
40 MB pieces. Reassemble with `cat`.

## Prebuilt Gentoo rootfs

`rootfs.tar.gz.partaa` … `rootfs.tar.gz.partag` — a gzip tarball of a complete Gentoo
arm64 OpenRC rootfs as installed on the phone (948 MB uncompressed). It already contains:

- the Gentoo base system from an arm64 OpenRC stage3
- TWRP's `/system` runtime (`adbd`, `linker64`, libs) for ADB
- the `etc/local.d/` USB/ADB/control scripts
- `/sbin/gentty`, `/sbin/rebootboot`
- root password `gentoo`, hostname `pocogentoo`

```bash
cd rootfs-parts
cat rootfs.tar.gz.part* > /tmp/rootfs-live.tar.gz
# Verify (optional)
gzip -t /tmp/rootfs-live.tar.gz && echo OK
# Extract into the mounted rootfs
adb shell 'mount -t ext4 /dev/block/loop0 /mnt/gentoo'
adb push /tmp/rootfs-live.tar.gz /data/rootfs-live.tar.gz
adb shell 'tar xpf /data/rootfs-live.tar.gz -C /mnt/gentoo'
```

## Gentoo stage3 (ARM64 OpenRC)

`stage3-arm64-openrc.tar.xz.partaa` … — the original stage3 used to build the rootfs
(237 MB compressed). Only needed if you want to rebuild the rootfs from scratch rather
than use the prebuilt tarball.

```bash
cd rootfs-parts
cat stage3-arm64-openrc.tar.xz.part* > /tmp/stage3-arm64-openrc.tar.xz
```

Always prefer a **current** stage3 from `https://www.gentoo.org/downloads/` for a fresh
install; the bundled one is provided for reproducibility.

## Notes

- Use `cat part* > file` (shell glob order is alphabetical, which matches `aa`, `ab`, …).
- Do not use `cat part??` if you later add unrelated files with the same prefix.
- After extraction, verify `/mnt/gentoo/sbin/init` exists and `etc/local.d/*.start` are
  executable (see `INSTALL.md` step 4).
