; Load sectors LBA mode
; ES:DI = destination memory address
; EBX:EAX = start LBA address
; DL = drive
; CX = count sectors
;
lba_packet	equ BOOT_DISK_PACKET_ADDRESS
lba_size	equ lba_packet + 0
lba_reserved	equ lba_packet + 1
lba_count	equ lba_packet + 2
lba_offset	equ lba_packet + 4
lba_segment	equ lba_packet + 6
lba_lba0	equ lba_packet + 8
lba_lba1	equ lba_packet + 12

disk_read_lba:
	; Keep the BIOS DAP in dedicated scratch RAM so firmware reads cannot
	; overwrite executable bytes in the 512-byte boot sector itself.
	mov byte [lba_size], 16
	mov byte [lba_reserved], 0
	mov [lba_count], cx
	mov [lba_offset], di
	mov [lba_segment], es
	mov [lba_lba0], eax
	mov [lba_lba1], ebx
	
	; setup INT 13h call
	mov si, lba_packet
	mov ah, 42h
	; dl = drive number
	int 13h
	ret
