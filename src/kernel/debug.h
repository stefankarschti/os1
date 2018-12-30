#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>
#include <stddef.h>

class Debug
{
public:
    Debug();
	// Style 1
	void Write(const char c);
	void Write(const char *str);
	void WriteLn(const char* str);
	void WriteInt(uint64_t value, int base = 10, int minimum_digits = 1);
	void WriteIntLn(uint64_t value, int base = 10, int minimum_digits = 1);

	// Style 2
	Debug& operator()();
	Debug& operator()(const char* str);
	Debug& operator()(uint64_t value, int base = 10, int minimum_digits = 1);

	// Style 3
	Debug& s(const char* str);
	Debug& u(uint64_t value, int base = 10, int minimum_digits = 1);
	Debug& nl();

private:
	static const uint16_t PORT = 0x3F8;
	void InitSerial();
	int Busy();
};

void DebugMemory(uint64_t begin, uint64_t end);
extern Debug debug;
#endif // DEBUG_H
