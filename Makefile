.PHONY: default
default: all ;

ASM=nasm
CC=x86_64-elf-gcc
CPP=x86_64-elf-g++
LD=x86_64-elf-ld
BUILD_DIR=$(PWD)/build/
export

prepare:
	mkdir -p $(BUILD_DIR)

all: prepare
	+$(MAKE) $@ -C src/boot
	+$(MAKE) $@ -C src/kernel
	#hd $(BUILD_DIR)boot.bin
	#readelf -e readelf -e $(BUILD_DIR)kernel64.elf
	dd if=/dev/zero of=os1.raw bs=1048576 count=1
	dd if=$(BUILD_DIR)boot.bin of=os1.raw bs=512 count=1 conv=notrunc conv=sync
	dd if=$(BUILD_DIR)kernel16.bin of=os1.raw seek=1 count=8 conv=notrunc conv=sync
	dd if=$(BUILD_DIR)kernel64.elf of=os1.raw bs=512 seek=9 count=256 conv=notrunc conv=sync

clean:
	+$(MAKE) $@ -C src/boot
	+$(MAKE) $@ -C src/kernel
	rm -df $(BUILD_DIR)
	rm -f os1.raw

