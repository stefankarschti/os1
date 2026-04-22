%include "../kernel/memory_layout.inc"

struc boot_text_console_info_struct
	.columns:		resw 1
	.rows:			resw 1
	.cursor_x:		resw 1
	.cursor_y:		resw 1
endstruc
struc boot_framebuffer_info_struct
	.physical_address:	resq 1
	.width:				resd 1
	.height:			resd 1
	.pitch_bytes:		resd 1
	.bits_per_pixel:	resw 1
	.pixel_format:		resw 1
endstruc
struc boot_memory_region_struct
	.physical_start:	resq 1
	.length:			resq 1
	.type:				resd 1
	.attributes:		resd 1
endstruc
struc boot_module_info_struct
	.physical_start:	resq 1
	.length:			resq 1
	.name:				resq 1
endstruc
struc boot_info_struct
	.magic:					resq 1
	.version:				resd 1
	.source:				resd 1
	.kernel_physical_start:	resq 1
	.kernel_physical_end:	resq 1
	.rsdp_physical:			resq 1
	.smbios_physical:		resq 1
	.command_line:			resq 1
	.bootloader_name:		resq 1
	.text_console:			resb boot_text_console_info_struct_size
	.framebuffer:			resb boot_framebuffer_info_struct_size
	.memory_map:			resq 1
	.memory_map_count:		resd 1
	.reserved0:				resd 1
	.modules:				resq 1
	.module_count:			resd 1
	.reserved1:				resd 1
endstruc

section .bss start=BOOT_INFO_ADDRESS
boot_info resb boot_info_struct_size

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

section .bss.boot_memory_map start=BOOT_MEMORY_REGION_BUFFER_ADDRESS
boot_memory_map resb boot_memory_region_struct_size * BOOT_MEMORY_REGION_CAPACITY

memory_pages	equ		EARLY_LONG_MODE_PAGE_TABLES_ADDRESS
kernel_image	equ 	KERNEL_IMAGE_LOAD_ADDRESS

section .text
[org LOADER16_LOAD_ADDRESS]
[bits 16]
	dw	0x7733
loader_main16:
	mov [boot_device], dl
	mov si, str_loader16_hello
	call print16
	
	; enable A20
	call enable_a20_fast
	
	; check A20
	call check_a20
	or ax, ax
	jz .a20disabled
	mov si, str_a20
	call print16
	mov si, str_on
	call print16
	jmp .a20_next
.a20disabled:
	mov si, str_a20
	call print16
	mov si, str_off
	call print16
	jmp $ ; // can't work with this
.a20_next:
	mov si, crlf
	call print16

	; check long mode
	call check_long_mode
	jc no_long_mode

	; load kernel64.elf
	; start sector = 9
	; sector count = 256 (128k)
	; destination = kernel_image

	; 9(64) -> kernel_image + 0
	mov ax, kernel_image >> 4
	mov	es, ax
	mov di, 0				            ; ES:DI = Address to load kernel into
	xor ebx, ebx
	mov eax, 9							; EBX:EAX = start LBA address
	mov dl, [boot_device]               ; DL    = Drive number to load from
	mov cx, 64		                	; CX    = Number of sectors to load
	call disk_read_lba              	; Call disk load function
	jc .disk_error

	; 73(64) -> kernel_image + 0x8000
	mov ax, kernel_image >> 4
	mov	es, ax
	mov di, 0x8000			            ; ES:DI = Address to load kernel into
	xor ebx, ebx
	mov eax, 73							; EBX:EAX = start LBA address
	mov dl, [boot_device]               ; DL    = Drive number to load from
	mov cx, 64		                	; CX    = Number of sectors to load
	call disk_read_lba              	; Call disk load function
	jc .disk_error

	; 137(64) -> kernel_image + 0x10000
	mov ax, kernel_image >> 4
	add ax, 0x1000
	mov	es, ax
	mov di, 0			            ; ES:DI = Address to load kernel into
	xor ebx, ebx
	mov eax, 137							; EBX:EAX = start LBA address
	mov dl, [boot_device]               ; DL    = Drive number to load from
	mov cx, 64		                	; CX    = Number of sectors to load
	call disk_read_lba              	; Call disk load function
	jc .disk_error

	; 201(64) -> kernel_image + 0x18000
	mov ax, kernel_image >> 4
	add ax, 0x1000
	mov	es, ax
	mov di, 0x8000			            ; ES:DI = Address to load kernel into
	xor ebx, ebx
	mov eax, 201						; EBX:EAX = start LBA address
	mov dl, [boot_device]               ; DL    = Drive number to load from
	mov cx, 64		                	; CX    = Number of sectors to load
	call disk_read_lba              	; Call disk load function
	jc .disk_error

	; success
	mov eax, kernel_image >> 4
	mov es, ax
	mov eax, [es:0]
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
	xor ax, ax
	mov es, ax

	; Zero the published handoff block first so BIOS-only boot leaves future
	; fields in a defined state until later boot paths populate them.
	mov di, boot_info
	mov cx, boot_info_struct_size / 2
	cld
	rep stosw
	mov dword [boot_info + boot_info_struct.version], BOOT_INFO_VERSION
	mov dword [boot_info + boot_info_struct.source], BOOT_SOURCE_BIOS_LEGACY
	mov word [boot_info + boot_info_struct.text_console + boot_text_console_info_struct.columns], BOOT_TEXT_COLUMNS
	mov word [boot_info + boot_info_struct.text_console + boot_text_console_info_struct.rows], BOOT_TEXT_ROWS

	; save cursor position
	mov ah, 03h
	xor bh, bh
	int 10h
	xor ax, ax
	mov al, dl
	mov word [boot_info + boot_info_struct.text_console + boot_text_console_info_struct.cursor_x], ax
	xor ax, ax
	mov al, dh
	mov word [boot_info + boot_info_struct.text_console + boot_text_console_info_struct.cursor_y], ax

	; detect memory
