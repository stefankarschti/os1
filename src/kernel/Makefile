.PHONY: default
default: all ;

CFLAGS=-std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-asynchronous-unwind-tables
CPPFLAGS=-std=c++14 -ffreestanding -O2 -Wall -Wextra -fno-asynchronous-unwind-tables
LDSCRIPT=linker.ld

all: kernel64.elf

kernel64.elf: kernel.o memory.o terminal.o
	$(LD) -T $(LDSCRIPT) $(BUILD_DIR)kernel.o $(BUILD_DIR)memory.o $(BUILD_DIR)terminal.o -o $(BUILD_DIR)$@ -n	#--strip-all

memory.o: memory.asm
	$(ASM) -felf64 $^ -o $(BUILD_DIR)$@
	
kernel.o: kernel.cpp
	$(CPP) -c $^ -o $(BUILD_DIR)$@ $(CPPFLAGS)

terminal.o: terminal.cpp
	$(CPP) -c $^ -o $(BUILD_DIR)$@ $(CPPFLAGS)

clean:
	rm -f $(BUILD_DIR)kernel64.elf $(BUILD_DIR)*.o
	
