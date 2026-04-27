#include <os1/syscall.hpp>

int main(void);

extern "C" [[noreturn]] void _start(void)
{
	const int exit_code = main();
	os1::user::exit(exit_code);
	for(;;)
	{
		asm volatile("hlt");
	}
}
