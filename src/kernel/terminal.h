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
	void Clear();
	void SetBuffer(uint16_t *buffer);
	void Link();	// links to real screen
	void Unlink();	// unlinks from real screen

	void MoveCursor(int row, int col);
	void Write(const char* str);
	void Write(const char c);
	void WriteLn(const char* str);

	void ReadLn(char *line);
	void KeyPress(char ascii, uint16_t scancode);

private:
	uint16_t *buffer_ = nullptr;
	uint16_t *screen_ = nullptr;	// if not null, mirror to screen
	const int width_ = 80;
	const int height_ = 25;
	int row_ = 0;
	int col_ = 0;
	
//	int ascii_buffer_size_ = 0;
//	uint64_t a;
//	char ascii_buffer_[32 + 1];

	void InternalWrite(char c);
};

#endif

