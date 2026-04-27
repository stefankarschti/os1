; Common trap-entry path for IRQs, exceptions, and int 0x80 syscalls. It saves
; registers into either the current Thread frame or the per-CPU interrupt frame,
; calls C++ trap_dispatch, then resumes or switches threads.

%include "cpu.inc"
%include "thread_layout.inc"
%include "trapframe.inc"

extern trap_dispatch
extern restore_thread
extern restore_frame_ptr

global trap_entry_common
trap_entry_common:
	push rax
	push rdx

	mov rax, [gs:CPU_CURRENT_THREAD]
	test rax, rax
	jz .cpu_frame
	lea rax, [rax + THREAD_FRAME]
	jmp .save_frame

.cpu_frame:
	lea rax, [gs:CPU_INTERRUPT_FRAME]

.save_frame:
	mov rdx, [rsp]
	mov [rax + TF_RDX], rdx
	mov rdx, [rsp + 8]
	mov [rax + TF_RAX], rdx
	mov [rax + TF_RBX], rbx
	mov [rax + TF_RCX], rcx
	mov [rax + TF_RSI], rsi
	mov [rax + TF_RDI], rdi
	mov [rax + TF_RBP], rbp
	mov [rax + TF_R8], r8
	mov [rax + TF_R9], r9
	mov [rax + TF_R10], r10
	mov [rax + TF_R11], r11
	mov [rax + TF_R12], r12
	mov [rax + TF_R13], r13
	mov [rax + TF_R14], r14
	mov [rax + TF_R15], r15

	mov rdx, [rsp + 16]
	mov [rax + TF_VECTOR], rdx
	mov rdx, [rsp + 24]
	mov [rax + TF_ERROR_CODE], rdx
	mov rdx, [rsp + 32]
	mov [rax + TF_RIP], rdx
	mov rdx, [rsp + 40]
	mov [rax + TF_CS], rdx
	mov rdx, [rsp + 48]
	mov [rax + TF_RFLAGS], rdx

	mov rdx, [rax + TF_CS]
	test dl, 3
	jz .kernel_rsp
	mov rdx, [rsp + 56]
	mov [rax + TF_RSP], rdx
	mov rdx, [rsp + 64]
	mov [rax + TF_SS], rdx
	jmp .dispatch

.kernel_rsp:
	lea rdx, [rsp + 56]
	mov [rax + TF_RSP], rdx
	mov qword [rax + TF_SS], 0

.dispatch:
	mov r12, rax
	mov rdi, rax
	mov r13, rsp
	and rsp, -16
	call trap_dispatch
	mov rsp, r13
	test rax, rax
	jz .resume_frame
	mov rbx, [gs:CPU_CURRENT_THREAD]
	cmp rax, rbx
	jne .switch_thread
	mov rdx, [r12 + TF_CS]
	test dl, 3
	jnz .switch_thread
	jmp .resume_live_kernel

.switch_thread:
	mov rdi, rax
	jmp restore_thread

.resume_frame:
	mov rdi, r12
	jmp restore_frame_ptr

.resume_live_kernel:
	mov rdx, [r12 + TF_RIP]
	mov [rsp + 32], rdx
	mov rdx, [r12 + TF_CS]
	mov [rsp + 40], rdx
	mov rdx, [r12 + TF_RFLAGS]
	mov [rsp + 48], rdx

	mov r15, [r12 + TF_R15]
	mov r14, [r12 + TF_R14]
	mov r13, [r12 + TF_R13]
	mov r11, [r12 + TF_R11]
	mov r10, [r12 + TF_R10]
	mov r9,  [r12 + TF_R9]
	mov r8,  [r12 + TF_R8]
	mov rbp, [r12 + TF_RBP]
	mov rsi, [r12 + TF_RSI]
	mov rdi, [r12 + TF_RDI]
	mov rdx, [r12 + TF_RDX]
	mov rcx, [r12 + TF_RCX]
	mov rbx, [r12 + TF_RBX]
	mov rax, [r12 + TF_RAX]
	mov r12, [r12 + TF_R12]
	add rsp, 32
	iretq

%macro IRQ_STUB 1
global irq%1
irq%1:
	push qword 0
	push qword (32 + %1)
	jmp trap_entry_common
%endmacro

IRQ_STUB 0
IRQ_STUB 1
IRQ_STUB 2
IRQ_STUB 3
IRQ_STUB 4
IRQ_STUB 5
IRQ_STUB 6
IRQ_STUB 7
IRQ_STUB 8
IRQ_STUB 9
IRQ_STUB 10
IRQ_STUB 11
IRQ_STUB 12
IRQ_STUB 13
IRQ_STUB 14
IRQ_STUB 15

global int_80h
int_80h:
	push qword 0
	push qword 0x80
	jmp trap_entry_common
