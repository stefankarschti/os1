; void disk_load_sectors(char* addr, int disk, int num);
;   bx = [addr]
;   dh = [num:disk]
;   cl = [base sector]
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

