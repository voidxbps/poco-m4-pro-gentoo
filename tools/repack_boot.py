#!/usr/bin/env python3
"""Repack a MediaTek header-v0 boot.img while preserving its exact layout.

The stock image has a DTB and other data after the ramdisk. We therefore:
  * keep the original header verbatim (MTK stores extra data in 'unused')
  * keep kernel bytes and kernel_size unchanged
  * pad the new ramdisk to the ORIGINAL ramdisk size so every later offset
    stays identical
  * copy all bytes after the ramdisk verbatim from the original image
"""
import struct, sys, os

def align(x, page):
    return (x + page - 1) // page * page

def repack(orig, kernel, ramdisk, out, cmdline=None):
    d = open(orig, 'rb').read()
    assert d[:8] == b'ANDROID!'
    ksize, kaddr, rsize, raddr, ssize, saddr, tags, page = struct.unpack_from('<8I', d, 8)
    oldcmd = d[64:576].rstrip(b'\0')
    if cmdline is None:
        cmdline = oldcmd
    if isinstance(cmdline, str):
        cmdline = cmdline.encode()
    kern = open(kernel, 'rb').read()
    ram = open(ramdisk, 'rb').read()
    assert len(kern) == ksize, (len(kern), ksize)
    assert len(ram) <= rsize, (len(ram), rsize)

    hdr = bytearray(d[:page])
    if cmdline != oldcmd:
        hdr[64:576] = b'\0' * 512
        hdr[64:64+len(cmdline)] = cmdline

    ko = page
    ro = ko + align(ksize, page)
    tail = d[ro + rsize:]

    newram = ram + b'\0' * (rsize - len(ram))
    img = bytearray(hdr) + kern + b'\0' * (align(ksize, page) - ksize)
    img += newram + tail
    assert len(img) == len(d), (len(img), len(d))
    open(out, 'wb').write(img)
    print(f"ramdisk {len(ram)} -> padded {rsize}; tail {len(tail)} bytes preserved")
    print(f"cmd old={oldcmd!r} new={cmdline!r}  -> {out} ({len(img)} bytes)")

if __name__ == '__main__':
    repack(*sys.argv[1:])
