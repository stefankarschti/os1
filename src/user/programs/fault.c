#include <os1/syscall.h>

int main(void)
{
	os1_write(1, "[user/fault] boom\n", 18);
	*(volatile unsigned long*)0x1234ull = 1;
	return 0;
}
