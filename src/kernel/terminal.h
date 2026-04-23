#ifndef _TERMINAL_H_
#define _TERMINAL_H_

// C++ Terminal class

#include "stdbool.h"
#include "stdint.h"

class TextDisplayBackend;

/**
 * @brief The Terminal class
 */
class Terminal
{
public:
	void Clear();
	void SetBuffer(uint16_t *buffer);
	void Copy(const uint16_t *buffer);
	void Link(TextDisplayBackend *display);	// links to active display backend
	void Unlink();	// detaches from whichever backend is active

	void MoveCursor(int row, int col);
	void Write(const char* str);
	void Write(const char c);
	void WriteLn(const char* str);
	void WriteInt(uint64_t value, int base = 10, int minimum_digits = 1);
	void WriteIntLn(uint64_t value, int base = 10, int minimum_digits = 1);

	void ReadLn(char *line);
	void KeyPress(char ascii, uint16_t scancode);

private:
	uint16_t *buffer_ = nullptr;
	TextDisplayBackend *display_ = nullptr;
	const int width_ = 80;
	const int height_ = 25;
	int row_ = 0;
	int col_ = 0;
	
	int ascii_char_ = 0;
//	int ascii_buffer_size_ = 0;
//	uint64_t a;
//	char ascii_buffer_[32 + 1];

	void InternalWrite(char c);
};

#endif
