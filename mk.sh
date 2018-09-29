#!/bin/sh

#as -o boot.o boot.s
#ld -Ttext 0x0000 -o boot.elf boot.o
#objcopy -O binary boot.elf boot.bin

echo 'boot.asm'
nasm boot.asm -f bin -o boot.bin
#objdump -D -b binary -m i8086 -M intel boot.bin
hd boot.bin
dd if=boot.bin of=os1.raw bs=512 conv=notrunc

echo 'kernel.asm'
nasm kernel.asm -f bin -o kernel.bin
hd kernel.bin
#objdump -D -b binary -m i386:x86-64 -M intel kernel.bin
#objdump -D -b binary -m i8086 -M intel kernel.bin
dd if=kernel.bin of=os1.raw seek=1 conv=notrunc

