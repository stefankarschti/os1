#include "proc/user_program.h"

#include "debug.h"
#include "fs/initrd.h"
#include "memory_layout.h"
#include "mm/user_copy.h"
#include "virtualmemory.h"

namespace
{
constexpr uint32_t kElfMagic = 0x464C457F;
constexpr uint16_t kElfTypeExec = 2;
constexpr uint16_t kElfMachineX86_64 = 62;
constexpr uint32_t kProgramTypeLoad = 1;
constexpr uint32_t kProgramFlagExecute = 0x1;
constexpr uint32_t kProgramFlagWrite = 0x2;

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

bool LoadUserElf(PageFrameContainer &frames,
		uint64_t kernel_root_cr3,
		const uint8_t *image,
		uint64_t image_size,
		uint64_t &cr3,
		uint64_t &entry,
		uint64_t &user_rsp)
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

	VirtualMemory vm(frames);
	if(!vm.CloneKernelPml4Entry(0, kernel_root_cr3))
	{
		return false;
	}

	const uint64_t stack_guard_base = kUserStackTop - (kUserStackPages + 1) * kPageSize;
	for(uint16_t i = 0; i < header->phnum; ++i)
	{
		const uint64_t ph_offset = header->phoff + i * sizeof(Elf64ProgramHeader);
		if((ph_offset + sizeof(Elf64ProgramHeader)) > image_size)
		{
			DestroyUserAddressSpace(frames, vm.Root());
			return false;
		}

		const Elf64ProgramHeader *program = (const Elf64ProgramHeader*)(image + ph_offset);
		if(kProgramTypeLoad != program->type)
		{
			continue;
		}
		if(0 == program->memsz)
		{
			continue;
		}
		if((program->memsz < program->filesz)
			|| ((program->offset + program->filesz) > image_size))
		{
			DestroyUserAddressSpace(frames, vm.Root());
			return false;
		}

		const uint64_t segment_start = AlignDown(program->vaddr, kPageSize);
		const uint64_t segment_end = AlignUp(program->vaddr + program->memsz, kPageSize);
		if((segment_start < kUserSpaceBase)
			|| (segment_end > stack_guard_base)
			|| (((segment_start >> 39) & 0x1FFull) != kUserPml4Index))
		{
			DestroyUserAddressSpace(frames, vm.Root());
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
			DestroyUserAddressSpace(frames, vm.Root());
			return false;
		}

		if(!CopyIntoAddressSpace(vm, program->vaddr, image + program->offset, program->filesz))
		{
			DestroyUserAddressSpace(frames, vm.Root());
			return false;
		}
	}

	const uint64_t user_stack_base = kUserStackTop - kUserStackPages * kPageSize;
	if(!vm.AllocateAndMap(user_stack_base, kUserStackPages,
		PageFlags::Present | PageFlags::Write | PageFlags::User | PageFlags::NoExecute))
	{
		DestroyUserAddressSpace(frames, vm.Root());
		return false;
	}

	uint64_t stack_physical = 0;
	uint64_t stack_flags = 0;
	if(!vm.Translate(kUserStackTop - 8, stack_physical, stack_flags))
	{
		debug("user stack translation missing at 0x")(kUserStackTop - 8, 16)();
		DestroyUserAddressSpace(frames, vm.Root());
		return false;
	}
	cr3 = vm.Root();
	entry = header->entry;
	// Like kernel threads, first user entry reaches `_start` via `iretq`, so we
	// reserve one dummy slot to match the SysV function-entry stack shape.
	user_rsp = AlignDown(kUserStackTop, 16) - sizeof(uint64_t);
	return true;
}
}

bool DestroyUserAddressSpace(PageFrameContainer &frames, uint64_t cr3)
{
	if(0 == cr3)
	{
		return false;
	}
	VirtualMemory vm(frames, cr3);
	vm.DestroyUserSlot(kUserPml4Index);
	uint64_t *pml4 = (uint64_t*)cr3;
	pml4[0] = 0;
	return frames.Free(cr3);
}

bool LoadUserProgramImage(PageFrameContainer &frames,
		uint64_t kernel_root_cr3,
		const char *path,
		uint64_t &user_cr3,
		uint64_t &entry,
		uint64_t &user_rsp)
{
	const uint8_t *file_data = nullptr;
	uint64_t file_size = 0;
	if(!FindInitrdFile(path, file_data, file_size))
	{
		debug("initrd missing ")(path)();
		return false;
	}

	if(!LoadUserElf(frames, kernel_root_cr3, file_data, file_size, user_cr3, entry, user_rsp))
	{
		debug("user ELF load failed for ")(path)();
		return false;
	}

	return true;
}

void PrepareUserThreadEntry(Thread *thread, uint64_t entry, uint64_t user_rsp)
{
	if(nullptr == thread)
	{
		return;
	}

	thread->exit_status = 0;
	clearThreadWait(thread);
	thread->frame = {};
	thread->frame.rip = entry;
	thread->frame.cs = kUserCodeSegment;
	thread->frame.rflags = 0x202;
	thread->frame.rsp = user_rsp;
	thread->frame.ss = kUserDataSegment;
	if(thread->process)
	{
		thread->process->exit_status = 0;
	}
}

Thread *LoadUserProgram(PageFrameContainer &frames, uint64_t kernel_root_cr3, const char *path, Process *parent)
{
	uint64_t user_cr3 = 0;
	uint64_t entry = 0;
	uint64_t user_rsp = 0;
	if(!LoadUserProgramImage(frames, kernel_root_cr3, path, user_cr3, entry, user_rsp))
	{
		return nullptr;
	}

	Process *process = createUserProcess(path, user_cr3);
	if(nullptr == process)
	{
		DestroyUserAddressSpace(frames, user_cr3);
		return nullptr;
	}
	process->parent = parent;

	Thread *thread = createUserThread(process, entry, user_rsp, frames);
	if(nullptr == thread)
	{
		reapProcess(process, frames);
		return nullptr;
	}
	PrepareUserThreadEntry(thread, entry, user_rsp);

	debug("user thread ready pid ")(process->pid)(" tid ")(thread->tid)(" entry 0x")(entry, 16)(" rsp 0x")(user_rsp, 16)();
	return thread;
}