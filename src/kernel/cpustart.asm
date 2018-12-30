; trampoline code
; start a new CPU
; LOAD at 0x1000
; memory format:
; at 0x20: uint64_t CPU_PAGE => RSP = CPU_PAGE + 0x1000
; at 0x28: uint64_t RIP
; at 0x30: uint64_t CR3
; at 0x38: uint16_t IDT.Length
; at 0x3A: uint32_t IDT.Base
; at 0x3E: end
; code start at 0x1000

%define CODE_SEG     0x0008
%define DATA_SEG     0x0010
GDTT			equ 0x0
P_CPU_PAGE		equ 0x20
P_RIP			equ 0x28
P_CR3			equ 0x30
P_IDT			equ 0x38

[bits 16]

global cpustart_begin
cpustart_begin:
	cli
	xor eax, eax
	mov ds, ax
	mov es, ax
	mov ss, ax

	; Disable IRQs
	mov al, 0xFF
	out 0xA1, al
	out 0x21, al
	nop
	nop
	lidt [P_IDT]

	; Enter long mode.
	mov eax, 10100000b                ; Set the PAE and PGE bit.
	mov cr4, eax

	mov edx, [P_CR3]					; Point CR3 at the PML4.
	mov cr3, edx

	mov ecx, 0xC0000080               ; Read from the EFER MSR.
	rdmsr

	or eax, 0x00000100                ; Set the LME bit.
	wrmsr

	mov ebx, cr0                      ; Activate long mode -
	or ebx,0x80000001                 ; - by enabling paging and protection simultaneously.
	mov cr0, ebx

	; load GDT
	lgdt [GDTT + 3 * 8 + 2]
	jmp CODE_SEG:(0x1000 + LongMode - cpustart_begin)         ; Load CS with 64 bit segment and flush the instruction cache

[BITS 64]
LongMode:
	mov ax, DATA_SEG
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	mov rsi, [P_CPU_PAGE]
	mov rbp, rsi
	add rbp, 0x1000
	mov rsp, rbp

	mov rax, [P_RIP]
	call rax

.die:
	hlt
	jmp .die

global cpustart_end
cpustart_end:
