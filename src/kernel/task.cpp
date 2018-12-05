#include "task.h"
#include "memory.h"
#include "debug.h"

Task** current_task = (Task**)(0x400);
Task* taskList = (Task*)(0x408);
uint64_t nextpid = 1;
const size_t k_num_tasks = 32;

void initTasks()
{
	debug("max tasks ")(k_num_tasks)();
//	memset((void*)(0x0), 0, 0x20000);
	for(int i = 0; i < k_num_tasks; ++i)
	{
		taskList[i].pid = 0;
	}
}

Task* nextfreetss()
{
	size_t maxTasks = k_num_tasks;
	for(size_t i = 0; i < maxTasks; ++i)
    {
        if(0 == taskList[i].pid)
        {
			debug("allocate task ")(i)(" 0x")((uint64_t)(taskList + i), 16)();
            return taskList + i;
        }
    }
    return nullptr;
}

void linkTasks()
{
    Task* first = nullptr;
    Task* last = nullptr;
	size_t maxTasks = k_num_tasks;
	for(size_t i = 0; i < maxTasks; ++i)
    {
        if(taskList[i].pid)
        {
            Task* p = taskList + i;
            if(!first)
            {
                first = p;
            }
            if(last)
            {
                last->nexttask = p;
            }
            last = p;
        }
    }
    if(last)
    {
        last->nexttask = first;
    }

	debug("task linkage:")();
	for(size_t i = 0; i < maxTasks; ++i)
	{
		if(taskList[i].pid)
		{
			debug("0x")((uint64_t)(taskList + i), 16)(" -> ")("0x")((uint64_t)(taskList[i].nexttask), 16)();
		}
	}
	debug("done")();
}

Task* newTask(void *code, uint64_t *stack, size_t stack_len)
{
	Task *task = nextfreetss();
    if(nullptr == task)
	{
		return nullptr;
	}

	task->pid = nextpid++;
	task->timer = 10;
//	task->quanta = 10;	// reload value
	task->regs.rax = 0;
	task->regs.rbx = 0;
	task->regs.rcx = 0;
	task->regs.rdx = 0;
	task->regs.rsi = 0;
	task->regs.rdi = 0;
	task->regs.rbp = 0;
	task->regs.rsp = (uint64_t) (&stack[stack_len - 5]);
	task->regs.r08 = 0;
	task->regs.r09 = 0;
	task->regs.r10 = 0;
	task->regs.r11 = 0;
	task->regs.r12 = 0;
	task->regs.r13 = 0;
	task->regs.r14 = 0;
	task->regs.r15 = (uint64_t) task;
	task->regs.cr3 = 0x60000;
	task->regs.rfl = 0x2202;

	stack[stack_len - 1] = DATA_SEG;						// SS
	stack[stack_len - 2] = (uint64_t)(stack + stack_len);	// RSP
	stack[stack_len - 3] = 0x2202;							// FLAGS
	stack[stack_len - 4] = CODE_SEG;						// CS
	stack[stack_len - 5] = (uint64_t)code;					// RIP

	debug("new task 0x")((uint64_t)(task), 16)(" pid ")(task->pid)(" cr3 0x")(task->regs.cr3, 16)(" code 0x")((uint64_t)code, 16)
			(" stack 0x")((uint64_t)stack, 16)(" stack len ")(stack_len)(" rsp 0x")(task->regs.rsp, 16)();
//    asm volatile("cli");
    linkTasks();
//    asm volatile("sti");
    return task;
}


void Registers::print()
{
	debug("RAX=")(rax, 16, 16)();
	debug("RBX=")(rbx, 16, 16)();
	debug("RCX=")(rcx, 16, 16)();
	debug("RDX=")(rdx, 16, 16)();
	debug("RSI=")(rsi, 16, 16)();
	debug("RDI=")(rdi, 16, 16)();
	debug("RBP=")(rbp, 16, 16)();
	debug("RSP=")(rsp, 16, 16)();
	debug("R08=")(r08, 16, 16)();
	debug("R09=")(r09, 16, 16)();
	debug("R10=")(r10, 16, 16)();
	debug("R11=")(r11, 16, 16)();
	debug("R12=")(r12, 16, 16)();
	debug("R13=")(r13, 16, 16)();
	debug("R14=")(r14, 16, 16)();
	debug("R15=")(r15, 16, 16)();
	debug("RFL=")(rfl, 16, 16)();
	debug("CR3=")(cr3, 16, 16)();
}
