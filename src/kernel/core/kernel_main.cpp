// Shared kernel entry and bring-up sequencing. Boot-frontends normalize their
// native firmware/bootloader state into BootInfo, then both paths arrive here.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <span>

#include "arch/x86_64/apic/ioapic.hpp"
#include "arch/x86_64/apic/lapic.hpp"
#include "arch/x86_64/apic/pic.hpp"
#include "arch/x86_64/cpu/control_regs.hpp"
#include "arch/x86_64/cpu/cpu.hpp"
#include "arch/x86_64/cpu/x86.hpp"
#include "arch/x86_64/interrupt/interrupt.hpp"
#include "console/console.hpp"
#include "console/console_input.hpp"
#include "core/fault.hpp"
#include "core/kernel_state.hpp"
#include "core/panic.hpp"
#include "debug/debug.hpp"
#include "debug/event_ring.hpp"
#include "drivers/display/text_display.hpp"
#include "drivers/timer/pit.hpp"
#include "fs/initrd.hpp"
#include "handoff/boot_info.hpp"
#include "handoff/memory_layout.h"
#include "mm/boot_mapping.hpp"
#include "mm/boot_reserve.hpp"
#include "mm/virtual_memory.hpp"
#include "platform/platform.hpp"
#include "platform/topology.hpp"
#include "proc/thread.hpp"
#include "proc/user_program.hpp"
#include "sched/idle.hpp"
#include "util/align.hpp"
#include "util/memory.h"

extern "C"
{
extern uint8_t __kernel_text_start[];
extern uint8_t __kernel_text_end[];
extern uint8_t __kernel_rodata_start[];
extern uint8_t __kernel_rodata_end[];
extern uint8_t __kernel_data_start[];
extern uint8_t __kernel_bss_end[];
}

namespace
{
bool map_kernel_section(VirtualMemory& vm, uint8_t* start, uint8_t* end, PageFlags flags)
{
    if(start >= end)
    {
        return true;
    }

    const uint64_t virtual_start = align_down(reinterpret_cast<uint64_t>(start), kPageSize);
    const uint64_t virtual_end = align_up(reinterpret_cast<uint64_t>(end), kPageSize);
    const uint64_t physical_start = kernel_virt_to_phys(virtual_start);
    if(kInvalidPhysicalAddress == physical_start)
    {
        return false;
    }

    return vm.map_physical(
        virtual_start, physical_start, (virtual_end - virtual_start) / kPageSize, flags);
}

bool map_kernel_image(VirtualMemory& vm)
{
    return map_kernel_section(vm, __kernel_text_start, __kernel_text_end, PageFlags::Present) &&
           map_kernel_section(vm,
                              __kernel_rodata_start,
                              __kernel_rodata_end,
                              PageFlags::Present | PageFlags::NoExecute) &&
           map_kernel_section(vm,
                              __kernel_data_start,
                              __kernel_bss_end,
                              PageFlags::Present | PageFlags::Write | PageFlags::NoExecute);
}
}  // namespace

