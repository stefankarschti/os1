[bits 16]
[org 0x7c00]
kernel_main16		equ 0x1000           ; Location for kernel in memory
_start:
	jmp 0 : _main
kernel_num_sectors	db 8
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

	mov bx, kernel_main16	                ; ES:BX = Address to load kernel into
	mov dh, [kernel_num_sectors]            ; DH    = Number of sectors to load
	mov dl, [boot_device]                   ; DL    = Drive number to load from
	mov cl, 2								; CL	= start sector
	call disk_load_sectors                  ; Call disk load function
	jc .disk_error
	mov dl, [boot_device]
	jmp kernel_main16						; call kernel16 main
.disk_error:
	mov si, str_disk_error
	call print16
.stop:
	hlt
	jmp .stop
        
%include "disk16.asm"
%include "console16.asm"

; Data
;hexdigit		db "0123456789ABCDEF",0
str_boot_hello   	db "[boot] hello", 13, 10, 0
str_disk_error   	db "[boot] disk error", 13, 10, 0
boot_device 		db 0x00                     ; Get byte to store boot drive number

; Tail
times 510-($-$$) db 0x00                ; Fill bootloader to 512-bytes
dw 0xAA55                               ; Magic word signature


