panic:
	mov rsi, 0xB8000
	xor ah, ah
	add ax, 0xF00
	mov word [rsi], ax
	hlt
	jmp panic

regs equ 3 * 8
current_task equ 0x400

; set active task to RDI
; switch to this task
; start timer
global startMultiTask
startMultiTask:
	cli
	mov r15, rdi
	mov [current_task], r15

	cmp r15, [r15 + regs + 15 * 8]
	je .do
	mov ax, 'E'
	call panic ; error stop - corrupted r15
.do:
; start irq0 timer: 1193182 ticks/s divided by ax
	mov al, 0x34
	out 0x43, al
	mov ax, 1193    ; 1193182 ticks/s divided by ax
	out 0x40, al    ; low
	rol ax, 8
	out 0x40, al    ; high
	rol ax, 8

	; restore registers
	mov rax, [r15 + regs + 17 * 8]
	mov cr3, rax
	mov rax, [r15 + regs + 16 * 8]
	push rax
	popf
	mov rax, [r15 + regs + 0 * 8]
	mov rbx, [r15 + regs + 1 * 8]
	mov rcx, [r15 + regs + 2 * 8]
	mov rdx, [r15 + regs + 3 * 8]
	mov rsi, [r15 + regs + 4 * 8]
	mov rdi, [r15 + regs + 5 * 8]
	mov rbp, [r15 + regs + 6 * 8]
	mov rsp, [r15 + regs + 7 * 8]
	mov r8,  [r15 + regs + 8 * 8]
	mov r9,  [r15 + regs + 9 * 8]
	mov r10, [r15 + regs + 10 * 8]
	mov r11, [r15 + regs + 11 * 8]
	mov r12, [r15 + regs + 12 * 8]
	mov r13, [r15 + regs + 13 * 8]
	mov r14, [r15 + regs + 14 * 8]
	mov r15, [r15 + regs + 15 * 8]
	iretq

global task_switch_irq
task_switch_irq:
	cli
	mov r15, [current_task]

	cmp r15, [r15 + regs + 15 * 8]
	je .do
	mov ax, 'E'
	call panic ; error stop - corrupted r15
.do:

	; decrement timer
	dec qword [r15 + 2 * 8]
	jz .switch
	push rax
	mov al, 0x20
	out 0x20, al
	pop rax
	iretq

.switch:
	; save registers
	mov [r15 + regs + 0 * 8], rax
	mov [r15 + regs + 1 * 8], rbx
	mov [r15 + regs + 2 * 8], rcx
	mov [r15 + regs + 3 * 8], rdx
	mov [r15 + regs + 4 * 8], rsi
	mov [r15 + regs + 5 * 8], rdi
	mov [r15 + regs + 6 * 8], rbp
	mov [r15 + regs + 7 * 8], rsp
	mov [r15 + regs + 8 * 8], r8
	mov [r15 + regs + 9 * 8], r9
	mov [r15 + regs + 10 * 8], r10
	mov [r15 + regs + 11 * 8], r11
	mov [r15 + regs + 12 * 8], r12
	mov [r15 + regs + 13 * 8], r13
	mov [r15 + regs + 14 * 8], r14
	mov [r15 + regs + 15 * 8], r15
	mov rax, [rsp + 16]	; rflags
	mov [r15 + regs + 16 * 8], rax
	mov rax, cr3
	mov [r15 + regs + 17 * 8], rax

	; reload timer
	mov rax, 10 ;[r15 + 3 * 8]
	mov [r15 + 2 * 8], rax

	; task switch
	mov r15, [r15]
	mov [current_task], r15

	; restore registers
	mov rax, [r15 + regs + 17 * 8]
	mov cr3, rax
	mov rax, [r15 + regs + 16 * 8]
	push rax
	popf
	mov rax, [r15 + regs + 0 * 8]
	mov rbx, [r15 + regs + 1 * 8]
	mov rcx, [r15 + regs + 2 * 8]
	mov rdx, [r15 + regs + 3 * 8]
	mov rsi, [r15 + regs + 4 * 8]
	mov rdi, [r15 + regs + 5 * 8]
	mov rbp, [r15 + regs + 6 * 8]
	mov rsp, [r15 + regs + 7 * 8]
	mov r8,  [r15 + regs + 8 * 8]
	mov r9,  [r15 + regs + 9 * 8]
	mov r10, [r15 + regs + 10 * 8]
	mov r11, [r15 + regs + 11 * 8]
	mov r12, [r15 + regs + 12 * 8]
	mov r13, [r15 + regs + 13 * 8]
	mov r14, [r15 + regs + 14 * 8]
	mov r15, [r15 + regs + 15 * 8]
	push rax
	mov al, 0x20
	out 0x20, al
	pop rax
	iretq
