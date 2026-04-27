// Kernel idle-thread entry point. The current system has one idle thread; this
// boundary is where per-CPU idle behavior can grow when AP scheduling arrives.
#pragma once

// Entry function for the scheduler's idle thread.
void kernel_idle_thread();

