.PHONY: default
default: all ;

ASM=nasm
CC=~/opt/cross/bin/x86_64-elf-gcc
CPP=~/opt/cross/bin/x86_64-elf-g++
CFLAGS=-std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-asynchronous-unwind-tables
CPPFLAGS=-std=c++14 -ffreestanding -O2 -Wall -Wextra -fno-asynchronous-unwind-tables
LD=~/opt/cross/bin/x86_64-elf-ld
LINK_SCRIPT=linker.ld

all: boot.bin kernel16.bin kernel64.elf
	hd boot.bin
	objdump -h kernel64.elf
	dd if=boot.bin of=os1.raw bs=512 count=1 conv=notrunc conv=sync
	dd if=kernel16.bin of=os1.raw seek=1 count=8 conv=notrunc conv=sync
	dd if=kernel64.elf of=os1.raw bs=512 seek=8 conv=notrunc conv=sync

kernel64.elf: kernel64.o memory.o stdlib.o terminal.o
	$(LD) -T $(LINK_SCRIPT) $^ -o $@ -n
	#--strip-all -n
#	$(CC) $(CFLAGS) -z max-page-size=0x1000 -T $(LINK_SCRIPT) kernel64.o -o kernel64.elf -nostdlib -lgcc
	
boot.bin: boot.asm
	$(ASM) -fbin $^ -o $@
	
memory.o: memory.asm
	$(ASM) -felf64 $^ -o $@
	
#kernel64.o: kernel64.c
#	$(CC) -c $^ -o $@ $(CFLAGS)
	
kernel64.o: kernel.cpp
	$(CPP) -c $^ -o $@ $(CPPFLAGS)

terminal.o: terminal.cpp
	$(CPP) -c $^ -o $@ $(CPPFLAGS)

stdlib.o: stdlib.c
	$(CC) -c $^ -o $@ $(CFLAGS)

kernel16.bin: kernel16.asm biosmemory.asm console16.asm console64.asm
	$(ASM) $(ASMFLAGS) kernel16.asm -o $@

clean:
	rm -f *.o *.bin *.elf

