#!/bin/sh
exec >/var/log/usb.log 2>&1
echo START
echo 1 > /sys/module/musb_hdrc/parameters/kernel_init_done 2>&1
echo "kid=$?"
echo 0 > /sys/devices/platform/mt_usb/saving 2>&1
mount -t configfs none /config
echo "mountcfg=$?"
mkdir -p /config/usb_gadget/g1/strings/0x409
mkdir -p /config/usb_gadget/g1/configs/b.1/strings/0x409
mkdir -p /config/usb_gadget/g1/functions/ffs.adb
echo 0 > /config/usb_gadget/g1/bDeviceClass
echo 0 > /config/usb_gadget/g1/bDeviceSubClass
echo 0 > /config/usb_gadget/g1/bDeviceProtocol
echo 0x0223 > /config/usb_gadget/g1/bcdDevice
echo 0x0200 > /config/usb_gadget/g1/bcdUSB
echo FMIZIRT4YP5TTORG > /config/usb_gadget/g1/strings/0x409/serialnumber
echo xiaomi > /config/usb_gadget/g1/strings/0x409/manufacturer
echo fleur > /config/usb_gadget/g1/strings/0x409/product
echo 1 > /config/usb_gadget/g1/os_desc/b_vendor_code
echo MSFT100 > /config/usb_gadget/g1/os_desc/qw_sign
ln -sf /config/usb_gadget/g1/configs/b.1 /config/usb_gadget/g1/os_desc/b.1
echo 1 > /config/usb_gadget/g1/os_desc/use
echo "osdesc=$?"
mkdir -p /dev/usb-ffs/adb
mount -t functionfs adb /dev/usb-ffs/adb -o rmode=0770,fmode=0660,uid=2000,gid=2000,no_disconnect=1
echo "ffs=$?"
echo adb > /sys/class/android_usb/android0/f_ffs/aliases 2>/dev/null
echo adb > /config/usb_gadget/g1/configs/b.1/strings/0x409/configuration
echo 0x18d1 > /config/usb_gadget/g1/idVendor
echo 0xd001 > /config/usb_gadget/g1/idProduct
ln -sf /config/usb_gadget/g1/functions/ffs.adb /config/usb_gadget/g1/configs/b.1/f1
sleep 5
echo "state=$(cat /sys/class/udc/musb-hdrc/state 2>&1)"
echo "func=$(cat /sys/class/udc/musb-hdrc/function 2>&1)"
echo 1 > /sys/class/udc/musb-hdrc/soft_connect 2>&1
echo "soft=$?"
for G in /config/usb_gadget/*; do [ -e "$G/UDC" ] && echo "" > "$G/UDC" 2>/dev/null; done
echo "unbound=$(cat /config/usb_gadget/g1/UDC 2>&1)"
echo "cmode=$(cat /sys/devices/platform/mt_usb/musb-hdrc/cmode 2>&1)"
echo 1 > /sys/devices/platform/mt_usb/musb-hdrc/cmode 2>&1
echo "cmode-set=$?"
for N in $(seq 1 10); do
  (echo musb-hdrc > /config/usb_gadget/g1/UDC) 2>/dev/null
  R=$?
  echo "try $N rc=$R"
  if [ -n "$(cat /config/usb_gadget/g1/UDC 2>/dev/null)" ]; then break; fi
  sleep 2
done
echo "UDC=$(cat /config/usb_gadget/g1/UDC 2>&1)"
ls -l /sys/class/udc/musb-hdrc/ 2>&1
cat /sys/class/udc/musb-hdrc/uevent 2>&1
echo DONE
