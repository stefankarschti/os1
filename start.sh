#!/bin/sh
touch os1.log
qemu-system-x86_64 -drive format=raw,file=os1.raw -serial file:os1.log -d cpu_reset -no-shutdown -no-reboot #-d int #-m 4G

