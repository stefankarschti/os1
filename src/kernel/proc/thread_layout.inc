; Assembly-visible Thread offsets. Keep synchronized with proc/thread.h static
; asserts before changing Thread layout.

%define THREAD_NEXT 0
%define THREAD_TID 8
%define THREAD_PROCESS 16
%define THREAD_STATE 24
%define THREAD_USER_MODE 28
%define THREAD_ADDRESS_SPACE_CR3 40
%define THREAD_KERNEL_STACK_BASE 48
%define THREAD_KERNEL_STACK_TOP 56
%define THREAD_EXIT_STATUS 64
%define THREAD_FRAME 72
