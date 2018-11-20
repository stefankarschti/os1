#!/bin/sh
rm -f dump.asm
objdump -d -M intel build/kernel64.elf >dump.asm

