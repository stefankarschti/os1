#include "cpu.h"
#include "pageframe.h"
#include "string.h"
#include "x86.h"
#include "ioapic.h"
#include "lapic.h"
#include "memory_layout.h"

namespace
{
[[noreturn]] void cpu_idle_loop()
{
	// Secondary CPUs are online in M1, but they do not yet run the final IDT or
	// scheduler path. Keep interrupts disabled so they cannot take vectors on
	// partially initialized state before later milestones wire them in fully.
	for(;;)
	{
		asm volatile("cli");
		asm volatile("hlt");
	}
}
}

cpu cpu_boot_template =
{
	self: nullptr,
	current_task: nullptr,
	interrupt_regs: {},
	gdt:
	{
		// 0x0 - unused (always faults: for trapping NULL far pointers)
		[0] = SEGDESC_NULL,

		// 0x08 - kernel code segment
		[CPU_GDT_KCODE >> 3] = SEGDESC64_CODE(0),

		// 0x10 - kernel data segment
		[CPU_GDT_KDATA >> 3] = SEGDESC64_DATA,
	},
	next: nullptr,
	id: 0,
	booted: 0,
	magic: CPU_MAGIC
};

cpu* g_cpu_boot;

void cpu_init()
{
	cpu *c = cpu_cur();
	c->self = c;

	// Load the GDT
	struct {
		uint16_t length;
		void*    base;
	} __attribute__((packed)) GDTR = { sizeof(c->gdt) - 1, c->gdt };

	asm volatile("lgdt %0" : : "m" (GDTR));

	// Reload all segment registers.
	asm volatile("movw %%ax,%%gs" :: "a" (CPU_GDT_KDATA));
	asm volatile("movw %%ax,%%fs" :: "a" (CPU_GDT_KDATA));
	asm volatile("movw %%ax,%%es" :: "a" (CPU_GDT_KDATA));
	asm volatile("movw %%ax,%%ds" :: "a" (CPU_GDT_KDATA));
	asm volatile("movw %%ax,%%ss" :: "a" (CPU_GDT_KDATA));
	asm volatile("jmp l1\n l1:\n");
	wrmsr(0xC0000101, (uint64_t)c);

	// We don't need an LDT.
	asm volatile("lldt %%ax" :: "a" (0));
}

// Allocate an additional cpu struct representing a non-bootstrap processor.
extern PageFrameContainer page_frames;
cpu *
cpu_alloc(void)
{
	cpu *last_cpu = g_cpu_boot;
	while(last_cpu->next)
		last_cpu = last_cpu->next;

	// allocate new cpu
	cpu *c = nullptr;
	{
		uint64_t p;
		bool result = page_frames.Allocate(p, 1);
		if(result) debug("alloc cpu at 0x")(p, 16)(); else debug("alloc cpu failed")();
		c = (cpu*)p;
	}

	// Clear the whole page for good measure: cpu struct and kernel stack
	assert(4096 == sizeof(cpu));
	memset((void*)c, 0, sizeof(cpu));

	// Now we need to initialize the new cpu struct
	// just to the same degree that cpu_boot was statically initialized.
	// The rest will be filled in by the CPU itself
	// when it starts up and calls cpu_init().

	// Initialize the new cpu's GDT by copying from the cpu_boot.
	// The TSS descriptor will be filled in later by cpu_init().
//	assert(sizeof(c->gdt) == sizeof(uint64_t) * CPU_GDT_NDESC);
//	memmove(c->gdt, g_cpu_boot->gdt, sizeof(c->gdt));

	// copy template
	memcpy(c, &cpu_boot_template, ((uint8_t*)&cpu_boot_template.kstacklo - (uint8_t*)&cpu_boot_template));

	// Magic verification tag for stack overflow/cpu corruption checking
	c->self = c;
	c->magic = CPU_MAGIC;

	// Chain the new CPU onto the tail of the list.
	last_cpu->next = c;

	return c;
}

void init()
{
	// a new cpu starting
        // TODO: initialize properly <---
	if (!cpu_onboot()) {
		debug("CPU ")(cpu_cur()->id)(" is alive at 0x")((uint64_t)cpu_cur(), 16)();

		uint64_t cookie = 0xfeedfacebae;
		assert(cpu_cur()->magic == CPU_MAGIC);
		cpu_init();
		debug("cpu init worked!")();
		assert(cookie == 0xfeedfacebae);

		// IOAPIC init
		ioapic_init();

		// LAPIC init
		lapic_init();

		// set booted flag
		xchg(&cpu_cur()->booted, 1);
	}

	cpu_idle_loop();
}

void
cpu_bootothers(uint64_t cr3)
{
	debug("booting other CPUs cr3 = 0x")(cr3, 16)();
	debug("current CPU = ")(cpu_cur()->id)();
	if (!cpu_onboot()) {
		// Just inform the boot cpu we've booted.
		xchg(&cpu_cur()->booted, 1);
		return;
	}

	// The AP trampoline lives in a fixed low-memory page until later boot paths
	// can hand over a richer bootstrap allocator.
	extern uint8_t cpustart_begin[];
	extern uint8_t cpustart_end[];
	uint8_t *code = (uint8_t*)kApTrampolineAddress;
	memcpy(code, cpustart_begin, (cpustart_end - cpustart_begin));

	// Boot CPUs
	cpu *c;
	for(c = g_cpu_boot; c; c = c->next)
	{
		if(c == cpu_cur())  // We've started already.
			continue;

		// Fill in %rsp, %rip and start code on cpu.
		*((uint64_t*)kApStartupCpuPageAddress) = (uint64_t)c;
		*((uint64_t*)kApStartupRipAddress) = (uint64_t)init;
		*((uint64_t*)kApStartupCr3Address) = cr3;
		memset((void*)kApStartupIdtAddress, 0, kApStartupIdtSizeBytes); // IDT.Length = 0, IDT.Base = 0

		debug("Starting CPU ")(c->id)();
		DebugMemory(0x0, 0x0 + 256);
		debug();

		lapic_startcpu(c->id, (uint64_t)code);

		// Wait for cpu to get through bootstrap.
		debug("waiting")();
		while(c->booted == 0)
			;
		debug("done waiting")();
	}
}
