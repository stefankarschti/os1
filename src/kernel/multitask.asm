panic:
	mov rsi, 0xB8000
	add ax, 0xF00
	mov word [rsi], ax
	hlt
	jmp panic

; set active task to RDI
; switch to this task
; start timer
global startMultiTask
startMultiTask:
.regs equ 6 * 8
	cli
	mov r15, rdi
	mov rsp, [r15 + .regs + 7 * 8]

	; start irq0 timer: 1193182 ticks/s divided by ax
	mov al, 0x34
	out 0x43, al
	mov ax, 1193    ; 1193182 ticks/s divided by ax
	out 0x40, al    ; low
	rol ax, 8
	out 0x40, al    ; high
	rol ax, 8

	; save registers
	mov [r15 + .regs + 0 * 8], rax
	mov rax, [rsp + 16]	; rflags
	mov [r15 + .regs + 16 * 8], rax
	mov [r15 + .regs + 1 * 8], rbx
	mov [r15 + .regs + 2 * 8], rcx
	mov [r15 + .regs + 3 * 8], rdx
	mov [r15 + .regs + 4 * 8], rsi
	mov [r15 + .regs + 5 * 8], rdi
	mov [r15 + .regs + 6 * 8], rbp
	mov [r15 + .regs + 7 * 8], rsp
	mov [r15 + .regs + 8 * 8], r8
	mov [r15 + .regs + 9 * 8], r9
	mov [r15 + .regs + 10 * 8], r10
	mov [r15 + .regs + 11 * 8], r11
	mov [r15 + .regs + 12 * 8], r12
	mov [r15 + .regs + 13 * 8], r13
	mov [r15 + .regs + 14 * 8], r14
	mov [r15 + .regs + 15 * 8], r15
	mov rax, cr3
	mov [r15 + .regs + 17 * 8], rax

	iretq

global task_switch_irq
task_switch_irq:
.regs equ 6 * 8
	cli
	push rax
	mov al, 0x20
	out 0x20, al
	pop rax

	cmp r15, [r15 + .regs + 15 * 8]
	je .do
	mov ax, 'E'
	call panic ; error stop - corrupted r15

.do:

	; save registers
	mov [r15 + .regs + 0 * 8], rax
	mov [r15 + .regs + 1 * 8], rbx
	mov [r15 + .regs + 2 * 8], rcx
	mov [r15 + .regs + 3 * 8], rdx
	mov [r15 + .regs + 4 * 8], rsi
	mov [r15 + .regs + 5 * 8], rdi
	mov [r15 + .regs + 6 * 8], rbp
	mov [r15 + .regs + 7 * 8], rsp
	mov [r15 + .regs + 8 * 8], r8
	mov [r15 + .regs + 9 * 8], r9
	mov [r15 + .regs + 10 * 8], r10
	mov [r15 + .regs + 11 * 8], r11
	mov [r15 + .regs + 12 * 8], r12
	mov [r15 + .regs + 13 * 8], r13
	mov [r15 + .regs + 14 * 8], r14
	mov [r15 + .regs + 15 * 8], r15
	mov rax, [rsp + 16]	; rflags
	mov [r15 + .regs + 16 * 8], rax
	mov rax, cr3
	mov [r15 + .regs + 17 * 8], rax

	; task switch
	mov r15, [r15]

	; restore registers
	mov rax, [r15 + .regs + 17 * 8]
	mov rbx, cr3
	cmp rax, rbx
	je .l1
	mov ax, 'C' ; panic: corrupted cr3

.l1:
	mov cr3, rax
	mov rax, [r15 + .regs + 0 * 8]
	mov rbx, [r15 + .regs + 1 * 8]
	mov rcx, [r15 + .regs + 2 * 8]
	mov rdx, [r15 + .regs + 3 * 8]
	mov rsi, [r15 + .regs + 4 * 8]
	mov rdi, [r15 + .regs + 5 * 8]
	mov rbp, [r15 + .regs + 6 * 8]
	mov rsp, [r15 + .regs + 7 * 8]
	mov r8,  [r15 + .regs + 8 * 8]
	mov r9,  [r15 + .regs + 9 * 8]
	mov r10, [r15 + .regs + 10 * 8]
	mov r11, [r15 + .regs + 11 * 8]
	mov r12, [r15 + .regs + 12 * 8]
	mov r13, [r15 + .regs + 13 * 8]
	mov r14, [r15 + .regs + 14 * 8]
	mov r15, [r15 + .regs + 15 * 8]
	; RFLAGS will be restored by iretq

	iretq
