#ifndef _TASK_H_
#define _TASK_H_

#include "stdint.h"

#define CODE_SEG     0x0008
#define DATA_SEG     0x0010

struct Task
{
    Task *nexttask;
    uint16_t pid = 0;
    uint16_t waiting;
    uint64_t timer;

    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rbp;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rsp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rflags;
    uint16_t ds;
    uint16_t es;
    uint16_t fs;
    uint16_t gs;
    uint16_t ss;
    uint64_t cr3;
};

void initTasks();
Task* newTask(void *taskCode);

#endif
