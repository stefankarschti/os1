#include "arch/x86_64/cpu/cpu.hpp"

#include "arch/x86_64/apic/ioapic.hpp"
#include "arch/x86_64/apic/lapic.hpp"
#include "arch/x86_64/cpu/x86.hpp"
#include "handoff/memory_layout.h"
#include "mm/page_frame.hpp"
#include "util/memory.h"

namespace
{
[[noreturn]] void cpu_idle_loop()
{
    // Secondary CPUs are online in M2, but BSP-only scheduling keeps them out of
    // the live thread runtime for now. They remain in an interrupt-disabled idle
    // loop until later milestones give them a full scheduler path and final IDT.
    for(;;)
    {
        asm volatile("cli");
        asm volatile("hlt");
    }
}

void set_tss_descriptor(cpu* c)
{
    const uint64_t base = (uint64_t)&c->tss;
    const uint64_t limit = sizeof(Tss64) - 1;

    const uint64_t low = (limit & 0xFFFFull) | ((base & 0xFFFFFFull) << 16) | (0x89ull << 40) |
                         (((limit >> 16) & 0xFull) << 48) | (((base >> 24) & 0xFFull) << 56);
    const uint64_t high = (base >> 32) & 0xFFFFFFFFull;

    c->gdt[CPU_GDT_TSS >> 3] = low;
    c->gdt[(CPU_GDT_TSS >> 3) + 1] = high;
}
}  // namespace

cpu cpu_boot_template = {
    self : nullptr,
    current_thread : nullptr,
    interrupt_frame : {},
    gdt : {
        [0] = SEGDESC_NULL,
        [CPU_GDT_KCODE >> 3] = SEGDESC64_CODE(0),
        [CPU_GDT_KDATA >> 3] = SEGDESC64_DATA(0),
        [CPU_GDT_UDATA >> 3] = SEGDESC64_DATA(3),
        [CPU_GDT_UCODE >> 3] = SEGDESC64_CODE(3),
        [CPU_GDT_TSS >> 3] = 0,
        [(CPU_GDT_TSS >> 3) + 1] = 0,
    },
    tss : {},
    next : nullptr,
    id : 0,
    booted : 0,
    magic : CPU_MAGIC
};

cpu* g_cpu_boot = nullptr;

void cpu_set_kernel_stack(uint64_t stack_top)
{
    cpu_cur()->tss.rsp0 = stack_top;
}

void cpu_init()
{
    cpu* c = cpu_cur();
    c->self = c;
    c->tss.io_bitmap_base = sizeof(Tss64);
    if(0 == c->tss.rsp0)
    {
        c->tss.rsp0 = (uint64_t)c->kstackhi;
    }
    set_tss_descriptor(c);

    struct
    {
        uint16_t length;
        void* base;
    } __attribute__((packed)) gdtr = {sizeof(c->gdt) - 1, c->gdt};

    asm volatile("lgdt %0" : : "m"(gdtr));

    asm volatile("movw %%ax,%%gs" ::"a"(CPU_GDT_KDATA));
    asm volatile("movw %%ax,%%fs" ::"a"(CPU_GDT_KDATA));
    asm volatile("movw %%ax,%%es" ::"a"(CPU_GDT_KDATA));
    asm volatile("movw %%ax,%%ds" ::"a"(CPU_GDT_KDATA));
    asm volatile("movw %%ax,%%ss" ::"a"(CPU_GDT_KDATA));
    asm volatile("jmp 1f\n1:\n");
    wrmsr(0xC0000101, (uint64_t)c);

    asm volatile("lldt %%ax" ::"a"(0));
    ltr(CPU_GDT_TSS);
}

extern PageFrameContainer page_frames;
cpu* cpu_alloc(void)
{
    cpu* last_cpu = g_cpu_boot;
    while(last_cpu->next)
    {
        last_cpu = last_cpu->next;
    }

    uint64_t page = 0;
    if(!page_frames.allocate(page, 1))
    {
        debug("alloc cpu failed")();
        return nullptr;
    }

    cpu* c = (cpu*)page;
    memset(c, 0, sizeof(cpu));
    memcpy(c,
           &cpu_boot_template,
           ((uint8_t*)&cpu_boot_template.kstacklo - (uint8_t*)&cpu_boot_template));
    c->self = c;
    c->magic = CPU_MAGIC;
    last_cpu->next = c;
    debug("alloc cpu at 0x")(page, 16)();
    return c;
}

void init()
{
    if(!cpu_onboot())
    {
        debug("CPU ")(cpu_cur()->id)(" is alive at 0x")((uint64_t)cpu_cur(), 16)();
        cpu_init();
        debug("cpu init worked!")();
        ioapic_init();
        lapic_init();
        xchg(&cpu_cur()->booted, 1);
    }

    cpu_idle_loop();
}

void cpu_bootothers(uint64_t cr3)
{
    debug("booting other CPUs cr3 = 0x")(cr3, 16)();
    debug("current CPU = ")(cpu_cur()->id)();
    if(!cpu_onboot())
    {
        xchg(&cpu_cur()->booted, 1);
        return;
    }

    extern uint8_t cpu_start_begin[];
    extern uint8_t cpu_start_end[];
    uint8_t* code = (uint8_t*)kApTrampolineAddress;
    memcpy(code, cpu_start_begin, (cpu_start_end - cpu_start_begin));

    for(cpu* c = g_cpu_boot; c; c = c->next)
    {
        if(c == cpu_cur())
        {
            continue;
        }

        *((uint64_t*)kApStartupCpuPageAddress) = (uint64_t)c;
        *((uint64_t*)kApStartupRipAddress) = (uint64_t)init;
        *((uint64_t*)kApStartupCr3Address) = cr3;
        memset((void*)kApStartupIdtAddress, 0, kApStartupIdtSizeBytes);

        debug("Starting CPU ")(c->id)();
        lapic_startcpu(c->id, (uint64_t)code);
        while(c->booted == 0)
        {
        }
        debug("done waiting")();
    }
}
