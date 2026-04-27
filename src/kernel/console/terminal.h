// Logical text terminal model. A Terminal owns an 80x25 cell buffer and cursor;
// display hardware is attached through TextDisplayBackend so terminal behavior
// stays separate from VGA/framebuffer rendering.
#ifndef OS1_KERNEL_CONSOLE_TERMINAL_H
#define OS1_KERNEL_CONSOLE_TERMINAL_H

#include "stdbool.h"
#include "stdint.h"

class TextDisplayBackend;

class Terminal
{
public:
	// Clear the backing cell buffer and reset the cursor to the origin.
	void Clear();
	// Attach a page-backed 80x25 text buffer owned by the caller.
	void SetBuffer(uint16_t *buffer);
	// Copy an existing VGA-compatible text buffer into this terminal.
	void Copy(const uint16_t *buffer);
	// Attach this terminal to the active display backend and present immediately.
	void Link(TextDisplayBackend *display);
	// Detach this terminal from the active display backend.
	void Unlink();

	// Move the logical cursor and refresh the attached display when present.
	void MoveCursor(int row, int col);
	// Write a nul-terminated string at the current cursor.
	void Write(const char* str);
	// Write one character at the current cursor.
	void Write(const char c);
	// Write a nul-terminated string followed by a newline.
	void WriteLn(const char* str);
	// Write an integer using the libc utoa formatting helper.
	void WriteInt(uint64_t value, int base = 10, int minimum_digits = 1);
	// Write an integer followed by a newline.
	void WriteIntLn(uint64_t value, int base = 10, int minimum_digits = 1);

	// Legacy blocking line reader retained for direct terminal experiments.
	void ReadLn(char *line);
	// Legacy keypress entry retained until all keyboard flow goes through console input.
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

	// Mutate the cell buffer without triggering a display refresh.
	void InternalWrite(char c);
};

#endif // OS1_KERNEL_CONSOLE_TERMINAL_H
