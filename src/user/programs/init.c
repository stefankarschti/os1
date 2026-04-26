#include <os1/syscall.h>

int main(void)
{
	static const char kShellPath[] = "/bin/sh";
	static const char kExecFailed[] = "[user/init] exec /bin/sh failed\n";

	const long result = os1_exec(kShellPath);
	os1_write(1, kExecFailed, sizeof(kExecFailed) - 1);
	return (result < 0) ? 1 : (int)result;
}
