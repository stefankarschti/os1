; void disk_load_sectors(char* addr, int disk, int num);
;   es:bx = [addr]
;	dl = [drive number]	
;   dh = [count sector]
;   cl = [start sector]
; Load dh sectors from disk dl into address es:bx
disk_load_sectors:
	pusha                               ; Preserve register values in stack

	push dx                             ; Store [DH:DL], DH = number of sectors to load 
	mov ah, 0x02                        ; BIOS sector load
	mov al, DH                          ; AL = DH = number of sectors to load
	mov ch, 0x00                        ; CH = Cylinder = 0
	mov dh, 0x00                        ; DH = Head = 0
;	mov cl, 0x02                        ; CL = Base Sector = 2
	int 0x13                            ; BIOS interrupt
	pop dx
	jc .disk_load_sectors_fail           ; Jump if error (carry flag)
	cmp dh, al                          ; Compare number of actual loaded sectors
	jne .disk_load_sectors_fail          ; Jump if error (incorrect number of sectors)
	
	popa                                ; Restore register values
	ret                                 ; Return function call

.disk_load_sectors_fail:
	stc
	popa
	ret

;   es:bx = [addr]
;	dl = [drive number]	
;   dh = [count sector]
;   cl = [start sector]
; Load dh sectors from disk dl into address es:bx
lba:
.size 		dw	16
.count		dw	0
.offset		dw	0
.segment	dw	0
.lba0		dd	0
.lba1		dd	0
disk_read_lba:
	pusha
	xor ax, ax
	mov al, dh
	mov [lba.count], ax
	mov [lba.offset], bx
	mov [lba.segment], es
	xor eax, eax
	mov al, cl
	mov [lba.lba0], eax
	xor eax, eax
	mov [lba.lba1], eax
	
	; setup INT 13h call
	mov si, lba
	mov ah, 42h
	int 13h

	popa
	ret

