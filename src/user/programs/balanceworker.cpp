#include <os1/syscall.hpp>

int main(void)
{
    for(unsigned i = 0; i < 8192; ++i)
    {
        os1::user::yield();
    }

    return 0;
}
