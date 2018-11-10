#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>
#include <stddef.h>

class Debug
{
public:
    Debug();
	void Write(const char c);
	void Write(const char *str);
	void WriteLn(const char* str);
	void WriteInt(uint64_t value, int base = 10, int minimum_digits = 1);
	void WriteIntLn(uint64_t value, int base = 10, int minimum_digits = 1);

private:
	static const uint16_t PORT = 0x3F8;
	void InitSerial();
	int Busy();
};

extern Debug debug;
#endif // DEBUG_H
