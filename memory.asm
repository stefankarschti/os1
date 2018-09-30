[bits 64]
section .text
global memset
memset:
	push rbp
	mov rbp, rsp
	
	mov rcx, [rbp + 16]			; len
	mov al,  [rbp + 16 + 8]		; value
	mov rdi, [rbp + 16 + 8 + 1]	; ptr
	cld
	rep stosb
	
	mov rsp, rbp
	pop rbp
	ret

