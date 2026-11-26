#!/system/bin/sh
set -e
R=/mnt/gentoo
HASH='$6$gentoosalta$fGjtoKXIa5mp1j4vhiHHpsFfhv.E1vyiAnyFp/l7r/bzJc94ZfaXGfuCt2J2bZvcNho2wy7QiO.srdcDug590/'
{ echo "root:$HASH:19722:0:99999:7:::"; tail -n +2 $R/etc/shadow; } > $R/etc/shadow.new
mv $R/etc/shadow.new $R/etc/shadow
chmod 640 $R/etc/shadow
chown 0:0 $R/etc/shadow

echo '# Managed by native-boot setup; filesystems are mounted by the initramfs.' > $R/etc/fstab
echo 'pocogentoo' > $R/etc/hostname
mkdir -p $R/etc/local.d
echo '#!/bin/sh' > $R/etc/local.d/99-bootmark.start
echo 'echo " gentoo userspace up $(date)" >> /gentoo-root-up' >> $R/etc/local.d/99-bootmark.start
chmod 755 $R/etc/local.d/99-bootmark.start

echo "=== verify ==="
ls -l $R/sbin/init $R/system/bin/adbd $R/system/bin/linker64
head -1 $R/etc/shadow
cat $R/etc/fstab
du -sh $R
echo "=== FINISH DONE ==="
