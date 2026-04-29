#include "arch/x86_64/cpu/syscall.hpp"

#include "arch/x86_64/cpu/x86.hpp"
#include "proc/thread.hpp"

extern "C" void syscall_entry();

namespace
{
constexpr uint32_t kIa32Efer = 0xC0000080;
constexpr uint32_t kIa32Star = 0xC0000081;
constexpr uint32_t kIa32Lstar = 0xC0000082;
constexpr uint32_t kIa32Sfmask = 0xC0000084;
constexpr uint64_t kEferSyscallEnable = 1ull;
constexpr uint64_t kSyscallRflagsMask = FL_TF | FL_IF | FL_DF | FL_NT | FL_AC;

static_assert(kKernelDataSegment == (kKernelCodeSegment + 8));
static_assert(kUserCodeSegment == ((kUserDataSegment - 8) + 16));
static_assert(kUserDataSegment == ((kUserDataSegment - 8) + 8));
}  // namespace

void cpu_enable_syscall_entry()
{
    const uint64_t user_star_selector = kUserDataSegment - 8;
    const uint64_t star =
        (user_star_selector << 48) | (static_cast<uint64_t>(kKernelCodeSegment) << 32);

    wrmsr(kIa32Star, star);
    wrmsr(kIa32Lstar, reinterpret_cast<uint64_t>(syscall_entry));
    wrmsr(kIa32Sfmask, kSyscallRflagsMask);
    wrmsr(kIa32Efer, rdmsr(kIa32Efer) | kEferSyscallEnable);
}
