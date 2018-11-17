#include "task.h"
#include "debug.h"

Task* taskList = (Task*)(0x8);
uint64_t nextpid = 1;
const size_t kNumTasks = 32;

void initTasks()
{
	size_t maxTasks = kNumTasks;
	debug("max tasks ")(maxTasks)();
    for(size_t i = 0; i < maxTasks; ++i)
    {
        taskList[i].pid = 0;
    }
}

Task* nextfreetss()
{
	size_t maxTasks = kNumTasks;
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
	size_t maxTasks = kNumTasks;
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
        return nullptr;

	task->pid = nextpid;
	nextpid++;
    task->waiting = 0;
	task->regs.rsp = (uint64_t) (stack + stack_len - 5);
	task->regs.r15 = (uint64_t) task;
	task->regs.cr3 = 0x60000;

	stack[stack_len - 1] = DATA_SEG;        // SS
	stack[stack_len - 2] = (uint64_t)stack; // RSP
	stack[stack_len - 3] = 0x2202;          // FLAGS
	stack[stack_len - 4] = CODE_SEG;        // CS
	stack[stack_len - 5] = (uint64_t)code;  // RIP

	debug("new task 0x")((uint64_t)(task), 16)(" pid ")(task->pid)(" cr3 0x")(task->regs.cr3, 16)(" code 0x")((uint64_t)code, 16)();
//    asm volatile("cli");
    linkTasks();
//    asm volatile("sti");
    return task;
}

