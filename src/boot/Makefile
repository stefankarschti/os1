.PHONY: default
default: all ;

all: boot.bin kernel16.bin

boot.bin: *.asm
	$(ASM) -fbin boot.asm -o $(BUILD_DIR)boot.bin

kernel16.bin: *.asm
	$(ASM) -fbin kernel16.asm -o $(BUILD_DIR)kernel16.bin

clean:
	rm -f $(BUILD_DIR)boot.bin $(BUILD_DIR)kernel16.bin

