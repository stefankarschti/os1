
extern exception_handler

%define CPU_CURRENT_TASK 8
%define CPU_INTERRUPT_REGS 16

; save registers in the current CPU page
global saveregs
saveregs:
	mov [gs:CPU_INTERRUPT_REGS + 0 * 8], rax
	mov [gs:CPU_INTERRUPT_REGS + 1 * 8], rbx
	mov [gs:CPU_INTERRUPT_REGS + 2 * 8], rcx
	mov [gs:CPU_INTERRUPT_REGS + 3 * 8], rdx
	mov [gs:CPU_INTERRUPT_REGS + 4 * 8], rsi
	mov [gs:CPU_INTERRUPT_REGS + 5 * 8], rdi
	mov [gs:CPU_INTERRUPT_REGS + 6 * 8], rbp
	mov [gs:CPU_INTERRUPT_REGS + 7 * 8], rsp
	mov [gs:CPU_INTERRUPT_REGS + 8 * 8], r8
	mov [gs:CPU_INTERRUPT_REGS + 9 * 8], r9
	mov [gs:CPU_INTERRUPT_REGS + 10 * 8], r10
	mov [gs:CPU_INTERRUPT_REGS + 11 * 8], r11
	mov [gs:CPU_INTERRUPT_REGS + 12 * 8], r12
	mov [gs:CPU_INTERRUPT_REGS + 13 * 8], r13
	mov [gs:CPU_INTERRUPT_REGS + 14 * 8], r14
	mov [gs:CPU_INTERRUPT_REGS + 15 * 8], r15
	pushf
	pop rax
	mov [gs:CPU_INTERRUPT_REGS + 16 * 8], rax
	mov rax, cr3
	mov [gs:CPU_INTERRUPT_REGS + 17 * 8], rax
	mov rax, [gs:CPU_INTERRUPT_REGS + 0 * 8]
	ret

; restore registers from the current CPU page
global restoreregs
restoreregs:
	mov rax, [gs:CPU_INTERRUPT_REGS + 0 * 8]
	mov rbx, [gs:CPU_INTERRUPT_REGS + 1 * 8]
	mov rcx, [gs:CPU_INTERRUPT_REGS + 2 * 8]
	mov rdx, [gs:CPU_INTERRUPT_REGS + 3 * 8]
	mov rsi, [gs:CPU_INTERRUPT_REGS + 4 * 8]
	mov rdi, [gs:CPU_INTERRUPT_REGS + 5 * 8]
	mov rbp, [gs:CPU_INTERRUPT_REGS + 6 * 8]
	mov rsp, [gs:CPU_INTERRUPT_REGS + 7 * 8]
	mov r8, [gs:CPU_INTERRUPT_REGS + 8 * 8]
	mov r9, [gs:CPU_INTERRUPT_REGS + 9 * 8]
	mov r10, [gs:CPU_INTERRUPT_REGS + 10 * 8]
	mov r11, [gs:CPU_INTERRUPT_REGS + 11 * 8]
	mov r12, [gs:CPU_INTERRUPT_REGS + 12 * 8]
	mov r13, [gs:CPU_INTERRUPT_REGS + 13 * 8]
	mov r14, [gs:CPU_INTERRUPT_REGS + 14 * 8]
	mov r15, [gs:CPU_INTERRUPT_REGS + 15 * 8]
	mov rax, [gs:CPU_INTERRUPT_REGS + 16 * 8]
	push rax
	popf
	mov rax, [gs:CPU_INTERRUPT_REGS + 0 * 8]
	ret

;int int_00h();	// #DE
global int_00h
int_00h:
	cli
	call saveregs
	mov rdi, 0			; number
	mov rsi, [rsp]		; RIP
	mov rdx, [rsp + 24]	; RSP
	xor rcx, rcx		; error code
	call exception_handler
	iretq
;int int_01h();	// #DB
global int_01h
int_01h:
	cli
	call saveregs
	mov rdi, 1			; number
	mov rsi, [rsp]		; RIP
	mov rdx, [rsp + 24]	; RSP
	xor rcx, rcx		; error code
	call exception_handler
	iretq
;int int_02h();	// NMI
global int_02h
int_02h:
	cli
	call saveregs
	mov rdi, 2			; number
	mov rsi, [rsp]		; RIP
	mov rdx, [rsp + 24]	; RSP
	xor rcx, rcx		; error code
	call exception_handler
	iretq
;int int_03h();	// #BP
global int_03h
int_03h:
	cli
	call saveregs
	mov rdi, 3			; number
	mov rsi, [rsp]		; RIP
	mov rdx, [rsp + 24]	; RSP
	xor rcx, rcx		; error code
	call exception_handler
	iretq
;int int_04h();	// #OF
global int_04h
int_04h:
	cli
	call saveregs
	mov rdi, 4			; number
	mov rsi, [rsp]		; RIP
	mov rdx, [rsp + 24]	; RSP
	xor rcx, rcx		; error code
	call exception_handler
	iretq
