// Logical terminal implementation: cell-buffer mutation, cursor movement,
// scrolling, and presentation through an attached TextDisplayBackend.
#include "util/ctype.hpp"
#include "util/memory.h"
#include <stdlib.h>
#include "debug/debug.hpp"
#include "drivers/display/text_display.hpp"
#include "console/terminal.hpp"

void Terminal::clear()
{
	if(!buffer_) return;
	memsetw(buffer_, 0x0720, 80 * 25 * 2);
	move_cursor(0, 0);
}

void Terminal::set_buffer(uint16_t *buffer)
{
	buffer_ = buffer;
	display_ = nullptr;
}

void Terminal::copy(const uint16_t *buffer)
{
	if(!buffer_ || !buffer) return;
	memcpy(buffer_, buffer, 80 * 25 * 2);
}

void Terminal::link(TextDisplayBackend *display)
{
	if(!buffer_) return;
	display_ = display;
	move_cursor(row_, col_);
}

void Terminal::unlink()
{
	if(display_)
	{
		detach_text_display(display_);
		display_ = nullptr;
	}
}

void Terminal::move_cursor(int row, int col)
{
	row_ = row;
	col_ = col;
	if(display_)
	{
		present_text_display(display_, buffer_, width_, height_, col_, row_);
	}
}

void Terminal::write(const char* str)
{
	while(*str)
	{
		internal_write(*str);
		str++;
	}
	move_cursor(row_, col_);
}

void Terminal::write(const char c)
{
	internal_write(c);
	move_cursor(row_, col_);
}

void Terminal::write_line(const char *str)
{
	while(*str)
	{
		internal_write(*str);
		str++;
	}
	internal_write('\n');
	move_cursor(row_, col_);
}

void Terminal::write_int(uint64_t value, int base, int minimum_digits)
{
	char temp[256];
	utoa(value, temp, base, minimum_digits);
	write(temp);
}

void Terminal::write_int_line(uint64_t value, int base, int minimum_digits)
{
	write_int(value, base, minimum_digits);
	write('\n');
}

void Terminal::read_line(char *line)
{
	char *p = line;
	while(p - line < 256)
	{
		// wait for char
		while(!ascii_char_)
		{
			asm volatile("hlt"); // TODO: multi CPU sync
		}

		// remove first char
		char c = ascii_char_;
		ascii_char_ = 0;

		if(isprint(c))
		{
			*p++ = c;
		}
		if('\n' == c)
		{
			*p++ = 0;
			return;
		}
	}
}

void Terminal::key_press(char ascii, uint16_t scancode)
{
	(void)scancode;
	ascii_char_ = ascii;
	if('\n' == ascii || isprint(ascii))
		write(ascii);
}

void Terminal::internal_write(char c)
{
	if(!buffer_) return;
	if('\n' == c)
	{
		row_++;
		col_ = 0;
	}
	else if('\b' == c)
	{
		if(col_ > 0)
		{
			--col_;
			buffer_[row_ * width_ + col_] = 0x0720;
		}
	}
	else if(isprint(c))
	{
		buffer_[row_ * width_ + col_] = c + (7 << 8);
		col_++;
		if(col_ >= width_)
		{
			row_++;
			col_ = 0;
		}
	}

	if(row_ >= height_)
	{
		// scroll up
		memcpy(buffer_, buffer_ + width_, 2 * (height_ - 1) * width_);
		memsetw(buffer_ + (height_ - 1) * width_, 0x0720, 2 * width_);
		row_ = height_ - 1;
	}
}
