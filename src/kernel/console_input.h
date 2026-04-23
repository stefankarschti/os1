#ifndef CONSOLE_INPUT_H
#define CONSOLE_INPUT_H

#include <stddef.h>

constexpr size_t kConsoleInputMaxLineBytes = 128;

void ConsoleInputInitialize();
void ConsoleInputOnKeyboardChar(char ascii);
void ConsoleInputPollSerial();
bool ConsoleInputHasLine();
bool ConsoleInputPopLine(char *buffer, size_t buffer_size, size_t &line_length);

#endif
