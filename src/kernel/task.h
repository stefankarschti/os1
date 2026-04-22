#ifndef _TASK_H_
#define _TASK_H_

#include <stddef.h>
#include <stdint.h>

#include "pageframe.h"
#include "trapframe.h"

constexpr uint16_t kKernelCodeSegment = 0x08;
constexpr uint16_t kKernelDataSegment = 0x10;
constexpr uint16_t kUserDataSegment = 0x1B;
constexpr uint16_t kUserCodeSegment = 0x23;

constexpr size_t kMaxProcesses = 32;
constexpr size_t kMaxThreads = 32;

enum class ProcessState : uint32_t
{
	Free = 0,
	Ready = 1,
	Running = 2,
	Dying = 3,
};

enum class ThreadState : uint32_t
{
	Free = 0,
	Ready = 1,
	Running = 2,
	Blocked = 3,
	Dying = 4,
};

struct AddressSpace
{
	uint64_t cr3 = 0;
};

struct Process
{
	uint64_t pid = 0;
	ProcessState state = ProcessState::Free;
	AddressSpace address_space{};
	int exit_status = 0;
	char name[32]{};
};

struct Thread
{
	Thread *next = nullptr;
	uint64_t tid = 0;
	Process *process = nullptr;
	ThreadState state = ThreadState::Free;
	bool user_mode = false;
	uint16_t reserved0 = 0;
	uint32_t reserved1 = 0;
	uint64_t address_space_cr3 = 0;
	uint64_t kernel_stack_base = 0;
	uint64_t kernel_stack_top = 0;
	int exit_status = 0;
	TrapFrame frame{};
};

#define THREAD_STATIC_ASSERT(name, expr) typedef char thread_static_assert_##name[(expr) ? 1 : -1]

THREAD_STATIC_ASSERT(frame_offset, offsetof(Thread, frame) == 72);
THREAD_STATIC_ASSERT(cr3_offset, offsetof(Thread, address_space_cr3) == 40);
THREAD_STATIC_ASSERT(stack_top_offset, offsetof(Thread, kernel_stack_top) == 56);

#undef THREAD_STATIC_ASSERT

bool initTasks(PageFrameContainer &frames);
Process *createKernelProcess(uint64_t kernel_cr3);
Process *createUserProcess(const char *name, uint64_t cr3);
Thread *createKernelThread(Process *process, void (*entry)(void), PageFrameContainer &frames);
Thread *createUserThread(Process *process,
		uint64_t user_rip,
		uint64_t user_rsp,
		PageFrameContainer &frames);
Thread *currentThread(void);
Thread *idleThread(void);
Thread *nextRunnableThread(Thread *after);
void setCurrentThread(Thread *thread);
void markThreadReady(Thread *thread);
void markCurrentThreadDying(int exit_status);
void reapDeadThreads(PageFrameContainer &frames);
size_t runnableThreadCount(void);
Thread *firstRunnableUserThread(void);

extern Process *processTable;
extern Thread *threadTable;

#ifdef __cplusplus
extern "C" {
#endif

void startMultiTask(Thread *thread);

#ifdef __cplusplus
}
#endif

#endif
