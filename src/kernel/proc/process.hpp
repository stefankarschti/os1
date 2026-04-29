// Process ownership model. This header describes process table entries,
// address-space ownership, and the lifecycle operations that are independent of
// scheduler run-queue policy.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "sync/smp.hpp"

class PageFrameContainer;

// The kernel keeps a fixed process table until dynamic kernel allocation grows.
constexpr size_t kMaxProcesses = 32;

// Process lifecycle state as observed by wait/reap and observe syscalls.
enum class ProcessState : uint32_t
{
    Free = 0,
    Ready = 1,
    Running = 2,
    Dying = 3,
    Zombie = 4,
};

// Owned page-table root for a process address space.
struct AddressSpace
{
    uint64_t cr3 = 0;
};

// Fixed-size process table entry. Future credentials, sessions, descriptor
// tables, and resource handles should attach here rather than to scheduler code.
struct Process
{
    uint64_t pid = 0;
    ProcessState state = ProcessState::Free;
    AddressSpace address_space{};
    int exit_status = 0;
    Process* parent = nullptr;
    char name[32]{};
};

// allocate and reset the fixed process table from physical pages.
bool initialize_process_table(PageFrameContainer& frames);

// Create the immortal kernel process that owns kernel threads.
Process* create_kernel_process(uint64_t kernel_cr3);

// Create a user process record bound to an already-built address space.
Process* create_user_process(const char* name, uint64_t cr3);

// Return true if any non-free thread still belongs to `process`.
bool process_has_threads(Process* process);

// Reclaim a process address space and table entry when all threads are gone.
bool reap_process(Process* process, PageFrameContainer& frames);

// BSP-only for now: global fixed process table storage.
OS1_BSP_ONLY extern Process* processTable;
