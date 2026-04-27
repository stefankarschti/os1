// Shared kernel entry and bring-up sequencing. Boot-frontends normalize their
// native firmware/bootloader state into BootInfo, then both paths arrive here.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <span>

#include "arch/x86_64/apic/ioapic.h"
#include "arch/x86_64/apic/lapic.h"
#include "arch/x86_64/apic/mp.h"
#include "arch/x86_64/apic/pic.h"
#include "arch/x86_64/cpu/control_regs.h"
#include "arch/x86_64/cpu/cpu.h"
#include "arch/x86_64/interrupt/interrupt.h"
#include "console/console.h"
#include "console/console_input.h"
#include "core/fault.h"
#include "core/kernel_state.h"
#include "core/panic.h"
#include "debug/debug.h"
#include "drivers/display/text_display.h"
#include "drivers/timer/pit.h"
#include "fs/initrd.h"
#include "handoff/bootinfo.h"
#include "handoff/memory_layout.h"
#include "mm/boot_mapping.h"
#include "mm/boot_reserve.h"
#include "mm/virtual_memory.h"
#include "platform/platform.h"
#include "proc/user_program.h"
#include "sched/idle.h"
#include "proc/thread.h"
#include "util/align.h"
#include "util/memory.h"

extern "C" void KernelMain(BootInfo *info, cpu* cpu_boot)
{
	bool result = false;
	debug("[kernel64] hello!\n");

	g_boot_info = OwnBootInfo(info);
	if(nullptr == g_boot_info)
	{
		debug("invalid boot info")();
		return;
	}
	BindInitrdBootInfo(g_boot_info);
	debug("boot source: ")(BootSourceName(g_boot_info->source))();
	if(g_boot_info->bootloader_name)
	{
		debug("bootloader: ")(g_boot_info->bootloader_name)();
	}

	const std::span<const BootMemoryRegion> memory_regions = BootMemoryRegions(*g_boot_info);

	{
		g_cpu_boot = cpu_boot;
		memcpy(g_cpu_boot, &cpu_boot_template, ((uint8_t*)&cpu_boot_template.kstacklo - (uint8_t*)&cpu_boot_template));
		cpu_init();
	}

	debug("initializing page frame allocator")();
	result = page_frames.Initialize(memory_regions, kPageFrameBitmapBaseAddress, kPageFrameBitmapQwordLimit);
	debug(result ? "Success" : "Failure")();
	if(!result)
	{
		return;
	}

	if(g_boot_info->module_count > 0)
	{
		for(uint32_t i = 0; i < g_boot_info->module_count; ++i)
		{
			ReserveTrackedPhysicalRange(page_frames, g_boot_info->modules[i].physical_start, g_boot_info->modules[i].length);
		}
		debug("initrd module discovered")();
	}
	if(0 != g_boot_info->framebuffer.physical_address)
	{
		ReserveTrackedPhysicalRange(page_frames,
				g_boot_info->framebuffer.physical_address,
				BootFramebufferLengthBytes(g_boot_info->framebuffer));
	}

	VirtualMemory kvm(page_frames);
	debug("create kernel identity page tables")();
	result = kvm.Allocate(0x0, kKernelReservedPhysicalStart / kPageSize, true);
	if(!result)
	{
		return;
	}
	for(size_t i = 0; i < memory_regions.size(); ++i)
	{
		const BootMemoryRegion &region = memory_regions[i];
		if(BootMemoryRegionIsUsable(region)
			&& (region.physical_start >= kKernelReservedPhysicalStart)
			&& (region.length > 0))
		{
			const uint64_t start = AlignDown(region.physical_start, kPageSize);
			const uint64_t end = AlignUp(region.physical_start + region.length, kPageSize);
			if(!kvm.Allocate(start, (end - start) / kPageSize, true))
			{
				return;
			}
		}
	}
	// Boot modules and the framebuffer may live in non-usable ranges on the
	// modern path, but the current kernel still dereferences them as physical
	// identity mappings after it switches to its own CR3. Map those explicit
	// boot-critical ranges before activating the kernel page tables.
	for(uint32_t i = 0; i < g_boot_info->module_count; ++i)
	{
		if(!MapIdentityRange(kvm, g_boot_info->modules[i].physical_start, g_boot_info->modules[i].length))
		{
			return;
		}
	}
	if(!MapIdentityRange(kvm,
			g_boot_info->framebuffer.physical_address,
			BootFramebufferLengthBytes(g_boot_info->framebuffer)))
	{
		return;
	}
	if(!MapIdentityRange(kvm, g_boot_info->rsdp_physical, 64))
	{
		return;
	}
	kvm.Activate();
	g_kernel_root_cr3 = kvm.Root();

	if(!platform_init(*g_boot_info, kvm))
	{
		return;
	}
	debug("muliprocessor: ")(ismp ? "yes" : "no")();
	debug("ncpu: ")(ncpu)();

	pic_init();
	ioapic_init();
	lapic_init();
	cpu_bootothers(g_kernel_root_cr3);

	for(size_t i = 0; i < kNumTerminals; ++i)
	{
		uint64_t page = 0;
		if(page_frames.Allocate(page))
		{
			terminal[i].SetBuffer((uint16_t*)page);
			terminal[i].Clear();
			terminal[i].Write("Terminal ");
			terminal[i].WriteIntLn(i + 1);
		}
	}

	g_text_display = SelectTextDisplay(*g_boot_info);
	active_terminal = &terminal[0];
	if(BootSource::BiosLegacy == g_boot_info->source)
	{
		active_terminal->Copy((uint16_t*)0xB8000);
	}
	else
	{
		active_terminal->Clear();
	}
	active_terminal->Link(g_text_display);
	if(BootSource::BiosLegacy == g_boot_info->source)
	{
		active_terminal->MoveCursor(g_boot_info->text_console.cursor_y, g_boot_info->text_console.cursor_x);
	}
	else
	{
		active_terminal->MoveCursor(0, 0);
	}
	active_terminal->WriteLn("[kernel64] hello");

	result = interrupts.Initialize();
	debug(result ? "Interrupts initialization successful" : "Interrupts initialization failed")();
	if(!result)
	{
		return;
	}

	const uint8_t kernel_fault_vectors[] = {
		T_DIVIDE, T_DEBUG, T_NMI, T_BRKPT, T_OFLOW, T_BOUND, T_ILLOP, T_DEVICE,
		T_DBLFLT, T_TSS, T_SEGNP, T_STACK, T_GPFLT, T_PGFLT, T_FPERR, T_ALIGN,
		T_MCHK, T_SIMD, 29, T_SECEV
	};
	for(size_t i = 0; i < sizeof(kernel_fault_vectors); ++i)
	{
		interrupts.SetExceptionHandler(kernel_fault_vectors[i], OnKernelException);
	}

	keyboard.Initialize();
	keyboard.SetActiveTerminal(active_terminal);
	ConsoleInputInitialize();
	if(ismp)
	{
		if(!platform_enable_isa_irq(IRQ_TIMER, IRQ_TIMER)
			|| !platform_enable_isa_irq(IRQ_KBD))
		{
			return;
		}
	}

	if(!initTasks(page_frames))
	{
		return;
	}

	Process *kernel_process = createKernelProcess(g_kernel_root_cr3);
	if(nullptr == kernel_process)
	{
		return;
	}
	if(nullptr == createKernelThread(kernel_process, KernelIdleThread, page_frames))
	{
		return;
	}

	Thread *init_thread = LoadUserProgram(page_frames, g_kernel_root_cr3, "/bin/init");
	if(nullptr == init_thread)
	{
		WriteConsoleLine("failed to load /bin/init");
		return;
	}

	debug("start multitasking")();
	WriteConsoleLine("starting first user process");
	SetTimer(1000);
	startMultiTask(init_thread);
	HaltForever();
}