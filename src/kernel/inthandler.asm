
extern exception_handler

; save registers at address 0x10000
global saveregs
saveregs:
.base equ 0x10000
	mov [.base + 0 * 8], rax
	mov [.base + 1 * 8], rbx
	mov [.base + 2 * 8], rcx
	mov [.base + 3 * 8], rdx
	mov [.base + 4 * 8], rsi
	mov [.base + 5 * 8], rdi
	mov [.base + 6 * 8], rbp
	mov [.base + 7 * 8], rsp
	mov [.base + 8 * 8], r8
	mov [.base + 9 * 8], r9
	mov [.base + 10 * 8], r10
	mov [.base + 11 * 8], r11
	mov [.base + 12 * 8], r12
	mov [.base + 13 * 8], r13
	mov [.base + 14 * 8], r14
	mov [.base + 15 * 8], r15
	pushf
	pop rax
	mov [.base + 16 * 8], rax
	mov rax, cr3
	mov [.base + 17 * 8], rax
	mov rax, [.base + 0 * 8]
	ret

; restore registers from address 0x10000
global restoreregs
restoreregs:
.base equ 0x10000
	mov rax, [.base + 0 * 8]
	mov rbx, [.base + 1 * 8]
	mov rcx, [.base + 2 * 8]
	mov rdx, [.base + 3 * 8]
	mov rsi, [.base + 4 * 8]
	mov rdi, [.base + 5 * 8]
	mov rbp, [.base + 6 * 8]
	mov rsp, [.base + 7 * 8]
	mov r8, [.base + 8 * 8]
	mov r9, [.base + 9 * 8]
	mov r10, [.base + 10 * 8]
	mov r11, [.base + 11 * 8]
	mov r12, [.base + 12 * 8]
	mov r13, [.base + 13 * 8]
	mov r14, [.base + 14 * 8]
	mov r15, [.base + 15 * 8]
	mov rax, [.base + 16 * 8]
	push rax
	popf
	mov rax, [.base + 0 * 8]
	ret

;int int_00h();	// #DE
global int_00h
int_00h:
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
	call saveregs
	mov rdi, 30			; number
	mov rsi, [rsp]		; RIP
	mov rdx, [rsp + 24]	; RSP
	xor rcx, rcx		; error code
	call exception_handler
	iretq