extern "C" void kernel_main(BootInfo* info, cpu* cpu_boot)
{
    bool result = false;
    debug("[kernel64] hello!\n");

    g_boot_info = own_boot_info(info);
    if(nullptr == g_boot_info)
    {
        debug("invalid boot info")();
        return;
    }
    bind_initrd_boot_info(g_boot_info);
    debug("boot source: ")(boot_source_name(g_boot_info->source))();
    if(g_boot_info->bootloader_name)
    {
        debug("bootloader: ")(g_boot_info->bootloader_name)();
    }

    const std::span<const BootMemoryRegion> memory_regions = boot_memory_regions(*g_boot_info);

    {
        g_cpu_boot = cpu_boot;
        cpu_initialize_record(g_cpu_boot);
        cpu_init();
    }

    debug("initializing page frame allocator")();
    result = page_frames.initialize(
        memory_regions, kPageFrameBitmapBaseAddress, kPageFrameBitmapQwordLimit);
    debug(result ? "Success" : "Failure")();
    if(!result)
    {
        return;
    }

    if(g_boot_info->module_count > 0)
    {
        for(uint32_t i = 0; i < g_boot_info->module_count; ++i)
        {
            reserve_tracked_physical_range(page_frames,
                                           g_boot_info->modules[i].physical_start,
                                           g_boot_info->modules[i].length);
        }
        debug("initrd module discovered")();
    }
    if(0 != g_boot_info->framebuffer.physical_address)
    {
        reserve_tracked_physical_range(page_frames,
                                       g_boot_info->framebuffer.physical_address,
                                       boot_framebuffer_length_bytes(g_boot_info->framebuffer));
    }

    VirtualMemory kvm(page_frames);
    debug("create kernel bootstrap page tables")();
    // Keep only the low bootstrap window that AP startup still executes from.
    result = map_bootstrap_identity_range(kvm, 0, kEarlyReservedPhysicalEnd);
    if(!result)
    {
        return;
    }
    if(!map_kernel_image(kvm))
    {
        return;
    }
    if(!map_direct_range(kvm, 0, page_frames.memory_end()))
    {
        return;
    }
    uint64_t bootstrap_stack_pointer = 0;
    asm volatile("mov %%rsp, %0" : "=r"(bootstrap_stack_pointer));
    // Both boot frontends still enter kernel_main on a low bootstrap stack, so
    // keep the current stack page mapped until the BSP later switches to a
    // steady-state kernel thread stack.
    if(!map_bootstrap_identity_range(
           kvm, align_down(bootstrap_stack_pointer, kPageSize), kPageSize))
    {
        return;
    }
    // Boot modules and the framebuffer may live in non-usable ranges on the
    // modern path, so map those explicit physical ranges into the direct map
    // before activating the kernel page tables.
    for(uint32_t i = 0; i < g_boot_info->module_count; ++i)
    {
        if(!map_direct_range(kvm,
                             g_boot_info->modules[i].physical_start,
                             g_boot_info->modules[i].length))
        {
            return;
        }
    }
    if(!map_direct_range(kvm,
                         g_boot_info->framebuffer.physical_address,
                         boot_framebuffer_length_bytes(g_boot_info->framebuffer)))
    {
        return;
    }
    if(!map_direct_range(kvm, g_boot_info->rsdp_physical, 64))
    {
        return;
    }
    kvm.activate();
    g_kernel_root_cr3 = kvm.root();
    g_kernel_direct_map_ready = true;
    page_frames.enable_direct_map_access();
    g_cpu_boot = kernel_physical_pointer<cpu>((uint64_t)g_cpu_boot);
    // `cpu_cur()` reads the self-pointer through GS, so rebind it before the
    // first direct-map `cpu_init()` reloads descriptor state.
    g_cpu_boot->self = g_cpu_boot;
    wrmsr(0xC0000101, (uint64_t)g_cpu_boot);
    g_cpu_boot->tss.rsp0 = 0;
    cpu_init();

    if(!platform_discover(*g_boot_info, kvm))
    {
        return;
    }
    debug("muliprocessor: ")(ismp ? "yes" : "no")();
    debug("ncpu: ")(ncpu)();

    pic_init();
    ioapic_init();
    lapic_init();
    cpu_boot_others(g_kernel_root_cr3);

    g_text_display = select_text_display(*g_boot_info);

    uint16_t terminal_columns = 80;
    uint16_t terminal_rows = 25;
    if((nullptr != g_text_display) &&
       (TextDisplayBackendKind::FramebufferText == g_text_display->kind) &&
       (nullptr != g_text_display->instance))
    {
        auto& framebuffer_display = *static_cast<FramebufferTextDisplay*>(g_text_display->instance);
        terminal_columns = framebuffer_display.columns;
        terminal_rows = framebuffer_display.rows;

        const uint64_t shadow_buffer_bytes =
            (uint64_t)terminal_columns * (uint64_t)terminal_rows * sizeof(uint16_t);
        const unsigned shadow_buffer_pages =
            (unsigned)(align_up(shadow_buffer_bytes, kPageSize) / kPageSize);
        uint64_t shadow_buffer = 0;
        if((0 == shadow_buffer_pages) || !page_frames.allocate(shadow_buffer, shadow_buffer_pages))
        {
            debug("framebuffer shadow allocation failed")();
            return;
        }

        framebuffer_display.shadow_buffer = kernel_physical_pointer<uint16_t>(shadow_buffer);
        framebuffer_display.shadow_cell_count =
            (uint32_t)terminal_columns * (uint32_t)terminal_rows;
        framebuffer_display.cursor_valid = false;
        memset(framebuffer_display.shadow_buffer, 0, shadow_buffer_pages * kPageSize);
    }

    const uint64_t terminal_buffer_bytes =
        (uint64_t)terminal_columns * (uint64_t)terminal_rows * sizeof(uint16_t);
    const unsigned terminal_buffer_pages =
        (unsigned)(align_up(terminal_buffer_bytes, kPageSize) / kPageSize);
    if(0 == terminal_buffer_pages)
    {
        debug("terminal buffer allocation size invalid")();
        return;
    }

    for(size_t i = 0; i < kNumTerminals; ++i)
    {
        uint64_t page = 0;
        if(!page_frames.allocate(page, terminal_buffer_pages))
        {
            debug("terminal buffer allocation failed")(" index=")(i)();
            return;
        }

        terminal[i].set_buffer(kernel_physical_pointer<uint16_t>(page), terminal_columns, terminal_rows);
        terminal[i].clear();
        terminal[i].write("Terminal ");
        terminal[i].write_int_line(i + 1);
    }

    active_terminal = &terminal[0];
    if(BootSource::BiosLegacy == g_boot_info->source)
    {
        active_terminal->copy(kernel_physical_pointer<uint16_t>(0xB8000));
    }
    else
    {
        active_terminal->clear();
    }
    active_terminal->link(g_text_display);
    if(BootSource::BiosLegacy == g_boot_info->source)
    {
        active_terminal->move_cursor(g_boot_info->text_console.cursor_y,
                                     g_boot_info->text_console.cursor_x);
    }
    else
    {
        active_terminal->move_cursor(0, 0);
    }
    active_terminal->write_line("[kernel64] hello");

    result = interrupts.initialize();
    debug(result ? "Interrupts initialization successful" : "Interrupts initialization failed")();
    if(!result)
    {
        return;
    }

    const uint8_t kernel_fault_vectors[] = {T_DIVIDE, T_DEBUG, T_NMI,    T_BRKPT,  T_OFLOW,
                                            T_BOUND,  T_ILLOP, T_DEVICE, T_DBLFLT, T_TSS,
                                            T_SEGNP,  T_STACK, T_GPFLT,  T_PGFLT,  T_FPERR,
                                            T_ALIGN,  T_MCHK,  T_SIMD,   29,       T_SECEV};
    for(size_t i = 0; i < sizeof(kernel_fault_vectors); ++i)
    {
        interrupts.set_exception_handler(kernel_fault_vectors[i], on_kernel_exception);
    }

    // Driver probing stays after the IDT and local interrupt controllers are
    // online so later MSI/MSI-X work does not need another boot-order split.
    if(!platform_probe_devices(kvm))
    {
        return;
    }

    keyboard.initialize();
    keyboard.set_active_terminal(active_terminal);
    console_input_initialize();
    if(ismp)
    {
        if(!platform_enable_isa_irq(IRQ_TIMER, IRQ_TIMER) || !platform_enable_isa_irq(IRQ_KBD))
        {
            return;
        }
    }

    if(!init_tasks(page_frames))
    {
        return;
    }

    Process* kernel_process = create_kernel_process(g_kernel_root_cr3);
    if(nullptr == kernel_process)
    {
        return;
    }
    if(nullptr == create_kernel_thread(kernel_process, kernel_idle_thread, page_frames))
    {
        return;
    }

    Thread* init_thread = load_user_program(page_frames, g_kernel_root_cr3, "/bin/init");
    if(nullptr == init_thread)
    {
        write_console_line("failed to load /bin/init");
        return;
    }

    kernel_event::record(OS1_KERNEL_EVENT_SMOKE_MARKER, 0, OS1_KERNEL_EVENT_SMOKE_MAGIC, 0, 0, 0);
    debug("start multitasking")();
    write_console_line("starting first user process");
    set_timer(1000);
    enter_first_thread(init_thread, init_thread->kernel_stack_top);
    halt_forever();
}
