#!/bin/sh
touch os1.log
qemu-system-x86_64 -smp 4 -drive format=raw,file=os1.raw -serial file:os1.log -d cpu_reset -no-reboot #-d int #-no-shutdown  #-m 4G

