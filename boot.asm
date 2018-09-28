[bits 16]
[org 0x7c00]
kernel_main16		equ 0x1000           ; Location for kernel in memory
_start:
	jmp 0 : _main
kernel_num_sectors	db 4
_main:
	mov ax, cs
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov fs, ax
	mov gs, ax
	mov [boot_device], dl			; save boot drive
	mov bp, _main				; set up basic stack
	mov sp, bp

	; go unreal
	; ... to load (full) kernel (high)
	mov bx, kernel_main16	                ; ES:BX = Address to load kernel into
	mov dh, [kernel_num_sectors]            ; DH    = Number of sectors to load
	mov dl, [boot_device]                   ; DL    = Drive number to load from
	call disk_load_sectors                  ; Call disk load function
	
	jmp kernel_main16

; void disk_load_sectors(char* addr, int disk, int num);
;   bx = [addr]
;   dh = [num:disk]
; Load dh sectors from disk dl into address es:bx
disk_load_sectors:
	pusha                               ; Preserve register values in stack

	push dx                             ; Store [DH:DL], DH = number of sectors to load 

	mov ah, 0x02                        ; BIOS sector load
	mov al, DH                          ; AL = DH = number of sectors to load
	mov ch, 0x00                        ; CH = Cylinder = 0
	mov dh, 0x00                        ; DH = Head = 0
	mov cl, 0x02                        ; CL = Base Sector = 2

	int 0x13                            ; BIOS interrupt

	jc disk_load_sectors_fail           ; Jump if error (carry flag)

	pop dx                              ; Restore [DH:DL], DH = number of sectors to load
	cmp dh, al                          ; Compare number of actual loaded sectors
	jne disk_load_sectors_fail          ; Jump if error (incorrect number of sectors)

	popa                                ; Restore register values
	ret                                 ; Return function call

	; Report disk_load_sectors failure
disk_load_sectors_fail:
	mov bx, disk_error_msg          ; BX = pointer to disk fail string
	call msg_print_str 		; Print string
	jmp $                           ; Hang
        
; void msg_print_str(char *str);
;   bx = [str]
; Print string in real mode
msg_print_str:
	pusha                               ; Preserve registers on stack

	mov ah, 0x0E                        ; BIOS TTY
	mov si, bx                          ; Move str to [SI]

	; Loop through string printing characters
msg_print_str_loop_char:
	lodsb                           ; Load new byte from SI into AX
	cmp al, 0x00                    ; Check null-termination character
	je msg_print_str_end            ; Terminate loop

	int 0x10                        ; Otherwise call BIOS interrupt
	jmp msg_print_str_loop_char     ; Print another character

	; End function
msg_print_str_end:
	popa                            ; Restore registers from stack
	ret                             ; Return from function

; Data
disk_error_msg   	db "[boot] disk error", 13, 10, 0
boot_device 		db 0x00                     ; Get byte to store boot drive number

; Tail
times 510-($-$$) db 0x00                ; Fill bootloader to 512-bytes
dw 0xAA55                               ; Magic word signature


