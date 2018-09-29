[bits 64]
VIDEO_MEM equ 0xB8000
; print64 console output
print64:
	mov edi, VIDEO_MEM
	; compose edi	
	xor rax, rax
	mov al, byte [system_info + system_info_struct.cursory]
	imul eax, 160	
	add edi, eax
	xor rax, rax
	mov al, byte [system_info + system_info_struct.cursorx]
	shl eax, 1
	add edi, eax

.go:
	mov al, [esi]
	cmp al, 0
	je .done
	
	; \n
	cmp al, 0xa
	jne .l1
	mov eax, edi
	sub eax, VIDEO_MEM
	mov ecx, 160
	xor edx, edx
	idiv ecx
	inc eax
	cmp eax, 25
	jne .l2
	; scroll up
	mov ecx, 24 * 160 / 8
	push rsi
	push rdi
	push rax
	mov esi, VIDEO_MEM + 160
	mov edi, VIDEO_MEM
	rep movsq 
	mov ecx, 160 / 8
	mov rax, 0x0720072007200720
	rep stosq
	pop rax
	pop rdi
	pop rsi
	dec eax
.l2:	
	imul eax, 160
	add eax, VIDEO_MEM
	mov edi, eax
	jmp .next
.l1:	
	mov [edi], al
	inc edi
	inc edi
	; if edi overflows, scroll up
	cmp edi, VIDEO_MEM + 160 * 25
	jne .next
	sub edi, 160
	; scroll up
	mov ecx, 24 * 160 / 8
	push rsi
	push rdi
	mov esi, VIDEO_MEM + 160
	mov edi, VIDEO_MEM
	rep movsq 
	mov ecx, 160 / 8
	mov rax, 0x0720072007200720
	rep stosq
	pop rdi
	pop rsi	
.next:
	inc esi
	jmp .go
.done:
	mov eax, edi
	sub eax, VIDEO_MEM
	mov ebx, eax
	shr ebx, 1
	mov ecx, 160
	xor edx, edx
	idiv ecx
	mov byte [system_info + system_info_struct.cursory], al
	shr edx, 1
	mov byte [system_info + system_info_struct.cursorx], dl

	; cursor low port to VGA index register
	mov al, 0Fh
	mov dx, 3D4h
	out dx, al
 
	; cursor low position to VGA data register
	mov ax, bx
	mov dx, 3D5h
	out dx, al
 
	; cursor high port to VGA index register
	mov al, 0Eh
	mov dx, 3D4h
	out dx, al
 
	; cursor high position to VGA data register
	mov ax, bx
	shr ax, 8
	mov dx, 3D5h
	out dx, al
	ret

