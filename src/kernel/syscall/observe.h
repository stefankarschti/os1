#ifndef OS1_KERNEL_SYSCALL_OBSERVE_H
#define OS1_KERNEL_SYSCALL_OBSERVE_H

#include <stddef.h>
#include <stdint.h>

#include "bootinfo.h"
#include "display.h"
#include "pageframe.h"

struct ObserveContext
{
	const BootInfo *boot_info = nullptr;
	const TextDisplayBackend *text_display = nullptr;
	uint64_t timer_ticks = 0;
	PageFrameContainer *frames = nullptr;
};

long SysObserve(const ObserveContext &context, uint64_t kind, uint64_t user_buffer, size_t length);

#endif
