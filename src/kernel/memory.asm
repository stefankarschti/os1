[bits 64]
section .text

; System V ABI cdecl call
; parameters: RDI, RSI, RDX, RCX, R8, R9 then on stack RTL
; float params: XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6 and XMM7
; int return: RDX:RAX
; float return: XMM1:XMM0
; preserve: RBX, RBP, R12-R15

global load_gdt
%define CODE_SEG     0x0008
%define DATA_SEG     0x0010
load_gdt:
	cli
	lgdt [GDT.Pointer]                ; Load GDT.Pointer defined below.
;	mov rbp, rsp
;	push QWORD DATA_SEG
;	push rbp
;	pushfq
;	pushfq
;	pop rax
;	and rax, 1111111111111111111111111111111111111111111111101011111011111111b
;	push rax
;	popfq
	push QWORD CODE_SEG
	push QWORD Flush
	retq

ALIGN 8
GDT:
.Null:
dq 0x0000000000000000             ; Null Descriptor - should be present.
.Code:
dq 0x00209A0000000000             ; 64-bit code descriptor (exec/read).
dq 0x0000920000000000             ; 64-bit data descriptor (read/write).
ALIGN 4
dw 0                              ; Padding to make the "address of the GDT" field aligned on a 4-byte boundary
.Pointer:
dw $ - GDT - 1                    ; 16-bit Size (Limit) of GDT.
dd GDT                            ; 32-bit Base Address of GDT. (CPU will zero extend to 64-bit)

Flush:
	mov ax, DATA_SEG
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	; Display "64 bit"
	mov rdi, 0xB8000 + 160
	mov rax, 0x1F621F201F341F36
	mov [rdi],rax

	mov eax, 0x1F741F69
	mov [rdi + 8], eax
	ret

;
; void memset(void* ptr, uint8_t value, uint64_t num)
;
global memset
memset:
	mov rcx, rdx		; num
	mov rax, rsi		; value
	cld
	rep stosb
	ret

;
; void memsetw(void* ptr, uint16_t value, uint64_t num)
;
global memsetw
memsetw:
	mov rcx, rdx		; num
	shr rcx, 1
	mov rax, rsi		; value
	cld
	rep stosw
	ret

;
; void memsetw(void* ptr, uint16_t value, uint64_t num)
;
global memsetd
memsetd:
	mov rcx, rdx		; num
	shr rcx, 2
	mov rax, rsi		; value
	cld
	rep stosd
	ret

;
; void memsetq(void* ptr, uint16_t value, uint64_t num)
;
global memsetq
memsetq:
	mov rcx, rdx		; num
	shr rcx, 3
	mov rax, rsi		; value
	cld
	rep stosq
	ret

;
; void memcpy(void* dest, void* source, uint64_t num)
;
global memcpy
memcpy:
;	push rbp
;	mov rbp, rsp
	
	mov rcx, rdx		; num
	cld
	rep movsb
	
;	mov rsp, rbp
;	pop rbp
	ret

