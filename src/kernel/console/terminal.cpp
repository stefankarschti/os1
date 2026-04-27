// Logical terminal implementation: cell-buffer mutation, cursor movement,
// scrolling, and presentation through an attached TextDisplayBackend.
#include "util/ctype.h"
#include "util/memory.h"
#include <stdlib.h>
#include "debug/debug.h"
#include "drivers/display/text_display.h"
#include "console/terminal.h"

void Terminal::Clear()
{
	if(!buffer_) return;
	memsetw(buffer_, 0x0720, 80 * 25 * 2);
	MoveCursor(0, 0);
}

void Terminal::SetBuffer(uint16_t *buffer)
{
	buffer_ = buffer;
	display_ = nullptr;
}

void Terminal::Copy(const uint16_t *buffer)
{
	if(!buffer_ || !buffer) return;
	memcpy(buffer_, buffer, 80 * 25 * 2);
}

void Terminal::Link(TextDisplayBackend *display)
{
	if(!buffer_) return;
	display_ = display;
	MoveCursor(row_, col_);
}

void Terminal::Unlink()
{
	if(display_)
	{
		DetachTextDisplay(display_);
		display_ = nullptr;
	}
}

void Terminal::MoveCursor(int row, int col)
{
	row_ = row;
	col_ = col;
	if(display_)
	{
		PresentTextDisplay(display_, buffer_, width_, height_, col_, row_);
	}
}

void Terminal::Write(const char* str)
{
	while(*str)
	{
		InternalWrite(*str);
		str++;
	}
	MoveCursor(row_, col_);
}

void Terminal::Write(const char c)
{
	InternalWrite(c);
	MoveCursor(row_, col_);
}

void Terminal::WriteLn(const char *str)
{
	while(*str)
	{
		InternalWrite(*str);
		str++;
	}
	InternalWrite('\n');
	MoveCursor(row_, col_);
}

void Terminal::WriteInt(uint64_t value, int base, int minimum_digits)
{
	char temp[256];
	utoa(value, temp, base, minimum_digits);
	Write(temp);
}

void Terminal::WriteIntLn(uint64_t value, int base, int minimum_digits)
{
	WriteInt(value, base, minimum_digits);
	Write('\n');
}

void Terminal::ReadLn(char *line)
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

void Terminal::KeyPress(char ascii, uint16_t scancode)
{
	(void)scancode;
	ascii_char_ = ascii;
	if('\n' == ascii || isprint(ascii))
		Write(ascii);
}

void Terminal::InternalWrite(char c)
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
