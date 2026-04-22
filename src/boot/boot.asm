[bits 16]
[org 0x7c00]
%include "../kernel/memory_layout.inc"
%include "image_layout.inc"

loader_main16		equ LOADER16_LOAD_ADDRESS

_start:
	cli
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov sp, loader_main16
	mov [boot_device], dl

	; Stage 0 only needs to pull the contiguous stage-1 image. Using classic
	; CHS reads here avoids the BIOS EDD path that was overwriting the boot
	; sector immediately after INT 13h returned.
	mov ah, 0x08
	mov dl, [boot_device]
	int 0x13
	jc .stop
	and cx, 0x003F
	mov [sectors_per_track], cx
	xor ax, ax
	mov al, dh
	inc ax
	mov [head_count], ax

	mov word [current_lba], LOADER16_IMAGE_START_LBA
	mov bx, loader_main16
	mov si, 1

.load_next:
	or si, si
	jz .loaded

	mov ax, [current_lba]
	xor dx, dx
	div word [sectors_per_track]
	mov di, dx
	inc di

	xor dx, dx
	div word [head_count]

	mov ch, al
	mov cx, di
	mov al, ah
	and al, 0x03
	shl al, 6
	or cl, al
	mov dh, dl

	mov ax, 0x0201
	mov dl, [boot_device]
	int 0x13
	jc .stop

	add bx, 512
	inc word [current_lba]
	dec si
	jmp .load_next

.loaded:
	cmp word [loader_main16], 0x7733
	jne .stop

	mov dl, [boot_device]
	jmp loader_main16 + 2

.stop:
	hlt
	jmp .stop

boot_device			db 0x00
sectors_per_track	dw 0
head_count			dw 0
current_lba			dw 0

times 510-($-$$) db 0x00
dw 0xAA55
