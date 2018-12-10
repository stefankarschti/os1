[bits 64]
section .text

; System V ABI cdecl call
; parameters: RDI, RSI, RDX, RCX, R8, R9 then on stack RTL
; float params: XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6 and XMM7
; int return: RDX:RAX
; float return: XMM1:XMM0
; preserve: RBX, RBP, R12-R15

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

