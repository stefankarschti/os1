; Thread restore and first scheduler entry for x86_64. C++ owns Thread and
; TrapFrame layout; this file consumes the generated offset constants and turns
; a selected Thread into an iretq return path.

%include "cpu.inc"
%include "thread_layout.inc"
%include "trapframe.inc"

%define KERNEL_DATA_SEGMENT 0x10

global start_multi_task
global enter_first_thread
global restore_thread
global restore_frame_ptr

enter_first_thread:
	mov rsp, rsi
	and rsp, -16
	jmp start_multi_task

start_multi_task:
	jmp restore_thread

; Install the selected thread into per-CPU state, load its CR3/RSP0, and resume
; from the embedded TrapFrame.
restore_thread:
	mov [gs:CPU_CURRENT_THREAD], rdi
	mov rax, [rdi + THREAD_KERNEL_STACK_TOP]
	mov [gs:CPU_TSS_RSP0], rax
	mov rax, [rdi + THREAD_ADDRESS_SPACE_CR3]
	mov cr3, rax
	lea rdi, [rdi + THREAD_FRAME]
	jmp restore_frame_ptr

; Restore a TrapFrame already addressed by RDI. Used both for direct resume and
; after trap_dispatch decides to keep the current thread.
restore_frame_ptr:
	sub rsp, 16
	mov r11, rdi
	mov rax, [r11 + TF_CS]
	test al, 3
	jz .kernel_return
	lea rax, [r11 + TF_RIP]
	mov [rsp], rax
	jmp .resume_user

.kernel_return:
	mov rax, [r11 + TF_RSP]
	sub rax, 24
	mov rcx, [r11 + TF_RIP]
	mov [rax], rcx
	mov rcx, [r11 + TF_CS]
	mov [rax + 8], rcx
	mov rcx, [r11 + TF_RFLAGS]
	mov [rax + 16], rcx
	mov rcx, [r11 + TF_RSP]
	mov [rax + 24], rcx
	mov qword [rax + 32], KERNEL_DATA_SEGMENT
	mov [rsp], rax
	jmp .resume_kernel

.resume_user:
	mov rax, [r11 + TF_R11]
	mov [rsp + 8], rax

	mov r15, [r11 + TF_R15]
	mov r14, [r11 + TF_R14]
	mov r13, [r11 + TF_R13]
	mov r12, [r11 + TF_R12]
	mov r10, [r11 + TF_R10]
	mov r9,  [r11 + TF_R9]
	mov r8,  [r11 + TF_R8]
	mov rbp, [r11 + TF_RBP]
	mov rsi, [r11 + TF_RSI]
	mov rdi, [r11 + TF_RDI]
	mov rdx, [r11 + TF_RDX]
	mov rcx, [r11 + TF_RCX]
	mov rbx, [r11 + TF_RBX]
	mov rax, [r11 + TF_RAX]
	mov r11, [rsp + 8]
	mov rsp, [rsp]
	iretq

.resume_kernel:
	mov rax, [r11 + TF_R11]
	mov [rsp + 8], rax

	mov r15, [r11 + TF_R15]
	mov r14, [r11 + TF_R14]
	mov r13, [r11 + TF_R13]
	mov r12, [r11 + TF_R12]
	mov r10, [r11 + TF_R10]
	mov r9,  [r11 + TF_R9]
	mov r8,  [r11 + TF_R8]
	mov rbp, [r11 + TF_RBP]
	mov rsi, [r11 + TF_RSI]
	mov rdi, [r11 + TF_RDI]
	mov rdx, [r11 + TF_RDX]
	mov rcx, [r11 + TF_RCX]
	mov rbx, [r11 + TF_RBX]
	mov rax, [r11 + TF_RAX]
	mov r11, [rsp + 8]
	mov rsp, [rsp]
	mov ax, KERNEL_DATA_SEGMENT
	mov ds, ax
	mov es, ax
	mov ss, ax
	iretq
