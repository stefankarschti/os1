#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <span>

#include "bootinfo.h"
#include "display.h"
#include "console_input.h"
#include "terminal.h"
#include "interrupt.h"
#include "memory.h"
#include "memory_layout.h"
#include "task.h"
#include "pageframe.h"
#include "virtualmemory.h"
#include "keyboard.h"
#include "debug.h"
#include "syscall_abi.h"

#include "cpu.h"
#include "mp.h"
#include "ioapic.h"
#include "lapic.h"
#include "pic.h"
#include "platform.h"

Interrupts interrupts;
PageFrameContainer page_frames;
Keyboard keyboard(interrupts);

const size_t kNumTerminals = 12;
Terminal terminal[kNumTerminals];
Terminal *active_terminal = nullptr;

namespace
{
const BootInfo *g_boot_info = nullptr;
uint64_t g_kernel_root_cr3 = 0;
VgaTextDisplay g_vga_text_display;
FramebufferTextDisplay g_framebuffer_text_display;
TextDisplayBackend g_vga_backend{TextDisplayBackendKind::VgaText, &g_vga_text_display};
TextDisplayBackend g_framebuffer_backend{TextDisplayBackendKind::FramebufferText, &g_framebuffer_text_display};
TextDisplayBackend *g_text_display = nullptr;

constexpr uint32_t kElfMagic = 0x464C457F;
constexpr uint16_t kElfTypeExec = 2;
constexpr uint16_t kElfMachineX86_64 = 62;
constexpr uint32_t kProgramTypeLoad = 1;
constexpr uint32_t kProgramFlagExecute = 0x1;
constexpr uint32_t kProgramFlagWrite = 0x2;

struct CpioNewcHeader
{
	char magic[6];
	char inode[8];
	char mode[8];
	char uid[8];
	char gid[8];
	char nlink[8];
	char mtime[8];
	char filesize[8];
	char devmajor[8];
	char devminor[8];
	char rdevmajor[8];
	char rdevminor[8];
	char namesize[8];
	char check[8];
} __attribute__((packed));

struct Elf64Header
{
	uint32_t magic;
	uint8_t ident[12];
	uint16_t type;
	uint16_t machine;
	uint32_t version;
	uint64_t entry;
	uint64_t phoff;
	uint64_t shoff;
	uint32_t flags;
	uint16_t ehsize;
	uint16_t phentsize;
	uint16_t phnum;
	uint16_t shentsize;
	uint16_t shnum;
	uint16_t shstrndx;
} __attribute__((packed));

struct Elf64ProgramHeader
{
	uint32_t type;
	uint32_t flags;
	uint64_t offset;
	uint64_t vaddr;
	uint64_t paddr;
	uint64_t filesz;
	uint64_t memsz;
	uint64_t align;
} __attribute__((packed));

[[nodiscard]] uint64_t AlignDown(uint64_t value, uint64_t alignment)
{
	return value & ~(alignment - 1);
}

[[nodiscard]] uint64_t AlignUp(uint64_t value, uint64_t alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}

[[nodiscard]] uint64_t BootFramebufferLengthBytes(const BootFramebufferInfo &framebuffer)
{
	return (uint64_t)framebuffer.pitch_bytes * (uint64_t)framebuffer.height;
}

[[nodiscard]] uint64_t ReadCr2()
{
	uint64_t value = 0;
	asm volatile("mov %%cr2, %0" : "=r"(value));
	return value;
}

[[nodiscard]] uint64_t ReadCr3()
{
	uint64_t value = 0;
	asm volatile("mov %%cr3, %0" : "=r"(value));
	return value;
}

[[noreturn]] void HaltForever()
{
	for(;;)
	{
		asm volatile("cli");
		asm volatile("hlt");
	}
}

void ReserveTrackedPhysicalRange(uint64_t physical_start, uint64_t length)
{
	if((0 == length) || (physical_start >= page_frames.MemoryEnd()))
	{
		return;
	}

	const uint64_t clamped_end = ((physical_start + length) < page_frames.MemoryEnd())
		? (physical_start + length)
		: page_frames.MemoryEnd();
	if(clamped_end > physical_start)
	{
		page_frames.ReserveRange(physical_start, clamped_end - physical_start);
	}
}

bool MapIdentityRange(VirtualMemory &vm, uint64_t physical_start, uint64_t length)
{
	if((0 == physical_start) || (0 == length))
	{
		return true;
	}

	const uint64_t start = AlignDown(physical_start, kPageSize);
	const uint64_t end = AlignUp(physical_start + length, kPageSize);
	return vm.MapPhysical(start,
			start,
			(end - start) / kPageSize,
			PageFlags::Present | PageFlags::Write);
}

TextDisplayBackend *SelectTextDisplay(const BootInfo &boot_info)
{
	if(BootSource::BiosLegacy == boot_info.source)
	{
		debug("console backend: vga")();
		return &g_vga_backend;
	}

	if(InitializeFramebufferTextDisplay(g_framebuffer_text_display, boot_info.framebuffer))
	{
		debug("framebuffer console active")();
		debug("framebuffer ")(boot_info.framebuffer.width)
			("x")(boot_info.framebuffer.height)
			(" pitch ")(boot_info.framebuffer.pitch_bytes)
			(" bpp ")(boot_info.framebuffer.bits_per_pixel)
			(" format ")(BootFramebufferPixelFormatName(boot_info.framebuffer.pixel_format))();
		return &g_framebuffer_backend;
	}

	if(0 != boot_info.framebuffer.physical_address)
	{
		debug("framebuffer console unavailable")();
	}
	else
	{
		debug("console backend: serial-only")();
	}

	return nullptr;
}

void WriteConsoleBytes(const char *data, size_t length)
{
	if(nullptr == data)
	{
		return;
	}
	for(size_t i = 0; i < length; ++i)
	{
		debug.Write(data[i]);
		if(active_terminal)
		{
			active_terminal->Write(data[i]);
		}
	}
}

void WriteConsoleLine(const char *text)
{
	if(nullptr == text)
	{
		return;
	}
	debug.WriteLn(text);
	if(active_terminal)
	{
		active_terminal->WriteLn(text);
	}
}

void DumpTrapFrame(const TrapFrame &frame)
{
	debug("vector=")(frame.vector)(" error=0x")(frame.error_code, 16)
		(" rip=0x")(frame.rip, 16)
		(" cs=0x")(frame.cs, 16)
		(" rsp=0x")(frame.rsp, 16)
		(" ss=0x")(frame.ss, 16)
		(" rflags=0x")(frame.rflags, 16)();
	debug("rax=0x")(frame.rax, 16)(" rbx=0x")(frame.rbx, 16)
		(" rcx=0x")(frame.rcx, 16)(" rdx=0x")(frame.rdx, 16)();
	debug("rsi=0x")(frame.rsi, 16)(" rdi=0x")(frame.rdi, 16)
		(" rbp=0x")(frame.rbp, 16)();
	debug("r8=0x")(frame.r8, 16)(" r9=0x")(frame.r9, 16)
		(" r10=0x")(frame.r10, 16)(" r11=0x")(frame.r11, 16)();
	debug("r12=0x")(frame.r12, 16)(" r13=0x")(frame.r13, 16)
		(" r14=0x")(frame.r14, 16)(" r15=0x")(frame.r15, 16)();
}

void AcknowledgeLegacyIrq(int irq)
{
	lapic_eoi();
	outb(0x20, 0x20);
	if(irq >= 8)
	{
		outb(0xA0, 0x20);
	}
}

uint16_t SetTimer(uint16_t frequency)
{
	uint32_t divisor = 1193180 / frequency;
	if(divisor > 65536)
	{
		divisor = 65536;
	}
	outb(0x43, 0x34);
	outb(0x40, divisor & 0xFF);
	outb(0x40, (divisor >> 8) & 0xFF);
	return 1193180 / divisor;
}

void KernelIdleThread()
{
	static bool announced = false;
	if(!announced)
	{
		announced = true;
		WriteConsoleLine("idle thread online");
	}
	for(;;)
	{
		// M2 has no wakeup-driven kernel work once the initrd self-tests finish,
		// so the BSP can sleep until the next interrupt hands control back to the
		// scheduler.
		asm volatile("sti; hlt");
	}
}

[[nodiscard]] Thread *ScheduleNext(bool keep_current)
{
	reapDeadThreads(page_frames);
	Thread *current = currentThread();
	if(keep_current && current)
	{
		markThreadReady(current);
	}

	Thread *next = nextRunnableThread(current);
	if(nullptr == next)
	{
		next = idleThread();
	}
	return next;
}

const char *KernelFaultName(uint64_t vector)
{
	switch(vector)
	{
	case T_DIVIDE: return "#DE divide error";
	case T_DEBUG: return "#DB debug";
	case T_NMI: return "NMI";
	case T_BRKPT: return "#BP breakpoint";
	case T_OFLOW: return "#OF overflow";
	case T_BOUND: return "#BR bound range";
	case T_ILLOP: return "#UD invalid opcode";
	case T_DEVICE: return "#NM device not available";
	case T_DBLFLT: return "#DF double fault";
	case T_TSS: return "#TS invalid TSS";
	case T_SEGNP: return "#NP segment not present";
	case T_STACK: return "#SS stack fault";
	case T_GPFLT: return "#GP general protection";
	case T_PGFLT: return "#PF page fault";
	case T_FPERR: return "#MF floating point";
	case T_ALIGN: return "#AC alignment";
	case T_MCHK: return "#MC machine check";
	case T_SIMD: return "#XF SIMD";
	case T_SECEV: return "#SX security";
	default: return "unknown trap";
	}
}

void OnKernelException(TrapFrame *frame)
{
	const char *name = KernelFaultName(frame->vector);
	if(active_terminal)
	{
		active_terminal->WriteLn(name);
	}
	debug(name)(" cr2=0x")(ReadCr2(), 16)(" cr3=0x")(ReadCr3(), 16)();
	DumpTrapFrame(*frame);
	HaltForever();
}

const char *NormalizeArchivePath(const char *path)
{
	if(nullptr == path)
	{
		return nullptr;
	}
	while('.' == path[0] && '/' == path[1])
	{
		path += 2;
	}
	while('/' == *path)
	{
		++path;
	}
	return path;
}

bool PathsEqual(const char *archive_name, const char *wanted)
{
	const char *normalized_archive = NormalizeArchivePath(archive_name);
	const char *normalized_wanted = NormalizeArchivePath(wanted);
	if((nullptr == normalized_archive) || (nullptr == normalized_wanted))
	{
		return false;
	}
	size_t index = 0;
	for(;; ++index)
	{
		if(normalized_archive[index] != normalized_wanted[index])
		{
			return false;
		}
		if(0 == normalized_archive[index])
		{
			return true;
		}
	}
}

uint32_t ParseHex(const char *text, size_t digits)
{
	uint32_t value = 0;
	for(size_t i = 0; i < digits; ++i)
	{
		value <<= 4;
		if((text[i] >= '0') && (text[i] <= '9'))
		{
			value |= (uint32_t)(text[i] - '0');
		}
		else if((text[i] >= 'a') && (text[i] <= 'f'))
		{
			value |= (uint32_t)(text[i] - 'a' + 10);
		}
		else if((text[i] >= 'A') && (text[i] <= 'F'))
		{
			value |= (uint32_t)(text[i] - 'A' + 10);
		}
	}
	return value;
}

bool FindInitrdFile(const char *path, const uint8_t *&data, uint64_t &size)
{
	data = nullptr;
	size = 0;
	if((nullptr == g_boot_info) || (0 == g_boot_info->module_count))
	{
		return false;
	}

	const BootModuleInfo &module = g_boot_info->modules[0];
	const uint8_t *cursor = (const uint8_t*)module.physical_start;
	const uint8_t *end = cursor + module.length;
	while((cursor + sizeof(CpioNewcHeader)) <= end)
	{
		const CpioNewcHeader *header = (const CpioNewcHeader*)cursor;
		bool magic_ok = true;
		for(size_t i = 0; i < 6; ++i)
		{
			if(header->magic[i] != "070701"[i])
			{
				magic_ok = false;
				break;
			}
		}
		if(!magic_ok)
		{
			return false;
		}

		const uint32_t name_size = ParseHex(header->namesize, 8);
		const uint32_t file_size = ParseHex(header->filesize, 8);
		const char *name = (const char*)(cursor + sizeof(CpioNewcHeader));
		const uint8_t *file_data = (const uint8_t*)AlignUp(
			(uint64_t)(cursor + sizeof(CpioNewcHeader) + name_size), 4);
		if((const uint8_t*)name > end || (file_data + file_size) > end)
		{
			return false;
		}

		if(PathsEqual(name, "TRAILER!!!"))
		{
			return false;
		}

		if(PathsEqual(name, path))
		{
			data = file_data;
			size = file_size;
			return true;
		}

		cursor = (const uint8_t*)AlignUp((uint64_t)(file_data + file_size), 4);
	}
	return false;
}

bool CopyIntoAddressSpace(VirtualMemory &vm, uint64_t virtual_address, const uint8_t *source, uint64_t length)
{
	uint64_t copied = 0;
	while(copied < length)
	{
		uint64_t physical = 0;
		uint64_t flags = 0;
		if(!vm.Translate(virtual_address + copied, physical, flags))
		{
			return false;
		}

		const uint64_t page_offset = (virtual_address + copied) & (kPageSize - 1);
		const uint64_t chunk = ((length - copied) < (kPageSize - page_offset))
			? (length - copied)
			: (kPageSize - page_offset);
		memcpy((void*)physical, source + copied, chunk);
		copied += chunk;
	}
	return true;
}

bool CopyToUser(const Thread *thread, uint64_t user_pointer, const void *source, size_t length)
{
	if((nullptr == thread) || (nullptr == source))
	{
		return false;
	}

	VirtualMemory vm(page_frames, thread->address_space_cr3);
	const uint8_t *src = (const uint8_t*)source;
	size_t copied = 0;
	while(copied < length)
	{
		uint64_t physical = 0;
		uint64_t flags = 0;
		if(!vm.Translate(user_pointer + copied, physical, flags))
		{
			return false;
		}
		const size_t page_offset = (user_pointer + copied) & (kPageSize - 1);
		const size_t chunk = ((length - copied) < (kPageSize - page_offset))
			? (length - copied)
			: (kPageSize - page_offset);
		memcpy((void*)physical, src + copied, chunk);
		copied += chunk;
	}
	return true;
}

bool DestroyUserAddressSpace(uint64_t cr3)
{
	if(0 == cr3)
	{
		return false;
	}
	VirtualMemory vm(page_frames, cr3);
	vm.DestroyUserSlot(kUserPml4Index);
	uint64_t *pml4 = (uint64_t*)cr3;
	pml4[0] = 0;
	return page_frames.Free(cr3);
}

bool LoadUserElf(const uint8_t *image, uint64_t image_size, uint64_t &cr3, uint64_t &entry, uint64_t &user_rsp)
{
	if((nullptr == image) || (image_size < sizeof(Elf64Header)))
	{
		return false;
	}

	const Elf64Header *header = (const Elf64Header*)image;
	if((header->magic != kElfMagic)
		|| (header->type != kElfTypeExec)
		|| (header->machine != kElfMachineX86_64)
		|| (header->phoff >= image_size)
		|| (header->phentsize != sizeof(Elf64ProgramHeader)))
	{
		return false;
	}

	VirtualMemory vm(page_frames);
	if(!vm.CloneKernelPml4Entry(0, g_kernel_root_cr3))
	{
		return false;
	}

	const uint64_t stack_guard_base = kUserStackTop - (kUserStackPages + 1) * kPageSize;
	for(uint16_t i = 0; i < header->phnum; ++i)
	{
		const uint64_t ph_offset = header->phoff + i * sizeof(Elf64ProgramHeader);
		if((ph_offset + sizeof(Elf64ProgramHeader)) > image_size)
		{
			DestroyUserAddressSpace(vm.Root());
			return false;
		}

		const Elf64ProgramHeader *program = (const Elf64ProgramHeader*)(image + ph_offset);
		if(kProgramTypeLoad != program->type)
		{
			continue;
		}
		if((program->memsz < program->filesz)
			|| ((program->offset + program->filesz) > image_size))
		{
			DestroyUserAddressSpace(vm.Root());
			return false;
		}

		const uint64_t segment_start = AlignDown(program->vaddr, kPageSize);
		const uint64_t segment_end = AlignUp(program->vaddr + program->memsz, kPageSize);
		if((segment_start < kUserSpaceBase)
			|| (segment_end > stack_guard_base)
			|| ((segment_start >> 39) & 0x1FFull) != kUserPml4Index)
		{
			DestroyUserAddressSpace(vm.Root());
			return false;
		}

		PageFlags page_flags = PageFlags::Present | PageFlags::User;
		if(program->flags & kProgramFlagWrite)
		{
			page_flags |= PageFlags::Write;
		}
		if(0 == (program->flags & kProgramFlagExecute))
		{
			page_flags |= PageFlags::NoExecute;
		}

		if(!vm.AllocateAndMap(segment_start, (segment_end - segment_start) / kPageSize, page_flags))
		{
			DestroyUserAddressSpace(vm.Root());
			return false;
		}

		if(!CopyIntoAddressSpace(vm, program->vaddr, image + program->offset, program->filesz))
		{
			DestroyUserAddressSpace(vm.Root());
			return false;
		}
	}

	const uint64_t user_stack_base = kUserStackTop - kUserStackPages * kPageSize;
	if(!vm.AllocateAndMap(user_stack_base, kUserStackPages,
		PageFlags::Present | PageFlags::Write | PageFlags::User | PageFlags::NoExecute))
	{
		DestroyUserAddressSpace(vm.Root());
		return false;
	}

	uint64_t stack_physical = 0;
	uint64_t stack_flags = 0;
	if(!vm.Translate(kUserStackTop - 8, stack_physical, stack_flags))
	{
		debug("user stack translation missing at 0x")(kUserStackTop - 8, 16)();
		DestroyUserAddressSpace(vm.Root());
		return false;
	}
	cr3 = vm.Root();
	entry = header->entry;
	// Like kernel threads, first user entry reaches `_start` via `iretq`, so we
	// reserve one dummy slot to match the SysV function-entry stack shape.
	user_rsp = AlignDown(kUserStackTop, 16) - sizeof(uint64_t);
	return true;
}

Thread *LoadUserProgram(const char *path)
{
	const uint8_t *file_data = nullptr;
	uint64_t file_size = 0;
	if(!FindInitrdFile(path, file_data, file_size))
	{
		debug("initrd missing ")(path)();
		return nullptr;
	}

	uint64_t user_cr3 = 0;
	uint64_t entry = 0;
	uint64_t user_rsp = 0;
	if(!LoadUserElf(file_data, file_size, user_cr3, entry, user_rsp))
	{
		debug("user ELF load failed for ")(path)();
		return nullptr;
	}

	Process *process = createUserProcess(path, user_cr3);
	if(nullptr == process)
	{
		DestroyUserAddressSpace(user_cr3);
		return nullptr;
	}

	Thread *thread = createUserThread(process, entry, user_rsp, page_frames);
	if(nullptr == thread)
	{
		DestroyUserAddressSpace(user_cr3);
		return nullptr;
	}

	debug("user thread ready pid ")(process->pid)(" tid ")(thread->tid)(" entry 0x")(entry, 16)(" rsp 0x")(user_rsp, 16)();
	return thread;
}

bool CopyFromUser(const Thread *thread, uint64_t user_pointer, void *destination, size_t length)
{
	if((nullptr == thread) || (nullptr == destination))
	{
		return false;
	}

	VirtualMemory vm(page_frames, thread->address_space_cr3);
	uint8_t *dest = (uint8_t*)destination;
	size_t copied = 0;
	while(copied < length)
	{
		uint64_t physical = 0;
		uint64_t flags = 0;
		if(!vm.Translate(user_pointer + copied, physical, flags))
		{
			return false;
		}
		const size_t page_offset = (user_pointer + copied) & (kPageSize - 1);
		const size_t chunk = ((length - copied) < (kPageSize - page_offset))
			? (length - copied)
			: (kPageSize - page_offset);
		memcpy(dest + copied, (const void*)physical, chunk);
		copied += chunk;
	}
	return true;
}

bool TryCompleteConsoleRead(Thread *thread, uint64_t user_buffer, size_t length, long &result)
{
	result = -1;
	if(nullptr == thread)
	{
		return true;
	}
	if((0 == user_buffer) || (0 == length))
	{
		return true;
	}
	if(!ConsoleInputHasLine())
	{
		return false;
	}

	char line[kConsoleInputMaxLineBytes];
	size_t line_length = 0;
	if(!ConsoleInputPopLine(line, sizeof(line), line_length))
	{
		return false;
	}
	if((line_length > length) || !CopyToUser(thread, user_buffer, line, line_length))
	{
		return true;
	}

	result = (long)line_length;
	return true;
}

void WakeConsoleReaders()
{
	while(ConsoleInputHasLine())
	{
		Thread *thread = firstBlockedThread(ThreadWaitReason::ConsoleRead);
		if(nullptr == thread)
		{
			return;
		}

		long result = -1;
		if(!TryCompleteConsoleRead(thread, thread->wait_address, (size_t)thread->wait_length, result))
		{
			return;
		}

		clearThreadWait(thread);
		thread->frame.rax = (uint64_t)result;
		markThreadReady(thread);
	}
}

long SysWrite(int fd, uint64_t user_buffer, size_t length)
{
	if((fd != 1) && (fd != 2))
	{
		return -1;
	}

	Thread *thread = currentThread();
	if(nullptr == thread)
	{
		return -1;
	}

	char buffer[128];
	size_t written = 0;
	while(written < length)
	{
		const size_t chunk = ((length - written) < sizeof(buffer))
			? (length - written)
			: sizeof(buffer);
		if(!CopyFromUser(thread, user_buffer + written, buffer, chunk))
		{
			return -1;
		}
		WriteConsoleBytes(buffer, chunk);
		written += chunk;
	}
	return (long)written;
}

Thread *HandleSyscall(TrapFrame *frame)
{
	Thread *thread = currentThread();
	if(nullptr == thread)
	{
		return nullptr;
	}

		switch(frame->rax)
	{
	case SYS_write:
		frame->rax = (uint64_t)SysWrite((int)frame->rdi, frame->rsi, (size_t)frame->rdx);
		return thread;
	case SYS_read:
	{
		long read_result = -1;
		if((int)frame->rdi != 0)
		{
			frame->rax = (uint64_t)-1;
			return thread;
		}
		if(TryCompleteConsoleRead(thread, frame->rsi, (size_t)frame->rdx, read_result))
		{
			frame->rax = (uint64_t)read_result;
			return thread;
		}

		blockCurrentThread(ThreadWaitReason::ConsoleRead, frame->rsi, frame->rdx);
		return ScheduleNext(false);
	}
	case SYS_exit:
		markCurrentThreadDying((int)frame->rdi);
		return ScheduleNext(false);
	case SYS_yield:
		return ScheduleNext(true);
	case SYS_getpid:
		frame->rax = thread->process ? thread->process->pid : 0;
		return thread;
	default:
		frame->rax = (uint64_t)-1;
		return thread;
	}
}

Thread *HandleIrq(TrapFrame *frame)
{
	const int irq = (int)(frame->vector - T_IRQ0);
	if(IRQ_KBD == irq)
	{
		DispatchIRQHook(irq);
	}
	else if(IRQ_TIMER == irq)
	{
		ConsoleInputPollSerial();
	}

	AcknowledgeLegacyIrq(irq);
	WakeConsoleReaders();

	if(nullptr == currentThread())
	{
		return nullptr;
	}

	if(IRQ_TIMER == irq)
	{
		return ScheduleNext(true);
	}

	reapDeadThreads(page_frames);
	if((currentThread() == idleThread()) && (nullptr != firstRunnableUserThread()))
	{
		return ScheduleNext(true);
	}
	return currentThread();
}

Thread *HandleException(TrapFrame *frame)
{
	if(TrapFrameIsUser(*frame))
	{
		const uint64_t pid = currentThread() && currentThread()->process
			? currentThread()->process->pid
			: 0;
		debug("user trap vector ")(frame->vector)(" pid ")(pid)
			(" cr2 0x")(ReadCr2(), 16)
			(" error 0x")(frame->error_code, 16)
			(" cr3 0x")(ReadCr3(), 16)();
		if(frame->vector == T_PGFLT)
		{
			debug("user page fault killed pid ")(pid)();
		}
		else
		{
			debug("user exception killed pid ")(pid)();
		}
		markCurrentThreadDying((int)frame->vector);
		return ScheduleNext(false);
	}

	DispatchExceptionHandler((int)frame->vector, frame);
	OnKernelException(frame);
	return nullptr;
}
}

