#include "terminal.h"
#include "memory.h"

inline bool isprint(char c)
{
	return (c >= ' ');
}

void Terminal::clear()
{
	memsetw(_buffer, 0x0720, 80 * 25 * 2);
	moveCursor(0, 0);
}

void Terminal::setBuffer(uint16_t *buffer)
{
	_buffer = buffer;
}

void Terminal::link()
{
	_screen = (uint16_t*)0xB8000;
	memcpy(_screen, _buffer, 80 * 25 * 2);
	moveCursor(_row, _col);
}

void Terminal::unlink()
{
	memsetw(_screen, 0x0720, 80 * 25 * 2);
	_screen = nullptr;
	uint16_t pos = 0;
	outb(0x3d4, 0x0f);
	outb(0x3d5, (uint8_t) (pos & 0xff));
	outb(0x3d4, 0x0e);
	outb(0x3d5, (uint8_t) ((pos >> 8) & 0xff));
}

void Terminal::moveCursor(int row, int col)
{
	_row = row;
	_col = col;
	if(_screen)
	{
		uint16_t pos = _row * _width + _col;
		outb(0x3D4, 0x0F);
		outb(0x3D5, (uint8_t) (pos & 0xFF));
		outb(0x3D4, 0x0E);
		outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
	}
}

void Terminal::write(const char* str)
{
	while(*str)
	{
		iput(*str);
		str++;
	}
	moveCursor(_row, _col);
}

void Terminal::readline(char *line)
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

void Terminal::iput(char c)
{
	if('\n' == c)
	{
		_row++;
		_col = 0;
	}
	else if(isprint(c))
	{
		_buffer[_row * _width + _col] = c + (7 << 8);
		if(_screen) _screen[_row * _width + _col] = c + (7 << 8);
		_col++;
		if(_col >= _width)
		{
			_row++;
			_col = 0;
		}
	}

	if(_row >= _height)
	{
		// scroll up
		memcpy(_buffer, _buffer + _width, 2 * (_height - 1) * _width);
		memsetw(_buffer + (_height - 1) * _width, 0x0720, 2 * _width);
		if(_screen)
		{
			memcpy(_screen, _screen + _width, 2 * (_height - 1) * _width);
			memsetw(_screen + (_height - 1) * _width, 0x0720, 2 * _width);
		}
		_row = _height - 1;
	}
}
