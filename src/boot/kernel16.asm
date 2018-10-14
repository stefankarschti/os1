struc system_info_struct
	.cursorx:		resb 1
	.cursory:		resb 1
	.num_memory_blocks	resw 1
	.memory_blocks_ptr	resq 1
endstruc
struc memory_block_struct
	.start:		resq 1
	.length:	resq 1	
	.type:		resd 1
	.unused:	resd 1
endstruc

section .bss start=0x4000
system_info resb system_info_struct_size

e_entry		resq	1
e_phoff		resq	1
e_shoff		resq	1
e_phentsize	resw	1
e_phnum		resw	1
e_shentsize	resw	1
e_shnum		resw	1

p_offset	resq	1
p_vaddr		resq	1
p_filesz	resq	1
p_memsz		resq	1

memory_blocks resb memory_block_struct_size
;...

FREE_SPACE		equ		0x5000		; Pointer to free space
kernel_image	equ 	0x2000		; ELF kernel image

section .text
[org 0x1000]
[bits 16]
kernel_main16:
	; load kernel64.elf
	mov bx, kernel_image	            ; ES:BX = Address to load kernel into
	mov dh, 20		                	; DH    = Number of sectors to load
;	mov dl, [boot_device]               ; DL    = Drive number to load from
	mov cl, 9							; CL	= start sector
	call disk_load_sectors              ; Call disk load function
	jc .disk_error
	; success
	mov eax, [kernel_image]
	cmp eax, 0x464C457F
	jne .elf_error
	jmp .next
.disk_error:
	mov si, str_elf_fail_disk
	call print16
	jmp .stop
.elf_error:
	mov si, str_elf_fail_check
	call print16
.stop:
	hlt
	jmp .stop
.next:	
	mov si, str_kernel16_hello
	call print16
	
	; check long mode
	call check_long_mode
	jc no_long_mode
	
	; detect memory
.l2:
	mov di, memory_blocks
	call do_e820
	jnc .l3
	mov si, str_e820_failed
	call print16
	hlt
	jmp $			; can't detect memory. die here
.l3:
	; e820 success
	mov [system_info + system_info_struct.num_memory_blocks], bp
	mov eax, memory_blocks
	mov [system_info + system_info_struct.memory_blocks_ptr], eax
	xor eax, eax
	mov [system_info + system_info_struct.memory_blocks_ptr + 4], eax

	; TODO: detect video modes
	
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
str_kernel16_hello   	db "[kernel16] hello", 13, 10, 0
str_no_long_mode   		db "[kernel16] 64bit mode not available", 13, 10, 0
str_e820_failed			db "[kernel16] Memory detection failed", 13, 10, 0
str_elf_fail_disk    	db "[kernel16] ELF load disk error", 13, 10, 0
str_elf_fail_check    	db "[kernel16] ELF check failed", 13, 10, 0

%include "disk16.asm"
%include "biosmemory.asm"
%include "console16.asm"
%include "long64.asm"
%include "console64.asm"

[bits 64]
kernel_main64:
	mov rsi, str_kernel_hello
	call print64

	;; expand the kernel ELF at [kernel_image] (quick and dirty = no checks)
	;; EH
	; e_entry
	mov rsi, 0x18
	mov rax, [kernel_image + rsi]
	mov [e_entry], rax
	; e_phoff
	mov rsi, 0x20
	mov rax, [kernel_image + rsi]
	mov [e_phoff], rax
	; e_shoff
	mov rsi, 0x28
	mov rax, [kernel_image + rsi]
	mov [e_shoff], rax
	; e_phentsize
	mov rsi, 0x36
	mov ax, [kernel_image + rsi]
	mov [e_phentsize], ax
	; e_phnum
	mov rsi, 0x38
	mov ax, [kernel_image + rsi]
	mov [e_phnum], ax
	; e_shentsize
	mov rsi, 0x3A
	mov ax, [kernel_image + rsi]
	mov [e_shentsize], ax
	; e_shnum
	mov rsi, 0x3C
	mov ax, [kernel_image + rsi]
	mov [e_shnum], ax

	;; PH	
	; p_offset
	mov rsi, 0x08
	add rsi, [e_phoff]
	mov rax, [kernel_image + rsi]
	mov [p_offset], rax	
	; p_vaddr
	mov rsi, 0x10
	add rsi, [e_phoff]
	mov rax, [kernel_image + rsi]
	mov [p_vaddr], rax
	; p_filesz
	mov rsi, 0x20
	add rsi, [e_phoff]
	mov rax, [kernel_image + rsi]
	mov [p_filesz], rax
	; p_memsz
	mov rsi, 0x28
	add rsi, [e_phoff]
	mov rax, [kernel_image + rsi]
	mov [p_memsz], rax

	; load 
	cld
	mov rsi, [p_offset]
	add rsi, kernel_image
	mov rdi, [p_vaddr]
	mov rcx, [p_filesz]
	shr rcx, 3
	inc rcx
	rep movsq				; move program in place
	xor rax, rax			; clear .bss
	mov rcx, 0x10000
	shr rcx, 3
	inc rcx
	rep stosq
	mov rsp, rdi			; init stack at top of .bss
	mov rbp, rsp	

	; and jump
	mov rax, [e_entry]
	mov rdi, system_info
	call rax		; call kernel_main
.l3:		
	jmp $ ; stop here
	
	; todo:
	; console
	; memory mgr
	; keyboard driver
	; disk driver
	; filesystem driver	
	
	jmp $		; die
	

; Data
str_kernel_hello	db "[kernel64] hello", 10, 0 
;hexdigit			db "0123456789ABCDEF",0

; Tail
times 4096-($-$$) db 0xCC                ; Fill sectors

