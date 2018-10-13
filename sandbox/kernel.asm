struc system_info_struct
	.cursorx:		resb 1
	.cursory:		resb 1
	.num_memory_blocks	resw 1
endstruc
struc memory_block_struct
	.start:		resq 1
	.length:	resq 1	
	.type:		resd 1
	.unused:	resd 1
endstruc

STACK_PTR 	equ	0x9000		; downwards
section .bss start=0x9000		; upwards
system_info resb system_info_struct_size
memory_blocks resb memory_block_struct_size
;...

align 0x1000, resb 1
FREE_SPACE	equ	$		; Pointer to free space

section .text
[org 0x1000]
[bits 16]
kernel_main16:
	; check long mode
	call check_long_mode
	jc no_long_mode
	
	; detect memory
.l2:
	mov si, str_e820_detection
	call print16
	mov edi, memory_blocks
	call do_e820
	jnc .l3
	mov si, str_e820_failed
	call print16
	hlt
	jmp $			; can't detect memory. die here
.l3:
	; e820 success
	mov [system_info + system_info_struct.num_memory_blocks], bp
	mov si, str_e820_success
	call print16
	xor ecx, ecx
	mov cx, [system_info + system_info_struct.num_memory_blocks]
	mov edi, memory_blocks
	xor ebx, ebx		; edx:ebx = total available memory
	xor edx, edx
.l5:		
	; print entry
	push cx
	
	;; base address
	mov ax, [es:di + 6]
	call print16_whex
	mov ax, [es:di + 4]
	call print16_whex
	mov ax, [es:di + 2]
	call print16_whex
	mov ax, [es:di + 0]
	call print16_whex

	;; length
	mov si, space
	call print16
	mov ax, [es:di + 14]
	call print16_whex
	mov ax, [es:di + 12]
	call print16_whex
	mov ax, [es:di + 10]
	call print16_whex
	mov ax, [es:di + 8]
	call print16_whex

	;; type
	mov si, space
	call print16
	mov ax, [es:di + 18]
	call print16_whex
	mov ax, [es:di + 16]
	call print16_whex

	cmp ax, 1
	jne .l6
	mov eax, [es:di + 8]
	add ebx, eax
	mov eax, [es:di + 12]
	add edx, eax
.l6:
	;; \n
	mov si, crlf
	call print16
	add di, 24
	
	; done print entry
	pop cx
	loop .l5	
.l4:

	; detect video modes
	
	; save cursor position
	mov ah, 03h
	xor bh, bh
	int 10h
	mov byte [system_info + system_info_struct.cursorx], dl
	mov byte [system_info + system_info_struct.cursory], dh
	
	; switch to long mode	
	mov edi, FREE_SPACE
	jmp SwitchToLongMode
	
no_long_mode:
	mov si, str_no_long_mode
	call print16
	jmp $

crlf		   	db 13, 10, 0
space		   	db " ", 0
str_no_long_mode   	db "[kernel16] 64bit mode not available", 13, 10, 0
str_e820_failed		db "[kernel16] INT 15h AX=E820h failed", 13, 10, 0
str_e820_success	db "[kernel16] INT 15h AX=E820h success", 13, 10, 0
str_e820_detection	db "[kernel16] INT 15h AX=E820h memory detection:", 13, 10, 0

%include "biosmemory.asm"
%include "console16.asm"
%include "long64.asm"
%include "console64.asm"

[bits 64]
kernel_main64:
;	mov rsp, STACK_PTR
;	mov rbp, rsp
	
	mov rsi, str_kernel_hello
	call print64
	
	jmp $

	; print total memory available
	xor rbx, rbx
	xor rcx, rcx
	mov cx, [system_info + system_info_struct.num_memory_blocks]
	mov rsi, memory_blocks
.l1:
	mov eax, [esi + 16]	; type
	cmp eax, 1
	jne .l2
	add rbx, qword [esi + 8]
.l2:
	add esi, 24
	loop .l1
	
	mov esi, str_memory_available
	call print64
	mov rax, rbx
	;call print64_qhex
	mov esi, str_bytes
	call print64
	mov esi, crlf + 1
	call print64
		
	; todo:
	; console driver
	; memory mgr
	; keyboard driver
	; disk driver
	; filesystem driver
	
	
	jmp $		; die
	


; Data
str_kernel_hello	db "[kernel64] hello", 10, "[kernel64] initializing...", 10, 0 
str_memory_available	db "[kernel64] Total memory: ", 0
str_bytes		db " bytes", 0
hexdigit		db "0123456789ABCDEF",0

; Tail
times 2048-($-$$) db 0xCC                ; Fill sectors

