// Serial debug logger. This remains the most reliable output path across boot,
// framebuffer, scheduler, and fault-handling failures.
#pragma once

#include <stdint.h>
#include <stddef.h>

class Debug
{
public:
	// initialize COM1 for polling serial output.
    Debug();
	// write one byte to COM1 after waiting for the transmit FIFO.
	void write(const char c);
	// write a nul-terminated string to COM1.
	void write(const char *str);
	// write a string followed by a newline.
	void write_line(const char* str);
	// Format and write an integer.
	void write_int(uint64_t value, int base = 10, int minimum_digits = 1);
	// Format and write an integer followed by a newline.
	void write_int_line(uint64_t value, int base = 10, int minimum_digits = 1);

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
	void init_serial();
	// Return non-zero while the serial transmitter is busy.
	int busy();
};

// Dump a memory range in hex plus printable ASCII to the serial logger.
void debug_memory(uint64_t begin, uint64_t end);
// Global serial logger, constructed before kernel_main runs.
extern Debug debug;
