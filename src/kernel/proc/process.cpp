// Fixed process-table implementation. This file owns process IDs, parent/child
// metadata, address-space ownership, and process reaping once threads are gone.
#include "proc/process.h"

#include "debug/debug.h"
#include "handoff/memory_layout.h"
#include "mm/page_frame.h"
#include "mm/virtual_memory.h"
#include "proc/thread.h"
#include "util/memory.h"

namespace
{
constexpr size_t kProcessTablePageCount =
	(kMaxProcesses * sizeof(Process) + kPageSize - 1) / kPageSize;

uint64_t g_next_pid = 1;
Process *g_kernel_process = nullptr;

Process *nextFreeProcess()
{
	for(size_t i = 0; i < kMaxProcesses; ++i)
	{
		if(ProcessState::Free == processTable[i].state)
		{
			return processTable + i;
		}
	}
	return nullptr;
}

void clearProcess(Process *process)
{
	if(process)
	{
		memset(process, 0, sizeof(Process));
		process->state = ProcessState::Free;
	}
}

void orphanChildren(Process *process)
{
	if(nullptr == process)
	{
		return;
	}

	for(size_t i = 0; i < kMaxProcesses; ++i)
	{
		if(processTable[i].parent == process)
		{
			processTable[i].parent = nullptr;
		}
	}
}

void fillProcessName(Process *process, const char *name)
{
	if(nullptr == process)
	{
		return;
	}
	if(nullptr == name)
	{
		process->name[0] = 0;
		return;
	}
	size_t index = 0;
	while((index + 1) < sizeof(process->name) && name[index])
	{
		process->name[index] = name[index];
		++index;
	}
	process->name[index] = 0;
}
}

Process *processTable = nullptr;

bool InitializeProcessTable(PageFrameContainer &frames)
{
	g_next_pid = 1;
	g_kernel_process = nullptr;

	if(nullptr == processTable)
	{
		uint64_t process_table_address = 0;
		if(!frames.Allocate(process_table_address, kProcessTablePageCount))
		{
			debug("process table allocation failed")();
			return false;
		}
		processTable = (Process*)process_table_address;
		debug("process table allocated at 0x")(process_table_address, 16)();
	}

	memset(processTable, 0, kProcessTablePageCount * kPageSize);
	for(size_t i = 0; i < kMaxProcesses; ++i)
	{
		processTable[i].state = ProcessState::Free;
	}
	return true;
}

Process *createKernelProcess(uint64_t kernel_cr3)
{
	Process *process = nextFreeProcess();
	if(nullptr == process)
	{
		return nullptr;
	}

	process->pid = g_next_pid++;
	process->state = ProcessState::Ready;
	process->address_space.cr3 = kernel_cr3;
	process->exit_status = 0;
	fillProcessName(process, "kernel");
	g_kernel_process = process;
	return process;
}

Process *createUserProcess(const char *name, uint64_t cr3)
{
	Process *process = nextFreeProcess();
	if(nullptr == process)
	{
		return nullptr;
	}

	process->pid = g_next_pid++;
	process->state = ProcessState::Ready;
	process->address_space.cr3 = cr3;
	process->exit_status = 0;
	fillProcessName(process, name);
	return process;
}

bool processHasThreads(Process *process)
{
	if((nullptr == process) || (nullptr == threadTable))
	{
		return false;
	}

	for(size_t i = 0; i < kMaxThreads; ++i)
	{
		if((threadTable[i].process == process)
			&& (ThreadState::Free != threadTable[i].state))
		{
			return true;
		}
	}
	return false;
}

bool reapProcess(Process *process, PageFrameContainer &frames)
{
	if((nullptr == process) || processHasThreads(process))
	{
		return false;
	}

	if((process != g_kernel_process) && (process->address_space.cr3 != 0))
	{
		VirtualMemory vm(frames, process->address_space.cr3);
		vm.DestroyUserSlot(kUserPml4Index);
		uint64_t *pml4 = (uint64_t*)process->address_space.cr3;
		pml4[0] = 0;
		frames.Free(process->address_space.cr3);
	}
	orphanChildren(process);
	clearProcess(process);
	return true;
}