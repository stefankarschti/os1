#ifndef _cpu_h_
#define _cpu_h_

#include "stdint.h"
#include "stddef.h"

#include "assert.h"

// Null segment
#define SEGDESC_NULL	0x0
// Code segment
#define SEGDESC64_CODE(dpl) (0x00209A0000000000ull & ((dpl & 3ull) << (32 + 13)))
// Data segment
#define SEGDESC64_DATA 0x0000920000000000ull

// Global segment descriptor numbers used by the kernel
#define CPU_GDT_NULL	0x00	// null descriptor (required by x86 processor)
#define CPU_GDT_KCODE	0x08	// kernel text
#define CPU_GDT_KDATA	0x10	// kernel data
#define CPU_GDT_NDESC	3		// number of GDT entries used, including null

// Pseudo-descriptors used for LGDT, LLDT and LIDT instructions.
struct pseudodesc
{
	uint16_t	pd_lim;		// Limit
	uint64_t	pd_base;	// Base - NOT 4-byte aligned!
};

// Per-CPU kernel state structure.
// Exactly one page (4096 bytes) in size.
struct cpu
{
	uint64_t	gdt[CPU_GDT_NDESC];
	cpu			*next;
	uint8_t		id;
	volatile uint32_t booted;
	uint32_t	magic;

	// Low end (growth limit) of the kernel stack.
	char		kstacklo[1];

	// High end (starting point) of the kernel stack.
	char		kstackhi[0] __attribute__((aligned(4096)));
};

#define CPU_MAGIC	0x98765432	// cpu.magic should always = this

// We have one statically-allocated cpu struct representing the boot CPU;
// others get chained onto this via cpu_boot.next as we find them.
extern cpu* g_cpu_boot;
extern cpu cpu_boot_template;

// Find the CPU struct representing the current CPU.
// It always resides at the bottom of the page containing the CPU's stack.
static inline uint64_t
read_rsp(void)
{
		uint64_t rsp;
		asm volatile( "mov %%rsp, %0" : "=r"(rsp) );
		return rsp;
}

static inline cpu *
cpu_cur() {
	cpu *c = (cpu*)(read_rsp() & ~0xFFF);
	assert(c->magic == CPU_MAGIC);
	return c;
}

// Returns true if we're running on the bootstrap CPU.
static inline int
cpu_onboot() {
	return cpu_cur() == g_cpu_boot;
}

// Set up the current CPU's private register state such as GDT and TSS.
// Assumes the cpu struct for this CPU is basically initialized
// and that we're running on the cpu's correct kernel stack.
void cpu_init(void);

// Allocate an additional cpu struct representing a non-bootstrap processor,
// and chain it onto the list of all CPUs.
cpu *cpu_alloc(void);

// Get any additional processors booted up and running.
void cpu_bootothers(uint64_t cr3);

#endif // _cpu_h_