.l2:
	mov di, boot_memory_map
	call do_e820
	jnc .l3
	mov si, str_e820_failed
	call print16
	hlt
	jmp $			; can't detect memory. die here
.l3:
	; e820 success
	xor eax, eax
	mov ax, bp
	mov dword [boot_info + boot_info_struct.memory_map_count], eax
	mov eax, boot_memory_map
	mov dword [boot_info + boot_info_struct.memory_map], eax
	xor eax, eax
	mov dword [boot_info + boot_info_struct.memory_map + 4], eax

	; TODO: detect video modes

	; switch to long mode
	mov edi, memory_pages
	jmp SwitchToLongMode
	
no_long_mode:
	mov si, str_no_long_mode
	call print16
	jmp $

crlf		   	db 13, 10, 0
newline			db 10, 0
space		   	db " ", 0
str_loader16_hello   	db "[loader16] hello", 13, 10, 0
str_no_long_mode   		db "[loader16] 64bit mode not available", 13, 10, 0
str_e820_failed			db "[loader16] Memory detection failed", 13, 10, 0
str_elf_fail_disk    	db "[loader16] ELF load disk error", 13, 10, 0
str_elf_fail_check    	db "[loader16] ELF check failed", 13, 10, 0
str_a20					db "[loader16] A20 ", 0
str_on					db "on", 0
str_off					db "off", 0
str_bootloader_name		db "bios-loader", 0
boot_device 			db 0x00

%include "disk16.asm"
%include "biosmemory.asm"
%include "console16.asm"
%include "long64.asm"
%include "console64.asm"

[bits 64]
loader_main64:
	mov rsi, str_loader_hello
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

	; Publish the immutable pieces of the BIOS boot contract right before the
	; kernel takes over. The kernel copies this block immediately on entry.
	mov rax, BOOT_INFO_MAGIC
	mov [boot_info + boot_info_struct.magic], rax
	mov rax, str_bootloader_name
	mov [boot_info + boot_info_struct.bootloader_name], rax
	mov rax, boot_memory_map
	mov [boot_info + boot_info_struct.memory_map], rax
	mov rax, [p_vaddr]
	mov [boot_info + boot_info_struct.kernel_physical_start], rax
	add rax, [p_memsz]
	mov [boot_info + boot_info_struct.kernel_physical_end], rax

    ; clear location
	mov rdi, [p_vaddr]
	xor rax, rax
	mov rcx, [p_memsz]
	shr rcx, 3
	cld
	rep stosq

    ; load program
	mov rsi, [p_offset]
	add rsi, kernel_image
	mov rdi, [p_vaddr]
	mov rcx, [p_filesz]
	shr rcx, 3
	cld
	rep movsq				; move program in place
	
	; clear .bss
	mov rcx, [p_memsz]
	sub rcx, [p_filesz]
	shr rcx, 3
	cld
	xor rax, rax
	rep stosq

    ; init program stack
	mov rbp, [p_vaddr]
	add rbp, [p_memsz]
	add rbp, PAGE_SIZE
	shr rbp, 12
	shl rbp, 12
	mov rsi, rbp			; base of stack: param to kernel main
	add rbp, PAGE_SIZE
	mov rsp, rbp			; give at least 4k stack

	; jump
	mov rax, [e_entry]
	mov rdi, boot_info
	call rax		; call KernelMain
.l3:		
	hlt
        jmp .l3 ; stop here

; Data
str_loader_hello	db "[loader64] hello", 10, 0
hexdigit			db "0123456789ABCDEF",0

; Tail
times 4096-($-$$) db 0xCC                ; Fill sectors
