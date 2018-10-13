#!/bin/sh

echo 'boot.asm'
nasm boot.asm -f bin -o boot.bin
#objdump -D -b binary -m i8086 -M intel boot.bin
hd boot.bin
dd if=boot.bin of=os1.raw bs=512 conv=notrunc

#echo 'kernel.asm'
#nasm kernel.asm -f bin -o kernel.bin
#hd kernel.bin
#dd if=kernel.bin of=os1.raw seek=1 conv=notrunc

echo "kernel.c"
~/opt/cross/bin/x86_64-elf-gcc -c kernel.c -o kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
echo "linking"
~/opt/cross/bin/x86_64-elf-gcc -T linker.ld -o myos.bin -ffreestanding -O2 -nostdlib kernel.o -lgcc