;int int_05h();	// #BR
global int_05h
int_05h:
	cli
	call saveregs
	mov rdi, 5			; number
	mov rsi, [rsp]		; RIP
	mov rdx, [rsp + 24]	; RSP
	xor rcx, rcx		; error code
	call exception_handler
	iretq
;int int_06h();	// #UD
global int_06h
int_06h:
	cli
	call saveregs
	mov rdi, 6			; number
	mov rsi, [rsp]		; RIP
	mov rdx, [rsp + 24]	; RSP
	xor rcx, rcx		; error code
	call exception_handler
	iretq
;int int_07h();	// #NM
global int_07h
int_07h:
	cli
	call saveregs
	mov rdi, 7			; number
	mov rsi, [rsp]		; RIP
	mov rdx, [rsp + 24]	; RSP
	xor rcx, rcx		; error code
	call exception_handler
	iretq
;int int_08h();	// #DF
global int_08h
int_08h:
	cli
	call saveregs
	mov rdi, 8			; number
	mov rsi, [rsp + 8]	; RIP
	mov rdx, [rsp + 32]	; RSP
	pop rcx				; error code
	call exception_handler
	iretq
;int int_09h();	// coprocessor
global int_09h
int_09h:
	cli
	call saveregs
	mov rdi, 9			; number
	mov rsi, [rsp]		; RIP
	mov rdx, [rsp + 24]	; RSP
	xor rcx, rcx		; error code
	call exception_handler
	iretq
;int int_0Ah();	// #TS
global int_0Ah
int_0Ah:
	cli
	call saveregs
	mov rdi, 10			; number
	mov rsi, [rsp + 8]	; RIP
	mov rdx, [rsp + 32]	; RSP
	pop rcx				; error code
	call exception_handler
	iretq
;int int_0Bh();	// #NP
global int_0Bh
int_0Bh:
	cli
	call saveregs
	mov rdi, 11			; number
	mov rsi, [rsp + 8]	; RIP
	mov rdx, [rsp + 32]	; RSP
	pop rcx				; error code
	call exception_handler
	iretq
;int int_0Ch();	// #SS
global int_0Ch
int_0Ch:
	cli
	call saveregs
	mov rdi, 12			; number
	mov rsi, [rsp + 8]	; RIP
	mov rdx, [rsp + 32]	; RSP
	pop rcx				; error code
	call exception_handler
	iretq
;int int_0Dh();	// #GP
global int_0Dh
int_0Dh:
	cli
	call saveregs
	mov rdi, 13			; number
	mov rsi, [rsp + 8]	; RIP
	mov rdx, [rsp + 32]	; RSP
	pop rcx				; error code
	call exception_handler
	iretq
;int int_0Eh();	// #PF page fault
global int_0Eh
int_0Eh:
	cli
	call saveregs
	mov rdi, 14			; number
	mov rsi, [rsp + 8]	; RIP
	mov rdx, [rsp + 32]	; RSP
	pop rcx				; error code
	call exception_handler
	iretq

;int int_10h();	// #MF
global int_10h
int_10h:
	cli
	call saveregs
	mov rdi, 16			; number
	mov rsi, [rsp]		; RIP
	mov rdx, [rsp + 24]	; RSP
	xor rcx, rcx		; error code
	call exception_handler
	iretq
;int int_11h();	// #AC
global int_11h
int_11h:
	cli
	call saveregs
	mov rdi, 17			; number
	mov rsi, [rsp + 8]	; RIP
	mov rdx, [rsp + 32]	; RSP
	pop rcx				; error code
	call exception_handler
	iretq
;int int_12h();	// #MC
global int_12h
int_12h:
	cli
	call saveregs
	mov rdi, 18			; number
	mov rsi, [rsp]		; RIP
	mov rdx, [rsp + 24]	; RSP
	xor rcx, rcx		; error code
	call exception_handler
	iretq
;int int_13h();	// #XF
global int_13h
int_13h:
	cli
	call saveregs
	mov rdi, 19			; number
	mov rsi, [rsp]		; RIP
	mov rdx, [rsp + 24]	; RSP
	xor rcx, rcx		; error code
	call exception_handler
	iretq

;int int_1Dh();	// #VC
global int_1Dh
int_1Dh:
	cli
	call saveregs
	mov rdi, 29			; number
	mov rsi, [rsp]		; RIP
	mov rdx, [rsp + 24]	; RSP
	xor rcx, rcx		; error code
	call exception_handler
	iretq
;int int_1Eh();	// #SX
global int_1Eh
int_1Eh:
	cli
	call saveregs
	mov rdi, 30			; number
	mov rsi, [rsp]		; RIP
	mov rdx, [rsp + 24]	; RSP
	xor rcx, rcx		; error code
	call exception_handler
	iretq