bool KernelKeyboardHook(uint16_t scancode)
{
	uint16_t hotkey[kNumTerminals] = {0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x57, 0x58};
	int index = -1;
	for(unsigned i = 0; i < kNumTerminals; i++)
	{
		if(scancode == hotkey[i])
		{
			index = i;
			break;
		}
	}
	if(index >= 0)
	{
		if(active_terminal != &terminal[index])
		{
		if(active_terminal)
		{
			active_terminal->Unlink();
		}
		active_terminal = &terminal[index];
		active_terminal->Link(g_text_display);
		keyboard.SetActiveTerminal(active_terminal);
	}
	}
	return true;
}

extern "C" Thread *trap_dispatch(TrapFrame *frame)
{
	if((nullptr == frame) || (frame->vector > 255))
	{
		return nullptr;
	}

	if((frame->vector >= T_IRQ0) && (frame->vector < (T_IRQ0 + 16)))
	{
		return HandleIrq(frame);
	}
	if(frame->vector == T_SYSCALL)
	{
		return HandleSyscall(frame);
	}
	return HandleException(frame);
}

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
			ReserveTrackedPhysicalRange(g_boot_info->modules[i].physical_start, g_boot_info->modules[i].length);
		}
		debug("initrd module discovered")();
	}
	if(0 != g_boot_info->framebuffer.physical_address)
	{
		ReserveTrackedPhysicalRange(g_boot_info->framebuffer.physical_address,
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

	Thread *init_thread = LoadUserProgram("/bin/init");
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
