// Generic byte and word memory primitives shared by the freestanding kernel and
// the Limine shim. Architecture-specific helpers such as port I/O intentionally
// live under arch/x86_64 so generic subsystems do not depend on x86 details.
#ifndef OS1_KERNEL_UTIL_MEMORY_H
#define OS1_KERNEL_UTIL_MEMORY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Fill `num` bytes at `ptr` with the low byte of `value`.
void memset(void *ptr, uint8_t value, uint64_t num);

// Fill `num` bytes at `ptr` with repeated 16-bit words.
void memsetw(void* ptr, uint16_t value, uint64_t num);

// Fill `num` bytes at `ptr` with repeated 32-bit doublewords.
void memsetd(void* ptr, uint32_t value, uint64_t num);

// Fill `num` bytes at `ptr` with repeated 64-bit quadwords.
void memsetq(void* ptr, uint64_t value, uint64_t num);

// Copy exactly `len` bytes from `src` to `dest`. Callers must avoid overlap.
void memcpy(void *dest, const void *src, uint64_t len);

#ifdef __cplusplus
}
#endif // OS1_KERNEL_UTIL_MEMORY_H

#endif
