// User and kernel exception policy extracted from the kernel entry file.
#include "core/fault.h"

#include "arch/x86_64/cpu/control_regs.h"
#include "arch/x86_64/interrupt/interrupt.h"
#include "console/terminal.h"
#include "core/kernel_state.h"
#include "core/panic.h"
#include "debug/debug.h"
#include "sched/scheduler.h"
#include "proc/thread.h"

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