#ifndef _TRAPFRAME_H_
#define _TRAPFRAME_H_

#include <stddef.h>
#include <stdint.h>

// `TrapFrame` is the single saved-register contract shared by IRQ, exception,
// syscall, and scheduler return paths. Assembly writes directly into this
// layout, so the offsets are guarded explicitly below.
struct TrapFrame
{
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rbp;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rdx;
	uint64_t rcx;
	uint64_t rbx;
	uint64_t rax;
	uint64_t vector;
	uint64_t error_code;
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
};

#define TRAPFRAME_STATIC_ASSERT(name, expr) typedef char trapframe_static_assert_##name[(expr) ? 1 : -1]

TRAPFRAME_STATIC_ASSERT(size, sizeof(TrapFrame) == 22 * sizeof(uint64_t));
TRAPFRAME_STATIC_ASSERT(r15_offset, offsetof(TrapFrame, r15) == 0);
TRAPFRAME_STATIC_ASSERT(rax_offset, offsetof(TrapFrame, rax) == 14 * sizeof(uint64_t));
TRAPFRAME_STATIC_ASSERT(vector_offset, offsetof(TrapFrame, vector) == 15 * sizeof(uint64_t));
TRAPFRAME_STATIC_ASSERT(error_offset, offsetof(TrapFrame, error_code) == 16 * sizeof(uint64_t));
TRAPFRAME_STATIC_ASSERT(rip_offset, offsetof(TrapFrame, rip) == 17 * sizeof(uint64_t));
TRAPFRAME_STATIC_ASSERT(ss_offset, offsetof(TrapFrame, ss) == 21 * sizeof(uint64_t));

#undef TRAPFRAME_STATIC_ASSERT

[[nodiscard]] inline bool TrapFrameIsUser(const TrapFrame &frame)
{
	return (frame.cs & 3u) == 3u;
}

#endif
