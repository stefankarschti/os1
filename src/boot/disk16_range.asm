; Load a contiguous LBA range with BIOS EDD packet reads.
; EDI = flat destination memory address (must stay below 1 MiB)
; EBX:EAX = start LBA address
; DL = drive
; CX = count sectors
;
; The M2 BIOS path still uses fixed low-memory buffers because INT 13h packet
; reads describe their destination as a real-mode segment:offset pair.
load_range_packet		equ LOADER_DISK_PACKET_ADDRESS
load_range_packet_size	equ load_range_packet + 0
load_range_packet_res	equ load_range_packet + 1
load_range_packet_count	equ load_range_packet + 2
load_range_packet_off	equ load_range_packet + 4
load_range_packet_seg	equ load_range_packet + 6
load_range_packet_lba0	equ load_range_packet + 8
load_range_packet_lba1	equ load_range_packet + 12

load_range_state		equ LOADER_DISK_RANGE_STATE_ADDRESS
load_range_lba0			equ load_range_state + 0
load_range_lba1			equ load_range_state + 4
load_range_dest			equ load_range_state + 8
load_range_count		equ load_range_state + 12
load_range_drive		equ load_range_state + 14

disk_read_lba_range:
	mov [load_range_lba0], eax
	mov [load_range_lba1], ebx
	mov [load_range_dest], edi
	mov [load_range_count], cx
	mov [load_range_drive], dl

.next:
	mov cx, [load_range_count]
	or cx, cx
	jz .done

	mov ax, cx
	cmp ax, 127
	jbe .chunk_count_ready
	mov ax, 127
.chunk_count_ready:
	mov byte [load_range_packet_size], 16
	mov byte [load_range_packet_res], 0
	mov [load_range_packet_count], ax

	mov edi, [load_range_dest]
	mov bx, di
	and bx, 0x000F
	shr edi, 4
	mov [load_range_packet_off], bx
	mov [load_range_packet_seg], di
	mov eax, [load_range_lba0]
	mov ebx, [load_range_lba1]
	mov [load_range_packet_lba0], eax
	mov [load_range_packet_lba1], ebx

	mov si, load_range_packet
	mov ah, 0x42
	mov dl, [load_range_drive]
	int 0x13
	jc .error

	xor eax, eax
	mov ax, [load_range_packet_count]
	mov ebx, eax
	shl eax, 9
	add [load_range_dest], eax
	add [load_range_lba0], ebx
	adc dword [load_range_lba1], 0
	sub [load_range_count], bx
	jmp .next

.done:
	clc
	ret

.error:
	stc
	ret
