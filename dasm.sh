#!/bin/sh
rm -f dump.asm
rm -f cpustart.asm
objdump -d -M intel build/kernel64.elf >dump.asm
objdump -D -b binary -m i8086 -M intel build/cpustart.bin >cpustart.asm

