#include "terminal.h"
#include "memory.h"

void Terminal::Clear()
{
	memsetw(buffer_, 0x0720, 80 * 25 * 2);
	MoveCursor(0, 0);
}

void Terminal::SetBuffer(uint16_t *buffer)
{
	buffer_ = buffer;
}

void Terminal::Link()
{
	screen_ = (uint16_t*)0xB8000;
	memcpy(screen_, buffer_, 80 * 25 * 2);
	MoveCursor(row_, col_);
}

void Terminal::Unlink()
{
	memsetw(screen_, 0x0720, 80 * 25 * 2);
	screen_ = nullptr;
	uint16_t pos = 0;
	outb(0x3d4, 0x0f);
	outb(0x3d5, (uint8_t) (pos & 0xff));
	outb(0x3d4, 0x0e);
	outb(0x3d5, (uint8_t) ((pos >> 8) & 0xff));
}

void Terminal::MoveCursor(int row, int col)
{
	row_ = row;
	col_ = col;
	if(screen_)
	{
		uint16_t pos = row_ * width_ + col_;
		outb(0x3D4, 0x0F);
		outb(0x3D5, (uint8_t) (pos & 0xFF));
		outb(0x3D4, 0x0E);
		outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
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

void Terminal::ReadLn(char *line)
{
/*
	char *p = line;

	// wait for char
	while(!ascii_buffer_size_)
	{
		asm volatile("pause");
	}

	// remove first char
	asm volatile("cli"); // TODO: proper mutex on multiprocessor
	char c = ascii_buffer_[0];
	memcpy(ascii_buffer_, ascii_buffer_ + 1, 32);
	ascii_buffer_size_--;
	asm volatile("sti");

	if(isprint(c)) *p++ = c;
	if('\n' == c)
	{
		*p++ = 0;
		return;
	}
*/
}

void Terminal::KeyPress(char ascii, uint16_t scancode)
{
	/*
	if(ascii && ascii_buffer_size_ < 32)
	{
		ascii_buffer_[ascii_buffer_size_] = ascii;
		ascii_buffer_size_++;
		ascii_buffer_[ascii_buffer_size_] = 0;
	}
	*/
}

void Terminal::InternalWrite(char c)
{
	if('\n' == c)
	{
		row_++;
		col_ = 0;
	}
	else if(isprint(c))
	{
		buffer_[row_ * width_ + col_] = c + (7 << 8);
		if(screen_) screen_[row_ * width_ + col_] = c + (7 << 8);
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
		if(screen_)
		{
			memcpy(screen_, screen_ + width_, 2 * (height_ - 1) * width_);
			memsetw(screen_ + (height_ - 1) * width_, 0x0720, 2 * width_);
		}
		row_ = height_ - 1;
	}
}
