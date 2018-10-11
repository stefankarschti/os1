#include "terminal.h"
#include "memory.h"

inline bool isprint(char c)
{
	return (c >= ' ');
}

Terminal::Terminal(uint16_t *screen, int numRows, int numCols)
	: _screen(screen), _numRows(numRows), _numCols(numCols), _row(0), _col(0)
{
	move(_row, _col);
}

void Terminal::move(int row, int col)
{
	_row = row;
	_col = col;
	uint16_t pos = _row * _numCols + _col;
 
	outb(0x3D4, 0x0F);
	outb(0x3D5, (uint8_t) (pos & 0xFF));
	outb(0x3D4, 0x0E);
	outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

void Terminal::write(const char* str)
{
	while(*str)
	{
		iput(*str);
		str++;
	}
	move(_row, _col);
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
		_screen[_row * _numCols + _col] = c + (7 << 8);
		_col++;
		if(_col >= _numCols)
		{
			_row++;
			_col = 0;
		}
	}

	if(_row >= _numRows)
	{
		// scroll up
		memcpy(_screen, _screen + _numCols, 2 * (_numRows - 1) * _numCols);
		memsetw(_screen + (_numRows - 1) * _numCols, ' ' + (7 << 8), 2 * _numCols);
		_row = _numRows - 1;
	}
}

