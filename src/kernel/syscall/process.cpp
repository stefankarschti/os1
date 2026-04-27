#include "syscall/process.hpp"

#include <os1/observe.h>

#include "debug/debug.hpp"
#include "mm/user_copy.hpp"
#include "proc/user_program.hpp"
#include "proc/thread.hpp"
#include "util/fixed_string.hpp"

long sys_write(const ProcessSyscallContext &context, int fd, uint64_t user_buffer, size_t length)
{
	if((fd != 1) && (fd != 2))
	{
		return -1;
	}
	if((nullptr == context.frames) || (nullptr == context.write_console_bytes))
	{
		return -1;
	}

	Thread *thread = current_thread();
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
		if(!copy_from_user(*context.frames, thread, user_buffer + written, buffer, chunk))
		{
			return -1;
		}
		context.write_console_bytes(buffer, chunk);
		written += chunk;
	}
	return (long)written;
}


long sys_spawn(const ProcessSyscallContext &context, uint64_t user_path)
{
	if(nullptr == context.frames)
	{
		return -1;
	}

	Thread *thread = current_thread();
	if((nullptr == thread) || (nullptr == thread->process))
	{
		return -1;
	}

	char path[OS1_OBSERVE_INITRD_PATH_BYTES];
	if(!copy_user_string(*context.frames, thread, user_path, path, sizeof(path)))
	{
		return -1;
	}

	Thread *child = LoadUserProgram(*context.frames, context.kernel_root_cr3, path, thread->process);
	if((nullptr == child) || (nullptr == child->process))
	{
		return -1;
	}
	return static_cast<long>(child->process->pid);
}

long sys_exec(const ProcessSyscallContext &context, uint64_t user_path)
{
	if((nullptr == context.frames) || (nullptr == context.write_cr3))
	{
		return -1;
	}

	Thread *thread = current_thread();
	if((nullptr == thread) || (nullptr == thread->process) || !thread->user_mode)
	{
		return -1;
	}

	char path[OS1_OBSERVE_INITRD_PATH_BYTES];
	if(!copy_user_string(*context.frames, thread, user_path, path, sizeof(path)))
	{
		return -1;
	}

	uint64_t new_cr3 = 0;
	uint64_t entry = 0;
	uint64_t user_rsp = 0;
	if(!load_user_program_image(*context.frames, context.kernel_root_cr3, path, new_cr3, entry, user_rsp))
	{
		return -1;
	}

	const uint64_t old_cr3 = thread->address_space_cr3;
	thread->address_space_cr3 = new_cr3;
	thread->process->address_space.cr3 = new_cr3;
	thread->process->state = ProcessState::Running;
	copy_fixed_string(thread->process->name, sizeof(thread->process->name), path);
	prepare_user_thread_entry(thread, entry, user_rsp);
	context.write_cr3(new_cr3);

	if((0 != old_cr3) && (old_cr3 != new_cr3) && !destroy_user_address_space(*context.frames, old_cr3))
	{
		debug("exec old address-space teardown failed for ")(path)();
	}

	return 0;
}