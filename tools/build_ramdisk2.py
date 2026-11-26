#!/usr/bin/env python3
"""Patch the original TWRP ramdisk cpio:
  - add /gentoo-init2.sh (our init script)
  - repoint the /init symlink from /system/bin/init to /gentoo-init2.sh
  - leave every other entry byte-identical
"""
import gzip, sys

def read_fields(hdr):
    return [int(hdr[6+8*k:14+8*k], 16) for k in range(13)]

def write_fields(f):
    return b'070701' + b''.join(f'{v:08X}'.encode() for v in f)

def parse(data):
    entries = []
    i = 0
    while i + 110 <= len(data):
        assert data[i:i+6] == b'070701', f'bad magic at {i}'
        f = read_fields(data[i:i+110])
        namesize = f[11]
        namebytes = data[i+110:i+110+namesize]
        name = namebytes[:-1].decode(errors='replace')
        st = (i + 110 + namesize + 3) & ~3
        payload = data[st:st+f[6]]
        i = st + ((f[6] + 3) & ~3)
        entries.append(dict(f=f, namebytes=namebytes, name=name, data=payload))
        if name == 'TRAILER!!!':
            break
    return entries

def make_entry(name, body, mode=0o100755):
    f = [0x50000, mode, 0, 0, 1, 0, len(body), 0, 0, 0, 0, len(name)+1, 0]
    out = write_fields(f) + name.encode() + b'\0'
    out += b'\0' * ((-len(out)) % 4)
    out += body
    out += b'\0' * ((-len(out)) % 4)
    return out

def make_dev(name, mode, major, minor):
    f = [0x50001, mode, 0, 0, 1, 0, 0, 0, 0, major, minor, len(name)+1, 0]
    out = write_fields(f) + name.encode() + b'\0'
    out += b'\0' * ((-len(out)) % 4)
    return out

