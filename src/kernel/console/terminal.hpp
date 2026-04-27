// Logical text terminal model. A Terminal owns an 80x25 cell buffer and cursor;
// display hardware is attached through TextDisplayBackend so terminal behavior
// stays separate from VGA/framebuffer rendering.
#pragma once

#include "stdbool.h"
#include "stdint.h"

class TextDisplayBackend;

class Terminal
{
public:
	// clear the backing cell buffer and reset the cursor to the origin.
	void clear();
	// attach a page-backed 80x25 text buffer owned by the caller.
	void set_buffer(uint16_t *buffer);
	// copy an existing VGA-compatible text buffer into this terminal.
	void copy(const uint16_t *buffer);
	// attach this terminal to the active display backend and present immediately.
	void link(TextDisplayBackend *display);
	// Detach this terminal from the active display backend.
	void unlink();

	// Move the logical cursor and refresh the attached display when present.
	void move_cursor(int row, int col);
	// write a nul-terminated string at the current cursor.
	void write(const char* str);
	// write one character at the current cursor.
	void write(const char c);
	// write a nul-terminated string followed by a newline.
	void write_line(const char* str);
	// write an integer using the libc utoa formatting helper.
	void write_int(uint64_t value, int base = 10, int minimum_digits = 1);
	// write an integer followed by a newline.
	void write_int_line(uint64_t value, int base = 10, int minimum_digits = 1);

	// Legacy blocking line reader retained for direct terminal experiments.
	void read_line(char *line);
	// Legacy keypress entry retained until all keyboard flow goes through console input.
	void key_press(char ascii, uint16_t scancode);

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
	void internal_write(char c);
};

