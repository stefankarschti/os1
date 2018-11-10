; Load sectors LBA mode
; ES:DI = destination memory address
; EBX:EAX = start LBA address
; DL = drive
; CX = count sectors
;
lba:
.size 		dw	16
.count		dw	0
.offset		dw	0
.segment	dw	0
.lba0		dd	0
.lba1		dd	0
disk_read_lba:
	mov [lba.count], cx
	mov [lba.offset], di
	mov [lba.segment], es
	mov [lba.lba0], eax
	mov [lba.lba1], ebx
	
	; setup INT 13h call
	mov si, lba	
	mov ah, 42h
	; dl = drive number
	int 13h
	ret
