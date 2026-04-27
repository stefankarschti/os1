// Serial debug logger. This remains the most reliable output path across boot,
// framebuffer, scheduler, and fault-handling failures.
#ifndef OS1_KERNEL_DEBUG_DEBUG_H
#define OS1_KERNEL_DEBUG_DEBUG_H

#include <stdint.h>
#include <stddef.h>

class Debug
{
public:
	// Initialize COM1 for polling serial output.
    Debug();
	// Write one byte to COM1 after waiting for the transmit FIFO.
	void Write(const char c);
	// Write a nul-terminated string to COM1.
	void Write(const char *str);
	// Write a string followed by a newline.
	void WriteLn(const char* str);
	// Format and write an integer.
	void WriteInt(uint64_t value, int base = 10, int minimum_digits = 1);
	// Format and write an integer followed by a newline.
	void WriteIntLn(uint64_t value, int base = 10, int minimum_digits = 1);

	// Finish a chained debug expression with a newline.
	Debug& operator()();
	// Append a string to a chained debug expression.
	Debug& operator()(const char* str);
	// Append a formatted integer to a chained debug expression.
	Debug& operator()(uint64_t value, int base = 10, int minimum_digits = 1);

	// Named string append helper for older call sites.
	Debug& s(const char* str);
	// Named integer append helper for older call sites.
	Debug& u(uint64_t value, int base = 10, int minimum_digits = 1);
	// Named newline helper for older call sites.
	Debug& nl();

private:
	static const uint16_t PORT = 0x3F8;
	// Program COM1 to 115200 8N1 and enable FIFO mode.
	void InitSerial();
	// Return non-zero while the serial transmitter is busy.
	int Busy();
};

// Dump a memory range in hex plus printable ASCII to the serial logger.
void DebugMemory(uint64_t begin, uint64_t end);
// Global serial logger, constructed before KernelMain runs.
extern Debug debug;
#endif // OS1_KERNEL_DEBUG_DEBUG_H
