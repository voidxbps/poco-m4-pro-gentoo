#!/usr/bin/env python3
"""Patch the ORIGINAL TWRP ramdisk cpio in place:
  - replace the contents of init.recovery.usb.rc with a same-size version
    that starts a Gentoo chroot service (keeps ADB intact)
  - append /gentoo-init.sh as a new entry before TRAILER!!!
All original entries stay byte-identical apart from those two changes.
"""
import gzip, struct, sys

def read_fields(hdr):
    return [int(hdr[6+8*k:14+8*k], 16) for k in range(13)]

def write_fields(f):
    return b'070701' + b''.join(f'{v:08X}'.encode() for v in f)

def parse(data):
    entries = []
    i = 0
    while i + 110 <= len(data):
        if data[i:i+6] != b'070701':
            raise ValueError(f'bad magic at {i}')
        hdr = data[i:i+110]
        f = read_fields(hdr)
        namesize = f[11]
        if namesize > 4096:
            print('BAD namesize', namesize, 'at', i)
            break
        name = data[i+110:i+110+namesize-1].decode(errors='replace')
        st = (i + 110 + namesize + 3) & ~3
        size = f[6]
        payload = data[st:st+size]
        i = st + ((size + 3) & ~3)
        entries.append(dict(hdr=hdr, fields=f, name=name, data=payload))
        if name == 'TRAILER!!!':
            break
    return entries, i

def make_entry(name, body, mode=0o100755):
    f = [0x50000, mode, 0, 0, 1, 0, len(body), 0, 0, 0, 0, len(name)+1, 0]
    hdr = write_fields(f)
    out = hdr + name.encode() + b'\0'
    out += b'\0' * ((-len(out)) % 4)
    out += body
    out += b'\0' * ((-len(out)) % 4)
    return out

def main():
    raw = gzip.open('boot_a/ramdisk', 'rb').read()
    entries, end = parse(raw)
    print('entries:', len(entries), 'end:', end, 'total:', len(raw))

    init_script = open('init.sh', 'rb').read()

    # Locate the rc file we will overwrite in place.
    rc = next(e for e in entries if e['name'] == 'init.recovery.mt6781.rc')
    old = rc['data']
    print('rc size:', len(old))

    service = (
        b'service gentoo /system/bin/sh /gentoo-init.sh\n'
        b'    class main\n'
        b'    user root\n'
        b'    group root\n'
        b'    oneshot\n'
        b'    seclabel u:r:recovery:s0\n'
        b'\n'
        b'on boot\n'
        b'    exec u:r:recovery:s0 root root -- /system/bin/sh -c "echo rcfired > /data/rcfired"\n'
        b'    start gentoo\n'
        b'\n'
    )
    assert len(service) < len(old), (len(service), len(old))
    new_rc = service + (b'#' * (len(old) - len(service)))
    assert len(new_rc) == len(old)
    rc['data'] = new_rc

    # Insert new script entry just before TRAILER!!!.
    idx = next(i for i, e in enumerate(entries) if e['name'] == 'TRAILER!!!')
    entries.insert(idx, dict(hdr=None, name='/gentoo-init.sh', data=init_script, new=True))

    # Rebuild sequentially using stored name bytes from original.
    out = bytearray()
    cursor = 0
    for e in entries:
        if e.get('new'):
            out += make_entry(e['name'].lstrip('/'), e['data'])
            continue
        # original name bytes live right after its header
        name_off = cursor + 110
        namesize = e['fields'][11]
        namebytes = raw[name_off:name_off+namesize]
        f = list(e['fields'])
        f[6] = len(e['data'])
        hdr = write_fields(f)
        out += hdr + namebytes
        out += b'\0' * ((-len(out)) % 4)
        out += e['data']
        out += b'\0' * ((-len(out)) % 4)
        cursor += 110 + namesize
        cursor = (cursor + 3) & ~3
        cursor += ((e['fields'][6] + 3) & ~3)

    open('ramdisk-patched.cpio', 'wb').write(out)
    with gzip.open('ramdisk-patched.cpio.gz', 'wb') as g:
        g.write(out)
    print('wrote ramdisk-patched.cpio.gz', len(out))

if __name__ == '__main__':
    main()
