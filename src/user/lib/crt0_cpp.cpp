#include <os1/syscall.h>

int main(void);

extern "C" [[noreturn]] void _start(void)
{
	const int exit_code = main();
	os1_exit(exit_code);
	for(;;)
	{
		asm volatile("hlt");
	}
}
