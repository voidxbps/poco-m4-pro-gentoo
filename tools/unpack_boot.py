#!/usr/bin/env python3
import struct, sys, os

def unpack(path, outdir):
    d = open(path, 'rb').read()
    magic = d[:8]
    assert magic == b'ANDROID!', magic
    (ksize, kaddr, rsize, raddr, ssize, saddr, tags, page,
     dtsize, u0, u1, hver) = struct.unpack_from('<10I4I', d, 8)[:12]
    name = d[56:72].rstrip(b'\0')
    cmdline = d[72:584].rstrip(b'\0')
    os.makedirs(outdir, exist_ok=True)
    print(f"header_version={hver} page={page}")
    print(f"kernel: size={ksize} addr=0x{kaddr:x}")
    print(f"ramdisk: size={rsize} addr=0x{raddr:x}")
    print(f"second: size={ssize} addr=0x{saddr:x}")
    print(f"dt: size={dtsize} tags=0x{tags:x}")
    print(f"name={name!r}")
    print(f"cmdline={cmdline!r}")
    p = page
    for label, size in (('kernel', ksize), ('ramdisk', rsize), ('second', ssize), ('dt', dtsize)):
        if size:
            data = d[p:p+size]
            open(os.path.join(outdir, label), 'wb').write(data)
            print(f"wrote {label} ({size} bytes) at 0x{p:x}")
            p += (size + page - 1) // page * page
    return dict(page=page, kaddr=kaddr, raddr=raddr, saddr=saddr,
                tags=tags, hver=hver, cmdline=cmdline.decode(errors='replace'), name=name.decode(errors='replace'))

if __name__ == '__main__':
    unpack(sys.argv[1], sys.argv[2])
