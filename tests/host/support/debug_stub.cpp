#include "debug/debug.hpp"

Debug debug;

Debug::Debug() {}

void Debug::write(const char) {}

void Debug::write(const char*) {}

void Debug::write_line(const char*) {}

void Debug::write_int(uint64_t, int, int) {}

void Debug::write_int_line(uint64_t, int, int) {}

Debug& Debug::operator()()
{
    return *this;
}

Debug& Debug::operator()(const char*)
{
    return *this;
}

Debug& Debug::operator()(uint64_t, int, int)
{
    return *this;
}

Debug& Debug::s(const char*)
{
    return *this;
}

Debug& Debug::u(uint64_t, int, int)
{
    return *this;
}

Debug& Debug::nl()
{
    return *this;
}

void debug_memory(uint64_t, uint64_t) {}
