// Read-only observability syscall. It snapshots kernel state into stable UAPI
// structs without giving user space direct pointers to kernel-owned records.
#ifndef OS1_KERNEL_SYSCALL_OBSERVE_H
#define OS1_KERNEL_SYSCALL_OBSERVE_H

#include <stddef.h>
#include <stdint.h>

#include "handoff/bootinfo.h"
#include "drivers/display/text_display.h"
#include "mm/page_frame.h"

struct ObserveContext
{
	const BootInfo *boot_info = nullptr;
	const TextDisplayBackend *text_display = nullptr;
	uint64_t timer_ticks = 0;
	PageFrameContainer *frames = nullptr;
};

// Fill an observe record of `kind` into a user buffer.
long SysObserve(const ObserveContext &context, uint64_t kind, uint64_t user_buffer, size_t length);

#endif // OS1_KERNEL_SYSCALL_OBSERVE_H
