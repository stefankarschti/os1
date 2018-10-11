#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include "memory.h"

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

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
size_t terminal_row;
size_t terminal_col;
uint8_t terminal_color;
uint16_t* terminal_buffer;

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
 
void update_cursor(int x, int y)
{
	uint16_t pos = y * VGA_WIDTH + x;
 
	outb(0x3D4, 0x0F);
	outb(0x3D5, (uint8_t) (pos & 0xFF));
	outb(0x3D4, 0x0E);
	outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
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

#endif

