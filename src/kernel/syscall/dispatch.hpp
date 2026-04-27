// Top-level syscall ABI dispatch. Individual syscall bodies live in smaller
// files so this module only translates register state into explicit contexts.
#pragma once

#include "arch/x86_64/interrupt/trap_frame.hpp"

struct Thread;

// Handle one syscall trap and return the thread that should resume next.
Thread* handle_syscall(TrapFrame* frame);
