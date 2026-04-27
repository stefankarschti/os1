// x86_64 per-CPU state, GDT/TSS layout, and CPU bootstrap helpers. Assembly
// code depends on selected offsets below, so this header owns the static checks.
#ifndef OS1_KERNEL_ARCH_X86_64_CPU_CPU_H
#define OS1_KERNEL_ARCH_X86_64_CPU_CPU_H

#include "stdint.h"
#include "stddef.h"

#include "util/assert.h"
#include "proc/thread.h"

// Null segment
#define SEGDESC_NULL	0x0
// Code segment
#define SEGDESC64_CODE(dpl) (0x00209A0000000000ull | ((uint64_t)(dpl & 3u) << (32 + 13)))
// Data segment
#define SEGDESC64_DATA(dpl) (0x0000920000000000ull | ((uint64_t)(dpl & 3u) << (32 + 13)))

// Global segment descriptor numbers used by the kernel.
#define CPU_GDT_NULL	0x00
#define CPU_GDT_KCODE	0x08
#define CPU_GDT_KDATA	0x10
#define CPU_GDT_UDATA	0x18
#define CPU_GDT_UCODE	0x20
#define CPU_GDT_TSS	0x28
#define CPU_GDT_NDESC	7

struct pseudodesc
{
	uint16_t	pd_lim;
	uint64_t	pd_base;
};

struct Tss64
{
	uint32_t reserved0;
	uint64_t rsp0;
	uint64_t rsp1;
	uint64_t rsp2;
	uint64_t reserved1;
	uint64_t ist1;
	uint64_t ist2;
	uint64_t ist3;
	uint64_t ist4;
	uint64_t ist5;
	uint64_t ist6;
	uint64_t ist7;
	uint64_t reserved2;
	uint16_t reserved3;
	uint16_t io_bitmap_base;
} __attribute__((packed));

struct cpu
{
	cpu			*self;
	Thread		*current_thread;
	TrapFrame	interrupt_frame;
	uint64_t	gdt[CPU_GDT_NDESC];
	Tss64		tss;
	cpu			*next;
	uint8_t		id;
	volatile uint32_t booted;
	uint32_t	magic;
	char		kstacklo[1];
	char		kstackhi[0] __attribute__((aligned(4096)));
};

#define CPU_MAGIC	0x98765432

#define CPU_STATIC_ASSERT(name, expr) typedef char cpu_static_assert_##name[(expr) ? 1 : -1]

CPU_STATIC_ASSERT(current_thread_offset, offsetof(cpu, current_thread) == 8);
CPU_STATIC_ASSERT(interrupt_frame_offset, offsetof(cpu, interrupt_frame) == 16);
CPU_STATIC_ASSERT(tss_offset, offsetof(cpu, tss) == 248);
CPU_STATIC_ASSERT(tss_rsp0_offset, offsetof(cpu, tss) + offsetof(Tss64, rsp0) == 252);

#undef CPU_STATIC_ASSERT

extern cpu* g_cpu_boot;
extern cpu cpu_boot_template;

// Read the current stack pointer.
static inline uint64_t
read_rsp(void)
{
	uint64_t rsp;
	asm volatile("mov %%rsp, %0" : "=r"(rsp));
	return rsp;
}

// Return the current CPU record, using GS when available and stack fallback early.
static inline cpu *
cpu_cur() {
	cpu *c = nullptr;
	asm volatile("mov %%gs:0, %0" : "=r"(c));
	if(c && (((uint64_t)c & 0xFFF) == 0))
	{
		assert(c->magic == CPU_MAGIC);
		return c;
	}
	c = (cpu*)(read_rsp() & ~0xFFF);
	assert(c->magic == CPU_MAGIC);
	return c;
}

// Return true when running on the bootstrap CPU.
static inline int
cpu_onboot() {
	return cpu_cur() == g_cpu_boot;
}

// Initialize the current CPU's GDT, TSS, and GS base.
void cpu_init(void);
// Allocate a CPU record plus kernel stack from physical pages.
cpu *cpu_alloc(void);
// Start application processors using the AP trampoline.
void cpu_bootothers(uint64_t cr3);
// Publish the kernel stack loaded into TSS.RSP0 for ring transitions.
void cpu_set_kernel_stack(uint64_t stack_top);

#endif // OS1_KERNEL_ARCH_X86_64_CPU_CPU_H
