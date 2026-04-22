%include "cpu.inc"
%include "task.inc"
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
	and rsp, -16
	call trap_dispatch
	test rax, rax
	jz .resume_frame
	mov rdi, rax
	jmp restore_thread

.resume_frame:
	mov rdi, r12
	jmp restore_frame_ptr

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
