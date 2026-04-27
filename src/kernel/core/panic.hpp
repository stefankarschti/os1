// Final kernel stop path. Keeping this tiny boundary named makes later panic
// reporting easier without scattering halt loops across exception handlers.
#pragma once

// Disable interrupts and halt forever.
[[noreturn]] void HaltForever();

