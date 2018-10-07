#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
 
/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif
 
/* This will only work for the 64-bit ix86 targets. */
#if !defined(__x86_64__)
#error "This kernel needs to be compiled with a x86_64-elf compiler"
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

inline bool isprint(char c)
{
	return (c >= ' ');
}

static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) 
{
	return fg | bg << 4;
}
 
static inline uint16_t vga_entry(uint8_t uc, uint8_t color) 
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
size_t terminal_col;
uint8_t terminal_color;
uint16_t* terminal_buffer;
 
void outb( unsigned short port, unsigned char val )
{
   asm volatile("outb %0, %1" : : "a"(val), "Nd"(port) );
}

void memset(void *ptr, char value, uint64_t len);
void memsetw(void* ptr, uint16_t value, uint64_t num);
void memcpy(void *dest, void *src, uint64_t len);

void update_cursor(int x, int y)
{
	uint16_t pos = y * VGA_WIDTH + x;
 
	outb(0x3D4, 0x0F);
	outb(0x3D5, (uint8_t) (pos & 0xFF));
	outb(0x3D4, 0x0E);
	outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

void clrscr()
{
	memsetw(terminal_buffer, vga_entry(' ', terminal_color), 
		VGA_HEIGHT * VGA_WIDTH * sizeof(terminal_buffer[0]));
}

void terminal_initialize(void) 
{
	terminal_row = 0;
	terminal_col = 0;
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	terminal_buffer = (uint16_t*) 0xB8000;
	//clrscr();
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
 
void putchar(char c) 
{
	if('\n' == c)
	{
		terminal_row++;
		terminal_col = 0;
	}
	else /*if(isprint(c))*/
	{
		terminal_putentryat(c, terminal_color, terminal_col, terminal_row);
		terminal_col++;
		if(terminal_col == VGA_WIDTH)
		{
			terminal_row++;
			terminal_col = 0;
		}
	}

	if(terminal_row == VGA_HEIGHT)
	{
		// scroll up
		memcpy(terminal_buffer, terminal_buffer + VGA_WIDTH, 
			sizeof(terminal_buffer[0]) * (VGA_HEIGHT - 1) * VGA_WIDTH);
		memsetw(terminal_buffer + (VGA_HEIGHT - 1) * VGA_WIDTH,
			vga_entry(' ', vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK)),
			sizeof(terminal_buffer[0]) * VGA_WIDTH);
		terminal_row = VGA_HEIGHT - 1;
	}
}
 
// Writes the C string pointed by str to the standard output (stdout)
void puts(char* str)
{
	while(*str)
	{
		putchar(*str);
		str++;
	}
	update_cursor(terminal_col, terminal_row);
}
 
char* itoa(uint64_t value, char *str, int base)
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

struct system_info sysinfo;

void kernel_main(struct system_info* pinfo) 
{
	sysinfo = *pinfo;
	
	/* Initialize terminal interface */
	terminal_initialize();
 	terminal_row = pinfo->cursory;
 	terminal_col = pinfo->cursorx;
	update_cursor(terminal_col, terminal_row);
 	
	/* Newline support is left as an exercise. */
	puts("[elf_kernel64] hello\n");

	char temp[16];
	itoa((int)(pinfo), temp, 16);
	puts("[elf_kernel64] system_info=0x");
	puts(temp);
	puts("\n");

	if(pinfo != 0x4000)
	{
		puts("[elf_kernel] unexpected system info pointer!\n");
		return;
	}

	uint64_t total_mem = 0;
	puts("Memory blocks (start, length, type):\n");
	for(int i = 0; i < pinfo->num_memory_blocks; ++i)
	{
		char start[16], len[16], type[16];
		itoa(pinfo->memory_blocks[i].start, start, 16);
		itoa(pinfo->memory_blocks[i].length, len, 16);
		itoa(pinfo->memory_blocks[i].type, type, 16);

		puts(start);
		puts(" ");
		puts(len);
		puts(" ");
		puts(type);
		puts("\n");
		
		if(1 == pinfo->memory_blocks[i].type)
		{
			total_mem += pinfo->memory_blocks[i].length;
		}
	}
	total_mem >>= 20;
	itoa((int)(total_mem), temp, 10);
	puts("[elf_kernel64] ");
	puts(temp);
	puts(" MB RAM detected\n");	
}

