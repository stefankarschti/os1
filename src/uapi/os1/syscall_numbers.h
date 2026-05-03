#pragma once

#include <stdint.h>

/**
 * @brief Stable user/kernel syscall numbers.
 *
 * These identifiers are intentionally C-compatible and global. Keep kernel
 * dispatch and user wrappers synchronized through this single header.
 */
enum
{
    os1_sys_write = 1,
    os1_sys_exit = 2,
    os1_sys_yield = 3,
    os1_sys_getpid = 4,
    os1_sys_read = 5,
    os1_sys_observe = 6,
    os1_sys_spawn = 7,
    os1_sys_waitpid = 8,
    os1_sys_exec = 9,
};
