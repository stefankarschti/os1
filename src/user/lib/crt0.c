#include <os1/syscall.h>

int main(void);

__attribute__((noreturn)) void _start(void)
{
	const int exit_code = main();
	os1_exit(exit_code);
	for(;;)
	{
		asm volatile("hlt");
	}
}
