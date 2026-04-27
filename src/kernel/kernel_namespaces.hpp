// Namespaced compatibility facade for kernel C++ APIs during the ABI-sensitive
// migration from historical global symbols to os1::kernel ownership namespaces.
#pragma once

#include "arch/x86_64/cpu/control_regs.hpp"
#include "arch/x86_64/interrupt/interrupt.hpp"
#include "console/console.hpp"
#include "console/console_input.hpp"
#include "console/terminal.hpp"
#include "debug/debug.hpp"
#include "drivers/display/text_display.hpp"
#include "drivers/input/ps2_keyboard.hpp"
#include "fs/initrd.hpp"
#include "handoff/boot_info.hpp"
#include "mm/boot_mapping.hpp"
#include "mm/boot_reserve.hpp"
#include "mm/page_frame.hpp"
#include "mm/user_copy.hpp"
#include "mm/virtual_memory.hpp"
#include "platform/platform.hpp"
#include "proc/process.hpp"
#include "proc/thread.hpp"
#include "sched/scheduler.hpp"
#include "storage/block_device.hpp"
#include "syscall/dispatch.hpp"
#include "syscall/observe.hpp"
#include "syscall/process.hpp"
#include "syscall/wait.hpp"

namespace os1::kernel::handoff {
using ::BootFramebufferInfo;
using ::BootFramebufferPixelFormat;
using ::BootInfo;
using ::BootMemoryRegion;
using ::BootMemoryType;
using ::BootModuleInfo;
using ::BootSource;
using ::BootTextConsoleInfo;
using ::boot_framebuffer_pixel_format_name;
using ::boot_memory_region_is_usable;
using ::boot_memory_regions;
using ::boot_source_name;
using ::own_boot_info;
}  // namespace os1::kernel::handoff

namespace os1::kernel::mm {
using ::PageFlags;
using ::PageFrameContainer;
using ::VirtualMemory;
using ::copy_from_user;
using ::copy_into_address_space;
using ::copy_to_user;
using ::copy_user_string;
using ::map_identity_range;
using ::reserve_tracked_physical_range;
}  // namespace os1::kernel::mm

namespace os1::kernel::console {
using ::Terminal;
using ::console_input_has_line;
using ::console_input_initialize;
using ::console_input_on_keyboard_char;
using ::console_input_poll_serial;
using ::console_input_pop_line;
using ::write_console_bytes;
using ::write_console_line;
}  // namespace os1::kernel::console

namespace os1::kernel::debug {
using ::Debug;
using ::debug_memory;
}  // namespace os1::kernel::debug

namespace os1::kernel::drivers::display {
using ::FramebufferTextDisplay;
using ::TextDisplayBackend;
using ::TextDisplayBackendKind;
using ::VgaTextDisplay;
using ::detach_text_display;
using ::initialize_framebuffer_text_display;
using ::present_text_display;
}  // namespace os1::kernel::drivers::display

namespace os1::kernel::drivers::input {
using ::Keyboard;
}  // namespace os1::kernel::drivers::input

namespace os1::kernel::fs {
using ::InitrdFileVisitor;
using ::bind_initrd_boot_info;
using ::copy_initrd_path;
using ::find_initrd_file;
using ::for_each_initrd_file;
}  // namespace os1::kernel::fs

namespace os1::kernel::platform {
using ::BlockDevice;
using ::CpuInfo;
using ::InterruptOverride;
using ::IoApicInfo;
using ::PciBarInfo;
using ::PciBarType;
using ::PciDevice;
using ::PciEcamRegion;
using ::VirtioBlkDevice;
using ::platform_block_device;
using ::platform_enable_isa_irq;
using ::platform_init;
using ::platform_pci_device_count;
using ::platform_pci_devices;
using ::platform_virtio_blk;
}  // namespace os1::kernel::platform

namespace os1::kernel::proc {
using ::AddressSpace;
using ::Process;
using ::ProcessState;
using ::Thread;
using ::ThreadState;
using ::ThreadWaitReason;
using ::block_current_thread;
using ::clear_thread;
using ::clear_thread_wait;
using ::create_kernel_thread;
using ::create_user_thread;
using ::current_thread;
using ::first_blocked_thread;
using ::first_runnable_user_thread;
using ::idle_thread;
using ::init_tasks;
using ::mark_current_thread_dying;
using ::mark_thread_ready;
using ::next_runnable_thread;
using ::process_has_threads;
using ::runnable_thread_count;
using ::reap_dead_threads;
using ::reap_process;
using ::relink_runnable_threads;
using ::set_current_thread;
}  // namespace os1::kernel::proc

namespace os1::kernel::sched {
using ::schedule_next;
}  // namespace os1::kernel::sched

namespace os1::kernel::syscall {
using ::ObserveContext;
using ::ProcessSyscallContext;
using ::handle_syscall;
using ::sys_exec;
using ::sys_observe;
using ::sys_spawn;
using ::sys_write;
using ::try_complete_wait_pid;
using ::wake_child_waiters;
}  // namespace os1::kernel::syscall

namespace os1::kernel::arch::x86_64::cpu {
using ::read_cr2;
using ::read_cr3;
using ::write_cr3;
}  // namespace os1::kernel::arch::x86_64::cpu

namespace os1::kernel::arch::x86_64::interrupt {
using ::ExceptionHandler;
using ::IDTDescriptor;
using ::Interrupts;
using ::TrapFrame;
using ::dispatch_exception_handler;
using ::dispatch_irq_hook;
using ::trap_frame_is_user;
}  // namespace os1::kernel::arch::x86_64::interrupt
