// Thread object model and the public scheduler-facing task API. Process table
// ownership lives in proc/process.hpp; this header owns Thread layout, wait state,
// and the assembly-visible offsets used by context-switch code.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "arch/x86_64/interrupt/trap_frame.hpp"
#include "mm/page_frame.hpp"
#include "proc/process.hpp"

constexpr uint16_t kKernelCodeSegment = 0x08;
constexpr uint16_t kKernelDataSegment = 0x10;
constexpr uint16_t kUserDataSegment = 0x1B;
constexpr uint16_t kUserCodeSegment = 0x23;

constexpr size_t kMaxThreads = 32;

enum class ThreadState : uint32_t
{
    Free = 0,
    Ready = 1,
    Running = 2,
    Blocked = 3,
    Dying = 4,
};

enum class ThreadWaitReason : uint32_t
{
    None = 0,
    ConsoleRead = 1,
    ChildExit = 2,
};

struct Thread
{
    Thread* next = nullptr;
    uint64_t tid = 0;
    Process* process = nullptr;
    ThreadState state = ThreadState::Free;
    bool user_mode = false;
    uint16_t reserved0 = 0;
    uint32_t reserved1 = 0;
    uint64_t address_space_cr3 = 0;
    uint64_t kernel_stack_base = 0;
    uint64_t kernel_stack_top = 0;
    int exit_status = 0;
    TrapFrame frame{};
    ThreadWaitReason wait_reason = ThreadWaitReason::None;
    uint32_t reserved2 = 0;
    uint64_t wait_address = 0;
    uint64_t wait_length = 0;
};

#define THREAD_STATIC_ASSERT(name, expr) typedef char thread_static_assert_##name[(expr) ? 1 : -1]

THREAD_STATIC_ASSERT(frame_offset, offsetof(Thread, frame) == 72);
THREAD_STATIC_ASSERT(cr3_offset, offsetof(Thread, address_space_cr3) == 40);
THREAD_STATIC_ASSERT(stack_top_offset, offsetof(Thread, kernel_stack_top) == 56);

#undef THREAD_STATIC_ASSERT

// allocate and initialize the fixed thread table.
bool init_tasks(PageFrameContainer& frames);
// Create a kernel-mode thread with a bootstrap frame that enters `entry`.
Thread* create_kernel_thread(Process* process, void (*entry)(void), PageFrameContainer& frames);
// Create a user-mode thread with an interrupt-return frame for ring 3 entry.
Thread* create_user_thread(Process* process,
                           uint64_t user_rip,
                           uint64_t user_rsp,
                           PageFrameContainer& frames);
// Return the thread bound to the current CPU.
Thread* current_thread(void);
// Return the scheduler's idle thread.
Thread* idle_thread(void);
// Find the next runnable thread after `after` in circular table order.
Thread* next_runnable_thread(Thread* after);
// Rebuild next pointers for runnable threads after state changes.
void relink_runnable_threads();
// Bind the current CPU to `thread`.
void set_current_thread(Thread* thread);
// Mark a thread ready and update its owning process state if needed.
void mark_thread_ready(Thread* thread);
// Block the current thread on a typed wait object.
void block_current_thread(ThreadWaitReason reason,
                          uint64_t wait_address = 0,
                          uint64_t wait_length = 0);
// clear a thread's wait metadata after the wait has completed.
void clear_thread_wait(Thread* thread);
// Return the first thread blocked on a wait reason.
Thread* first_blocked_thread(ThreadWaitReason reason);
// Mark the current thread dying and publish its process exit status.
void mark_current_thread_dying(int exit_status);
// Reset a thread table entry to the free state.
void clear_thread(Thread* thread);
// Reclaim dying threads and their associated process state when possible.
void reap_dead_threads(PageFrameContainer& frames);
// Count threads currently eligible to run.
size_t runnable_thread_count(void);
// Return the first runnable user thread, if any.
Thread* first_runnable_user_thread(void);

extern Thread* threadTable;

#ifdef __cplusplus
extern "C"
{
#endif

    // Assembly context-switch entry: resume execution on the supplied thread frame.
    void start_multi_task(Thread* thread);

#ifdef __cplusplus
}
#endif
