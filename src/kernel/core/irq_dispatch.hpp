// IRQ dispatch policy. Hardware-specific interrupt controller code stays in
// arch/platform modules; this layer handles timer/input side effects and thread
// selection after an IRQ.
#pragma once

#include "arch/x86_64/interrupt/trap_frame.hpp"

struct Thread;

// Main IRQ path called by trap dispatch for vectors T_IRQ0 through T_IRQ0+15.
Thread* handle_irq(TrapFrame* frame);
