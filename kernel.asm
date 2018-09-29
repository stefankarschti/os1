struc system_info_struct
	.cursorx:	resb 1
	.cursory:	resb 1	
endstruc
struc memory_block_struct
	.start:		resq 1
	.length:	resq 1	
	.type:		resd 1
	.unused:	resd 1
endstruc

section .bss start=0x8000
system_info: resb system_info_struct_size
num_memory_blocks:	resw 1
memory_blocks: resb memory_block_struct_size
;...

align 0x1000, resb 1
FREE_SPACE	equ	$		; Pointer to free space

section .text
[bits 16]
[org 0x1000]
	mov ax, system_info
	mov bx, FREE_SPACE		
kernel_main16:
	; check long mode
	call CheckCPU
	jc no_long_mode
	
	; detect memory
	clc
	int 12h
	jc .l1	
	mov si, low_mem_msg
	call Print
	; having amount of KB in AX
	call PrintHex
	mov si, str_KB
	call Print	
	mov si, crlf
	call Print
	jmp .l2
.l1:	; no mem
	mov si, low_mem_error_msg
	call Print
.l2:
	mov si, str_e820_detection
	call Print
	mov edi, FREE_SPACE
	call do_e820
	jnc .l3
	mov si, str_e820_failed
	call Print
	jmp .l4
.l3:
	; e820 success
	mov si, str_e820_success
	call Print	
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
	mov si, no_long_mode_msg
	call Print
	jmp $

; use the INT 0x15, eax= 0xE820 BIOS function to get a memory map
; inputs: es:di -> destination buffer for 24 byte entries
; outputs: bp = entry count, trashes all registers except esi
do_e820:
	xor ebx, ebx		; ebx must be 0 to start
	xor bp, bp		; keep an entry count in bp
	mov edx, 0x0534D4150	; Place "SMAP" into edx
	mov eax, 0xe820
	mov [es:di + 20], dword 1	; force a valid ACPI 3.X entry
	mov ecx, 24		; ask for 24 bytes
	int 0x15
	jc .failed	; carry set on first call means "unsupported function"
	mov edx, 0x0534D4150	; Some BIOSes apparently trash this register?
	cmp eax, edx		; on success, eax must have been reset to "SMAP"
	jne .failed
	test ebx, ebx		; ebx = 0 implies list is only 1 entry long (worthless)
	je .failed
	jmp .jmpin
.e820lp:
	mov eax, 0xe820		; eax, ecx get trashed on every int 0x15 call
	mov [es:di + 20], dword 1	; force a valid ACPI 3.X entry
	mov ecx, 24		; ask for 24 bytes again
	int 0x15
	jc .e820f		; carry set means "end of list already reached"
	mov edx, 0x0534D4150	; repair potentially trashed register
.jmpin:
	jcxz .skipent		; skip any 0 length entries
	cmp cl, 20		; got a 24 byte ACPI 3.X response?
	jbe .notext
	test byte [es:di + 20], 1	; if so: is the "ignore this data" bit clear?
	je .skipent
.notext:
	mov ecx, [es:di + 8]	; get lower uint32_t of memory region length
	or ecx, [es:di + 12]	; "or" it with upper uint32_t to test for zero
	jz .skipent		; if length uint64_t is 0, skip entry
	inc bp			; got a good entry: ++count, move to next storage spot

	; print entry
	;; base address
	mov ax, [es:di + 6]
	call PrintHex
	mov ax, [es:di + 4]
	call PrintHex
	mov ax, [es:di + 2]
	call PrintHex
	mov ax, [es:di + 0]
	call PrintHex

	;; length
	mov si, space
	call Print
	mov ax, [es:di + 14]
	call PrintHex
	mov ax, [es:di + 12]
	call PrintHex
	mov ax, [es:di + 10]
	call PrintHex
	mov ax, [es:di + 8]
	call PrintHex

	;; type
	mov si, space
	call Print
	mov ax, [es:di + 18]
	call PrintHex
	mov ax, [es:di + 16]
	call PrintHex

	;; \n
	mov si, crlf
	call Print
	
	; done print
	add di, 24
.skipent:
	test ebx, ebx		; if ebx resets to 0, list is complete
	jne .e820lp
.e820f:
;	mov [mmap_ent], bp	; store the entry count
	clc			; there is "jc" on end of list to this point, so the carry must be cleared
	ret
