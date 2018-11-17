#ifndef _TASK_H_
#define _TASK_H_

#include <stddef.h>
#include <stdint.h>

#define CODE_SEG     0x0008
#define DATA_SEG     0x0010

struct Registers
{
	uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp, r8, r9, r10, r11, r12, r13, r14, r15;
	uint64_t rflags, cr3;
};

struct Task
{
    Task *nexttask = 0;
    uint64_t pid = 0;
    uint64_t waiting = 0;
    uint64_t timer = 0;
    uint64_t rsp = 0;
    uint64_t r15 = 0;
	Registers regs;
};

void initTasks();
Task* newTask(void *taskCode, uint64_t *stack, size_t stack_len);

#ifdef __cplusplus
extern "C" {
#endif

void startMultiTask(Task* task);

#ifdef __cplusplus
}
#endif

#endif
