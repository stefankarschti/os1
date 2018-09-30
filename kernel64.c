#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
 
/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif
 
/* This will only work for the 64-bit ix86 targets. */
#if !defined(__x86_64__)
#error "This tutorial needs to be compiled with a x86_64-elf compiler"
#endif

#pragma pack(1)
struct memory_block
{
	uint64_t start;
	uint64_t length;
	uint32_t type;
	uint32_t unused;
};
struct system_info
{
	uint8_t cursorx;
	uint8_t cursory;
	uint16_t num_memory_blocks;
	struct memory_block* memory_blocks;
};
#pragma pack()

/* Hardware text mode color constants. */
enum vga_color {
	VGA_COLOR_BLACK = 0,
	VGA_COLOR_BLUE = 1,
	VGA_COLOR_GREEN = 2,
	VGA_COLOR_CYAN = 3,
	VGA_COLOR_RED = 4,
	VGA_COLOR_MAGENTA = 5,
	VGA_COLOR_BROWN = 6,
	VGA_COLOR_LIGHT_GREY = 7,
	VGA_COLOR_DARK_GREY = 8,
	VGA_COLOR_LIGHT_BLUE = 9,
	VGA_COLOR_LIGHT_GREEN = 10,
	VGA_COLOR_LIGHT_CYAN = 11,
	VGA_COLOR_LIGHT_RED = 12,
	VGA_COLOR_LIGHT_MAGENTA = 13,
	VGA_COLOR_LIGHT_BROWN = 14,
	VGA_COLOR_WHITE = 15,
};
 
static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) 
{
	return fg | bg << 4;
}
 
static inline uint16_t vga_entry(unsigned char uc, uint8_t color) 
{
	return (uint16_t) uc | (uint16_t) color << 8;
}
 
size_t strlen(const char* str) 
{
	size_t len = 0;
	while (str[len])
		len++;
	return len;
}
 
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
 
size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer;
 
void outb( unsigned short port, unsigned char val )
{
   asm volatile("outb %0, %1" : : "a"(val), "Nd"(port) );
}

void memset(char *ptr, char value, int len);
void memcpy(char *psrc, char *pdest, int len);
/*
void memset(char *ptr, char value, int len)
{
	char *dummy_ptr;
	int dummy_len;

	asm __volatile__ (
		"cld	\n\t"
		"rep	\n\t"
		"stosb	\n\t"
		: "=c" (dummy_len), "=D" (dummy_ptr)
		: "0" (len), "a" (value), "1" (ptr)
	);
}

void memcpy(char *psrc, char *pdest, int len)
{
	char *dummy_ptr;
	int dummy_len;

	asm __volatile__ (
		"cld	\n\t"
		"rep	\n\t"
		"movsb	\n\t"
		: "=c" (dummy_len), "=D" (dummy_ptr)
		: "0" (len), "a" (pdest), "1" (psrc)
	);
}
*/

void update_cursor(int x, int y)
{
	uint16_t pos = y * VGA_WIDTH + x;
 
	outb(0x3D4, 0x0F);
	outb(0x3D5, (uint8_t) (pos & 0xFF));
	outb(0x3D4, 0x0E);
	outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

void terminal_initialize(void) 
{
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	terminal_buffer = (uint16_t*) 0xB8000;
/*	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry(' ', terminal_color);
		}
	}*/
}
 
void terminal_setcolor(uint8_t color) 
{
	terminal_color = color;
}
 
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) 
{
	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = vga_entry(c, color);
}
 
void terminal_putchar(char c) 
{
	terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
	if (++terminal_column == VGA_WIDTH) {
		terminal_column = 0;
		if (++terminal_row == VGA_HEIGHT)
		{
			// TODO: scroll up
//			memcpy(terminal_buffer + VGA_WIDTH, terminal_buffer, sizeof(terminal_buffer[0]) * (VGA_HEIGHT - 1) * VGA_WIDTH);
			memset(terminal_buffer + VGA_WIDTH * (VGA_HEIGHT - 1), vga_entry(' ', terminal_color), sizeof(terminal_buffer[0]) * VGA_WIDTH);
			terminal_row = VGA_HEIGHT - 1;
		}
	}
}
 
void terminal_write(const char* data)
{
	while(*data)
	{
		terminal_putchar(*data);
		data++;
	}
	update_cursor(terminal_column, terminal_row);
}
 
char * itoa( int value, char * str, int base )
{
    char * rc;
    char * ptr;
    char * low;
    // Check for supported base.
    if ( base < 2 || base > 36 )
    {
        *str = '\0';
        return str;
    }
    rc = ptr = str;
    // Set '-' for negative decimals.
    if ( value < 0 && base == 10 )
    {
        *ptr++ = '-';
    }
    // Remember where the numbers start.
    low = ptr;
    // The actual conversion.
    do
    {
        // Modulo is negative for negative value. This trick makes abs() unnecessary.
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz"[35 + value % base];
        value /= base;
    } while ( value );
    // Terminating the string.
    *ptr-- = '\0';
    // Invert the numbers.
    while ( low < ptr )
    {
        char tmp = *low;
        *low++ = *ptr;
        *ptr-- = tmp;
    }
    return rc;
}

void kernel_main() 
{
	// (ugly) pointer to global list of system parameters
	// TODO:
	struct system_info* pinfo = (struct system_info*)0x4000;
	
	/* Initialize terminal interface */
	terminal_initialize();
 	terminal_row = pinfo->cursory;
 	terminal_column = pinfo->cursorx;
	update_cursor(terminal_column, terminal_row);
 	
	/* Newline support is left as an exercise. */
	terminal_write("[elf_kernel64] hello\n");

	uint64_t total_mem = 0;
	for(int i = 0; i < pinfo->num_memory_blocks; ++i)
	{
		if(1 == pinfo->memory_blocks[i].type)
		{
			total_mem += pinfo->memory_blocks[i].length;
		}
	}
	total_mem >>= 20;
	char temp[16];
	itoa((int)(total_mem), temp, 10);
	terminal_write("[elf_kernel64] ");
	terminal_write(temp);
	terminal_write(" MB RAM detected");
}

