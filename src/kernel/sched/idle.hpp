// Kernel idle-thread entry point. Each CPU owns a Thread record that enters
// this same function once that CPU joins the scheduler.
#pragma once

// Entry function for the scheduler's idle thread.
void kernel_idle_thread();
