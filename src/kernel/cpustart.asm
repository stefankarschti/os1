; trampoline code
; start a new CPU
; LOAD at the shared low-memory trampoline page
; memory format:
; at AP_STARTUP_CPU_PAGE_ADDRESS: uint64_t CPU_PAGE => RSP = CPU_PAGE + PAGE_SIZE
; at AP_STARTUP_RIP_ADDRESS: uint64_t RIP
; at AP_STARTUP_CR3_ADDRESS: uint64_t CR3
; at AP_STARTUP_IDT_ADDRESS: uint16_t IDT.Length + uint32_t IDT.Base
; code start at AP_TRAMPOLINE_ADDRESS

%include "memory_layout.inc"

%define CODE_SEG     0x0008
%define DATA_SEG     0x0010
P_CPU_PAGE		equ AP_STARTUP_CPU_PAGE_ADDRESS
P_RIP			equ AP_STARTUP_RIP_ADDRESS
P_CR3			equ AP_STARTUP_CR3_ADDRESS
P_IDT			equ AP_STARTUP_IDT_ADDRESS

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

	; Match the BSP boot path: APs also enable NXE before they inherit kernel
	; page tables that may contain non-executable mappings.
	or eax, 0x00000900                ; Set the LME and NXE bits.
	wrmsr

	mov ebx, cr0                      ; Activate long mode -
	or ebx,0x80000001                 ; - by enabling paging and protection simultaneously.
	mov cr0, ebx

	; Load the trampoline-local GDT so AP startup does not depend on whatever
	; low-memory bootstrap state the active boot frontend happened to leave at 0.
	lgdt [AP_TRAMPOLINE_ADDRESS + GdtPointer - cpustart_begin]
	jmp CODE_SEG:(AP_TRAMPOLINE_ADDRESS + LongMode - cpustart_begin)         ; Load CS with 64 bit segment and flush the instruction cache

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
	add rbp, PAGE_SIZE
	mov rsp, rbp

	mov rax, [P_RIP]
	call rax

.die:
	hlt
	jmp .die

align 8
GdtTable:
	dq 0x0000000000000000
	dq 0x00209A0000000000
	dq 0x0000920000000000

align 4
GdtPointer:
	dw GdtPointer - GdtTable - 1
	dd AP_TRAMPOLINE_ADDRESS + GdtTable - cpustart_begin

global cpustart_end
cpustart_end:
