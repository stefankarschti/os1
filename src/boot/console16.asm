[bits 16]

; Prints out a message using the BIOS.
; es:si    Address of ASCIIZ string to print.
print16:
    push ax
    push si
.l1:
    lodsb
    test al, al
    jz .done                  	
    mov ah, 0x0E	
    int 0x10
    jmp .l1
.done:
    pop si
    pop ax
    ret
    
; Prints out a hex number using the BIOS.
; ax	Number to print.
;print16_whex:
;    pusha
;    mov bx, ax
;    mov si, bx
;    shr si, 12
;    and si, 0fh
;    mov al, [hexdigit + si]
;    mov ah, 0x0E	
;    int 0x10
;    mov si, bx
;    shr si, 8
;    and si, 0fh
;    mov al, [hexdigit + si]
;    mov ah, 0x0E	
;    int 0x10
;    mov si, bx
;    shr si, 4
;    and si, 0fh
;    mov al, [hexdigit + si]
;    mov ah, 0x0E	
;    int 0x10
;    mov si, bx
;    and si, 0fh
;    mov al, [hexdigit + si]
;    mov ah, 0x0E	
;    int 0x10 
;    popa
;    ret

