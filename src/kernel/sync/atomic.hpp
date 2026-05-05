// Named atomic operations used by SMP scheduler and completion primitives.
#pragma once

template <typename T>
[[nodiscard]] inline T atomic_load_acquire(const volatile T* value)
{
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

template <typename T>
inline void atomic_store_release(volatile T* value, T desired)
{
    __atomic_store_n(value, desired, __ATOMIC_RELEASE);
}

template <typename T>
[[nodiscard]] inline bool atomic_compare_exchange_strong(volatile T* value, T* expected, T desired)
{
    return __atomic_compare_exchange_n(
        value, expected, desired, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}

template <typename T>
[[nodiscard]] inline T atomic_fetch_add(volatile T* value, T increment)
{
    return __atomic_fetch_add(value, increment, __ATOMIC_RELAXED);
}

template <typename T>
[[nodiscard]] inline T atomic_exchange(volatile T* value, T desired)
{
    return __atomic_exchange_n(value, desired, __ATOMIC_ACQ_REL);
}
