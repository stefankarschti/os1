#include "task.h"

Task taskList[32];
uint16_t nextpid = 1;
uint64_t tempStack[512] __attribute__ ((aligned (4096)));
uint64_t userStack[512] __attribute__ ((aligned (4096)));
uint64_t userData [512] __attribute__ ((aligned (4096)));

Task* runnable[32] = {0};

void initTasks()
{
	int maxTasks = sizeof(taskList) / sizeof(Task);
	for(int i = 0; i < maxTasks; ++i)
	{
		taskList[i].pid = 0;
	}
	int maxRunnable = sizeof(runnable) / sizeof(Task*);
	for(int i = 0; i < maxRunnable; ++i)
	{
		runnable[i] = nullptr;
	}
}

Task *
nextfreetss()
{
	int maxTasks = sizeof(taskList) / sizeof(Task);
	for(int i = 0; i < maxTasks; ++i)
	{
		if(0 == taskList[i].pid)
		{
			return taskList + i;
		}
	}
    return nullptr;
}

void linkTask(Task *task)
{
	int maxRunnable = sizeof(runnable) / sizeof(Task*);
	for(int i = 0; i < maxRunnable; ++i)
	{
		if(runnable[i] == nullptr)
		{
			runnable[i] = task;
			return;
		}
	}	
}

Task* newTask(void *taskCode)
{
    uint64_t *stack;
    struct Task *task = nextfreetss();
    if(nullptr == task)
    	return task;

    task->pid = nextpid++;
    task->waiting = 0;
//    task->cr3 = (long) VCreatePageDir(task->pid, 0);
    task->ds = DATA_SEG;
    stack = (uint64_t*)(tempStack + 512 - 5);
    task->rsp = (uint64_t) stack;
    task->r15 = (uint64_t) task;
    stack[0] = (uint64_t) taskCode;
    stack[1] = CODE_SEG;
    stack[2] = 0x2202;
	{
		int taskIndex = task - taskList;
	    stack[3] = (uint64_t) (userStack + 512 - taskIndex * 16);  // TODO: add VMEM to have separate stacks
	}
    stack[4] = DATA_SEG;
    asm("cli");
    linkTask(task);
    asm("sti");
    return (task);
}