.failed:
	stc			; "function unsupported" error exit
	ret
crlf		   	db 13, 10, 0
space		   	db " ", 0
no_long_mode_msg   	db "[kernel16] 64bit error", 13, 10, 0
low_mem_error_msg   	db "[kernel16] low mem error", 13, 10, 0
low_mem_msg	   	db "[kernel16] low mem detected ", 0
str_e820_failed		db "[kernel16] INT 15h AX=E820h failed", 13, 10, 0
str_e820_success	db "[kernel16] INT 15h AX=E820h success", 13, 10, 0
str_e820_detection	db "[kernel16] INT 15h AX=E820h memory detection:", 13, 10, 0
str_KB			db "KB", 0

	
%include "long64.asm"

[bits 64]
kernel_main64:
	mov ecx, 80 * 8
.lz:
	mov esi, str_a
	push rcx
	call print64
	pop rcx
	loop .lz
	
	jmp $
	
	mov esi, kernel_hello_msg
	call print64
	mov esi, kernel_hello_msg
	call print64
	mov esi, kernel_hello_msg
	call print64
	mov esi, str_test
	call print64
	mov esi, str_test
	call print64
	mov esi, str_test
	call print64
	jmp kernel_main64
		
	; todo:
	; console driver
	; memory mgr
	; keyboard driver
	; disk driver
	; filesystem driver
	
	
	jmp $		; die
	
print64:
	mov edi, VIDEO_MEM
	; compose edi	
	xor rax, rax
	mov al, byte [system_info + system_info_struct.cursory]
	imul eax, 160	
	add edi, eax
	xor rax, rax
	mov al, byte [system_info + system_info_struct.cursorx]
	shl eax, 1
	add edi, eax

.go:
	mov al, [esi]
	cmp al, 0
	je .done
	
	; \n
	cmp al, 0xa
	jne .l1
	mov eax, edi
	sub eax, VIDEO_MEM
	mov ecx, 160
	xor edx, edx
	idiv ecx
	inc eax
	cmp eax, 25
	jne .l2
	; scroll up
	mov ecx, 24 * 160 / 8
	push rsi
	push rdi
	push rax
	mov esi, VIDEO_MEM + 160
	mov edi, VIDEO_MEM
	rep movsq 
	mov ecx, 80 / 8
	mov rax, 0x0720072007200720
	rep stosq
	pop rax
	pop rdi
	pop rsi
	dec eax
.l2:	
	imul eax, 160
	add eax, VIDEO_MEM
	mov edi, eax
	jmp .next
.l1:	
	mov [edi], al
	inc edi
	inc edi
	; if edi overflows, scroll up
	cmp edi, VIDEO_MEM + 160 * 25
	jne .next
	sub edi, 160
	; scroll up
	mov ecx, 24 * 160 / 8
	push rsi
	push rdi
	mov esi, VIDEO_MEM + 160
	mov edi, VIDEO_MEM
	rep movsq 
	mov ecx, 160 / 8
	mov rax, 0x0720072007200720
	rep stosq
	pop rdi
	pop rsi	
.next:
	inc esi
	jmp .go
.done:
	mov eax, edi
	sub eax, VIDEO_MEM
	mov ebx, eax
	shr ebx, 1
	mov ecx, 160
	xor edx, edx
	idiv ecx
	mov byte [system_info + system_info_struct.cursory], al
	shr edx, 1
	mov byte [system_info + system_info_struct.cursorx], dl

	; cursor low port to VGA index register
	mov al, 0Fh
	mov dx, 3D4h
	out dx, al
 
	; cursor low position to VGA data register
	mov ax, bx
	mov dx, 3D5h
	out dx, al
 
	; cursor high port to VGA index register
	mov al, 0Eh
	mov dx, 3D4h
	out dx, al
 
	; cursor high position to VGA data register
	mov ax, bx
	shr ax, 8
	mov dx, 3D5h
	out dx, al
	ret


; Data
VIDEO_MEM equ 0xB8000
kernel_hello_msg   db "[kernel64] hello at ", 10, "line 2", 10, 0 
str_test	db "[kernel64] test", 10, 0
str_a		db "a", 0

; Tail
times 2048-($-$$) db 0xCC                ; Fill sectors

