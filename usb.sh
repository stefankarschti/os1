#!/bin/sh
sudo dd if=os1.raw of=/dev/sdc conv=fdatasync
sudo qemu-system-x86_64 -usb -usbdevice host:058f:6387

