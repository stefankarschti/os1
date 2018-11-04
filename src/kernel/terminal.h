#ifndef _TERMINAL_H_
#define _TERMINAL_H_

// C++ Terminal class

#include "stdbool.h"
#include "stdint.h"

/**
 * @brief The Terminal class
 */
class Terminal
{
public:
	void clear();
	void setBuffer(uint16_t *buffer);
	void link();	// links to real screen
	void unlink();	// unlinks from real screen

	void moveCursor(int row, int col);
	void write(const char* str);

	void readline(char *line);
	void KeyPress(char ascii, uint16_t scancode);

private:
	uint16_t *_buffer = nullptr;
	uint16_t *_screen = nullptr;	// if not null, mirror to screen
	const int _width = 80;
	const int _height = 25;
	int _row = 0;
	int _col = 0;
	
	int ascii_buffer_size_ = 0;
//	char ascii_buffer_[32 + 1];

	void iput(char c);
};

#endif

