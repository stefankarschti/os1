#include <os1/syscall.h>

int main(void)
{
	char buffer[128];
	char output[160];
	static const char ready[] = "[user/init] shell input ready\n";
	static const char echo_prefix[] = "[user/init] shell input echo: ";

	os1_write(1, ready, sizeof(ready) - 1);
	for(;;)
	{
		const long count = os1_read(0, buffer, sizeof(buffer));
		if(count <= 0)
		{
			static const char failure[] = "[user/init] shell input read failed\n";
			os1_write(1, failure, sizeof(failure) - 1);
			os1_yield();
			continue;
		}

		size_t output_length = 0;
		for(size_t i = 0; i < (sizeof(echo_prefix) - 1); ++i)
		{
			output[output_length++] = echo_prefix[i];
		}
		for(size_t i = 0; (i < (size_t)count) && (output_length < sizeof(output)); ++i)
		{
			output[output_length++] = buffer[i];
		}
		os1_write(1, output, output_length);
	}
}