def main():
    raw = gzip.open('boot_a/ramdisk', 'rb').read()
    entries = parse(raw)
    print('entries', len(entries))

    script = open(sys.argv[1] if len(sys.argv) > 1 else 'init.sh', 'rb').read()
    target = b'/gentoo-init2.sh'
    assert len(target) == 16, len(target)

    if len(sys.argv) > 2 and sys.argv[2] == 'pidboot':
        bootsh = open('gentoo-boot.sh', 'rb').read()
        pidelf = open('pidinit', 'rb').read()
        for e in entries:
            if e['name'] == 'init.recovery.usb.rc':
                d = e['data']
                svc = (b'service gentoopid /system/bin/sh /gentoo-boot.sh\n'
                       b'    class main\n'
                       b'    user root\n'
                       b'    group root\n'
                       b'    oneshot\n'
                       b'    seclabel u:r:recovery:s0\n\n')
                d = svc + d
                key = b'    start adbd\n'
                pos = d.index(key) + len(key)
                d = d[:pos] + b'    start gentoopid\n' + d[pos:]
                e['data'] = d
                print('added async gentoopid service')
                break
        idx2 = next(i for i, e in enumerate(entries) if e['name'] == 'TRAILER!!!')
        entries.insert(idx2, dict(name='gentoo-boot.sh', data=bootsh, new=True))
        entries.insert(idx2 + 1, dict(name='pidinit', data=pidelf, new=True))
    elif len(sys.argv) > 2 and sys.argv[2] == 'usbboot2':
        bootsh = open('gentoo-boot.sh', 'rb').read()
        for e in entries:
            if e['name'] == 'init.recovery.usb.rc':
                key = b'on property:sys.usb.config=mtp,adb\n'
                d = e['data']
                pos = d.index(key) + len(key)
                e['data'] = d[:pos] + b'    exec u:r:recovery:s0 root root -- /system/bin/sh /gentoo-boot.sh\n' + d[pos:]
                print('added script boot action')
                break
        idx2 = next(i for i, e in enumerate(entries) if e['name'] == 'TRAILER!!!')
        entries.insert(idx2, dict(name='gentoo-boot.sh', data=bootsh, new=True))
    elif len(sys.argv) > 2 and sys.argv[2] == 'usbaction':
        for e in entries:
            if e['name'] == 'init.recovery.usb.rc':
                key = b'on property:sys.usb.config=mtp,adb\n'
                d = e['data']
                pos = d.index(key) + len(key)
                add = b'    exec u:r:recovery:s0 root root -- /system/bin/sh -c "/system/bin/chroot /system_root /bin/sh -c \'/sbin/openrc sysinit && /sbin/openrc boot && /sbin/openrc default\' >/system_root/openrc.log 2>&1 &"\n'
                e['data'] = d[:pos] + add + d[pos:]
                print('added Gentoo boot action to usb rc')
                break
        else:
            raise RuntimeError('no usb rc')
    elif len(sys.argv) > 2 and sys.argv[2] == 'recoverybin':
        for e in entries:
            if e['name'] == 'system/bin/recovery':
                e['data'] = script
                e['f'][1] = 0o100755
                print('replaced system/bin/recovery')
                break
        else:
            raise RuntimeError('no recovery entry')
    elif len(sys.argv) > 2 and sys.argv[2] == 'initbin':
        for e in entries:
            if e['name'] == 'init':
                e['data'] = script
                e['f'][1] = 0o100755
                print('replaced /init with static binary')
                break
        else:
            raise RuntimeError('no init entry')
    elif len(sys.argv) > 2 and sys.argv[2] == 'replaceinit':
        for e in entries:
            if e['name'] == 'system/bin/init':
                e['data'] = script
                print('replaced system/bin/init with script')
                break
        else:
            raise RuntimeError('no system/bin/init entry')
    # repoint /init symlink (data length must stay 16), unless told to keep it
    elif not (len(sys.argv) > 2 and sys.argv[2] == 'keepinit'):
        for e in entries:
            if e['name'] == 'init':
                assert len(e['data']) == 16, (e['name'], e['data'])
                e['data'] = target
                print('init ->', target)
                break
        else:
            raise RuntimeError('no init entry')

    # insert script and essential /dev nodes before TRAILER!!!
    idx = next(i for i, e in enumerate(entries) if e['name'] == 'TRAILER!!!')
    devices = [] if (len(sys.argv) > 3 and sys.argv[3] == 'nodev') else [
        ('dev/null', 0x21B6, 1, 3), ('dev/zero', 0x21B6, 1, 5),
        ('dev/console', 0x21B0, 5, 1), ('dev/tty', 0x21B0, 5, 0),
        ('dev/ptmx', 0x21B6, 5, 2), ('dev/random', 0x21B6, 1, 8),
        ('dev/urandom', 0x21B6, 1, 9), ('dev/kmsg', 0x21A0, 1, 11),
        ('dev/block/sdc1', 0x61B0, 8, 33),
        ('dev/block/sdc59', 0x61B0, 259, 43),
        ('dev/loop-control', 0x21B0, 10, 237),
    ]
    for i in range(8):
        devices.append((f'dev/block/loop{i}', 0x61B0, 7, i))
    newnodes = []
    for name, mode, maj, mnr in devices:
        newnodes.append(dict(name=name, dev=(mode, maj, mnr), new=True))
    entries[idx:idx] = newnodes
    entries.insert(idx + len(newnodes), dict(name='gentoo-init2.sh', data=script, new=True))

    out = bytearray()
    for e in entries:
        if e.get('new'):
            if 'dev' in e:
                out += make_dev(e['name'], *e['dev'])
            else:
                out += make_entry(e['name'], e['data'])
            continue
        f = list(e['f'])
        f[6] = len(e['data'])
        out += write_fields(f) + e['namebytes']
        out += b'\0' * ((-len(out)) % 4)
        out += e['data']
        out += b'\0' * ((-len(out)) % 4)

    open('ramdisk-patched.cpio', 'wb').write(out)
    with gzip.open('ramdisk-patched.cpio.gz', 'wb') as g:
        g.write(out)
    print('wrote', len(out))

if __name__ == '__main__':
    main()
