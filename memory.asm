[bits 64]
section .text

; System V ABI cdecl call
; parameters: RDI, RSI, RDX, RCX, R8, R9 then on stack RTL
; float params: XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6 and XMM7
; int return: RDX:RAX
; float return: XMM1:XMM0
; preserve: RBX, RBP, R12-R15

;
; void memset(void* ptr, uint8_t value, size_t num)
;
global memset
memset:
	push rbp
	mov rbp, rsp
	
	mov rcx, rdx		; num
	mov rax, rsi		; value
	cld
	rep stosb
	
	mov rsp, rbp
	pop rbp
	ret

;
; void memsetw(void* ptr, uint16_t value, size_t num)
;
global memsetw
memsetw:
	push rbp
	mov rbp, rsp
	
	mov rcx, rdx		; num
	shr rcx, 1
	mov rax, rsi		; value
	cld
	rep stosw
	
	mov rsp, rbp
	pop rbp
	ret

;
; void memcpy(void* dest, void* source, size_t num)
;
global memcpy
memcpy:
	push rbp
	mov rbp, rsp
	
	mov rcx, rdx		; num
	cld
	rep movsb
	
	mov rsp, rbp
	pop rbp
	ret

