#include <os1/syscall.h>

int main(void)
{
	os1_write(1, "[user/init] hello\n", 18);
	for(int i = 0; i < 3; ++i)
	{
		os1_yield();
	}
	os1_write(1, "[user/init] exit\n", 17);
	return 0;
}
