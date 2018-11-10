.PHONY: default
default: all ;

INCLUDE=../libc
CFLAGS=-I$(INCLUDE) -std=gnu99 -ffreestanding -O -Wall -Wextra -fno-asynchronous-unwind-tables
CPPFLAGS=-I$(INCLUDE) -std=c++14 -ffreestanding -O0 -Wall -Wextra -fno-asynchronous-unwind-tables
LDSCRIPT=linker2.ld

all: kernel64.elf

kernel64.elf: kernel.o memory.o terminal.o interrupt.o irqhandler.o ihandler.o stdlib.o task.o keyboard.o pageframe.o virtualmemory.o debug.o
	$(LD) -T $(LDSCRIPT) \
	$(BUILD_DIR)kernel.o \
	$(BUILD_DIR)memory.o \
	$(BUILD_DIR)terminal.o \
	$(BUILD_DIR)interrupt.o \
	$(BUILD_DIR)irqhandler.o \
	$(BUILD_DIR)ihandler.o \
	$(BUILD_DIR)stdlib.o \
	$(BUILD_DIR)task.o \
	$(BUILD_DIR)keyboard.o \
	$(BUILD_DIR)pageframe.o \
	$(BUILD_DIR)virtualmemory.o \
	$(BUILD_DIR)debug.o \
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

clean:
	rm -f $(BUILD_DIR)kernel64.elf $(BUILD_DIR)*.o
	
