#include "task.h"

Task taskList[32];
uint64_t nextpid = 1;

void initTasks()
{
    size_t maxTasks = sizeof(taskList) / sizeof(Task);
    for(size_t i = 0; i < maxTasks; ++i)
    {
        taskList[i].pid = 0;
    }
}

Task* nextfreetss()
{
    size_t maxTasks = sizeof(taskList) / sizeof(Task);
    for(size_t i = 0; i < maxTasks; ++i)
    {
        if(0 == taskList[i].pid)
        {
            return taskList + i;
        }
    }
    return nullptr;
}

void linkTasks()
{
    Task* first = nullptr;
    Task* last = nullptr;
    size_t maxTasks = sizeof(taskList) / sizeof(Task);
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
}

Task* newTask(void *code, uint64_t *stack, size_t stack_len)
{
    struct Task *task = nextfreetss();
    if(nullptr == task)
        return nullptr;

    task->pid = nextpid++;
    task->waiting = 0;
//    task->cr3 = (long) VCreatePageDir(task->pid, 0);
    task->rsp = (uint64_t) (&stack[stack_len - 19]); // RSP - check value!!!
    task->r15 = (uint64_t) task;

    stack[stack_len -  1] = DATA_SEG;        // SS
    stack[stack_len -  2] = (uint64_t)stack; // RSP
    stack[stack_len -  3] = 0x2202;          // FLAGS
    stack[stack_len -  4] = CODE_SEG;        // CS
    stack[stack_len -  5] = (uint64_t) code; // RIP
    stack[stack_len -  6] = 0; // RAX
    stack[stack_len -  7] = 0; // RBX
    stack[stack_len -  8] = 0; // RCX
    stack[stack_len -  9] = 0; // RDX
    stack[stack_len - 10] = 0; // RDI
    stack[stack_len - 11] = 0; // RSI
    stack[stack_len - 12] = 0; // RBP
    stack[stack_len - 13] = 0; // R8
    stack[stack_len - 14] = 0; // R9
    stack[stack_len - 15] = 0; // R10
    stack[stack_len - 16] = 0; // R11
    stack[stack_len - 17] = 0; // R12
    stack[stack_len - 18] = 0; // R13
    stack[stack_len - 19] = 0x1234; // R14

//    asm volatile("cli");
    linkTasks();
//    asm volatile("sti");
    return task;
}

