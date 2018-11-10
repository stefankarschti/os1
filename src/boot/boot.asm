[bits 16]
[org 0x7c00]
loader_main16		equ 0x1000           ; Location for kernel in memory
_start:
	jmp 0 : _main
loader_num_sectors	dw 8
_main:
	cli
	mov ax, cs
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov fs, ax
	mov gs, ax
	mov [boot_device], dl					; save boot drive
	mov ebp, 0x1000							; set up basic stack 
	mov esp, ebp
	
	mov si, str_boot_hello
	call print16

	mov si, str_boot_dev
	call print16
	
	xor ax, ax
	mov al, [boot_device]
	call print16_whex
	mov si, str_crlf
	call print16
	
	mov di, loader_main16	                ; ES:DI = Address to load kernel into
	xor ebx, ebx
	mov eax, 1								; EBX:EAX = start LBA address
	mov dl, [boot_device]                   ; DL    = Drive number to load from
	mov cx, word [loader_num_sectors]       ; CX    = Number of sectors to load
	call disk_read_lba		                ; Call disk load function
	jc .disk_error

	mov ax, [loader_main16]
	xor ax, 0x7733
	jz .go
	mov si, str_sig_err
	call print16
	jmp .stop	
.go:
	mov dl, [boot_device]
	jmp loader_main16 + 2						; call kernel16 main
.disk_error:
	mov si, str_disk_error
	call print16
.stop:
	hlt
	jmp .stop
        
%include "disk16.asm"
%include "console16.asm"

; Data
hexdigit		db "0123456789ABCDEF",0
str_crlf		db 13,10,0
str_boot_hello   	db "[boot] hello", 13, 10, 0
str_boot_dev		db "[boot] booting from device ", 0
str_sig_err			db "[boot] signature error", 13, 10, 0
str_disk_error   	db "[boot] disk error", 13, 10, 0
boot_device 		db 0x00                     ; Get byte to store boot drive number

; Tail
times 510-($-$$) db 0x00                ; Fill bootloader to 512-bytes
dw 0xAA55                               ; Magic word signature


