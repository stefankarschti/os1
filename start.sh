#!/bin/sh
touch os1.log
qemu-system-x86_64 -drive format=raw,file=os1.raw -serial file:os1.log #-m 4G

