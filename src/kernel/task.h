#ifndef _TASK_H_
#define _TASK_H_

#include <stddef.h>
#include <stdint.h>

#define CODE_SEG     0x0008
#define DATA_SEG     0x0010

struct Task
{
    Task *nexttask = 0;
    uint64_t pid = 0;
    uint64_t waiting = 0;
    uint64_t timer = 0;
    uint64_t rsp = 0;
    uint64_t r15 = 0;
};

void initTasks();
Task* newTask(void *taskCode, uint64_t *stack, size_t stack_len);

#ifdef __cplusplus
extern "C" {
#endif

void startMultiTask(Task* task);

#ifdef __cplusplus
}
#endif

#endif
