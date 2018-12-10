#!/bin/sh
touch os1.log
qemu-system-x86_64 -drive format=raw,file=os1.raw -serial file:os1.log -d cpu_reset #-d int #-no-shutdown -no-reboot #-m 4G

