.PHONY: default
default: all ;

INCLUDE=../libc
CFLAGS=-I$(INCLUDE) -std=gnu99 -ffreestanding -O1 -Wall -Wextra -fno-asynchronous-unwind-tables
CPPFLAGS=-I$(INCLUDE) -std=c++14 -ffreestanding -O0 -Wall -Wextra -fno-asynchronous-unwind-tables
LDSCRIPT=linker2.ld

all: kernel64.elf cpustart.bin

cpustart.bin: cpustart.asm
	$(ASM) -fbin cpustart.asm -o $(BUILD_DIR)cpustart.bin

kernel64.elf: kernel.o memory.o terminal.o interrupt.o irqhandler.o inthandler.o multitask.o ihandler.o stdlib.o task.o keyboard.o pageframe.o virtualmemory.o debug.o cpu.o mp.o lapic.o ioapic.o pic.o string.o cpustart.o
	$(LD) -T $(LDSCRIPT) \
	$(BUILD_DIR)kernel.o \
	$(BUILD_DIR)memory.o \
	$(BUILD_DIR)terminal.o \
	$(BUILD_DIR)interrupt.o \
	$(BUILD_DIR)irqhandler.o \
	$(BUILD_DIR)inthandler.o \
	$(BUILD_DIR)multitask.o \
	$(BUILD_DIR)ihandler.o \
	$(BUILD_DIR)stdlib.o \
	$(BUILD_DIR)task.o \
	$(BUILD_DIR)keyboard.o \
	$(BUILD_DIR)pageframe.o \
	$(BUILD_DIR)virtualmemory.o \
	$(BUILD_DIR)debug.o \
	$(BUILD_DIR)cpu.o \
	$(BUILD_DIR)mp.o \
	$(BUILD_DIR)lapic.o \
	$(BUILD_DIR)ioapic.o \
	$(BUILD_DIR)pic.o \
	$(BUILD_DIR)string.o \
	$(BUILD_DIR)cpustart.o \
	-o $(BUILD_DIR)$@ -n

memory.o: memory.asm
	$(ASM) -felf64 $^ -o $(BUILD_DIR)$@
	
kernel.o: kernel.cpp
	$(CPP) -c $^ -o $(BUILD_DIR)$@ $(CPPFLAGS)

terminal.o: terminal.cpp
	$(CPP) -c $^ -o $(BUILD_DIR)$@ $(CPPFLAGS)

interrupt.o: interrupt.cpp
	$(CPP) -c $^ -o $(BUILD_DIR)$@ $(CPPFLAGS)

ihandler.o: ihandler.c
	$(CC) -c $^ -o $(BUILD_DIR)$@ $(CFLAGS)

stdlib.o: ../libc/stdlib.cpp
	$(CPP) -c $^ -o $(BUILD_DIR)$@ $(CPPFLAGS)

irqhandler.o: irqhandler.asm
	$(ASM) -felf64 $^ -o $(BUILD_DIR)$@

inthandler.o: inthandler.asm
	$(ASM) -felf64 $^ -o $(BUILD_DIR)$@

multitask.o: multitask.asm
	$(ASM) -felf64 $^ -o $(BUILD_DIR)$@

task.o: task.cpp
	$(CPP) -c $^ -o $(BUILD_DIR)$@ $(CPPFLAGS)

keyboard.o: keyboard.cpp
	$(CPP) -c $^ -o $(BUILD_DIR)$@ $(CPPFLAGS)

pageframe.o: pageframe.cpp
	$(CPP) -c $^ -o $(BUILD_DIR)$@ $(CPPFLAGS)

virtualmemory.o: virtualmemory.cpp
	$(CPP) -c $^ -o $(BUILD_DIR)$@ $(CPPFLAGS)

debug.o: debug.cpp
	$(CPP) -c $^ -o $(BUILD_DIR)$@ $(CPPFLAGS)

cpu.o: cpu.cpp
	$(CPP) -c $^ -o $(BUILD_DIR)$@ $(CPPFLAGS)

mp.o: mp.cpp
	$(CPP) -c $^ -o $(BUILD_DIR)$@ $(CPPFLAGS)

lapic.o: lapic.cpp
	$(CPP) -c $^ -o $(BUILD_DIR)$@ $(CPPFLAGS)

ioapic.o: ioapic.cpp
	$(CPP) -c $^ -o $(BUILD_DIR)$@ $(CPPFLAGS)

pic.o: pic.cpp
	$(CPP) -c $^ -o $(BUILD_DIR)$@ $(CPPFLAGS)

string.o: string.cpp
	$(CPP) -c $^ -o $(BUILD_DIR)$@ $(CPPFLAGS)

cpustart.o: cpustart.asm
	$(ASM) -felf64 $^ -o $(BUILD_DIR)$@

clean:
	rm -f $(BUILD_DIR)kernel64.elf $(BUILD_DIR)*.o $(BUILD_DIR)cpustart.bin
	
