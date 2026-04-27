; Exception/syscall vector stubs. Each label normalizes the CPU's interrupt
; stack frame by pushing an error-code slot and vector number, then jumps into
; the common trap-entry path in irqhandler.asm.

extern trap_entry_common

%macro EXC_NOERR 2
global %1
%1:
	push qword 0
	push qword %2
	jmp trap_entry_common
%endmacro

%macro EXC_ERR 2
global %1
%1:
	push qword %2
	jmp trap_entry_common
%endmacro

EXC_NOERR int_00h, 0
EXC_NOERR int_01h, 1
EXC_NOERR int_02h, 2
EXC_NOERR int_03h, 3
EXC_NOERR int_04h, 4
EXC_NOERR int_05h, 5
EXC_NOERR int_06h, 6
EXC_NOERR int_07h, 7
EXC_ERR   int_08h, 8
EXC_NOERR int_09h, 9
EXC_ERR   int_0Ah, 10
EXC_ERR   int_0Bh, 11
EXC_ERR   int_0Ch, 12
EXC_ERR   int_0Dh, 13
EXC_ERR   int_0Eh, 14
EXC_NOERR int_10h, 16
EXC_ERR   int_11h, 17
EXC_NOERR int_12h, 18
EXC_NOERR int_13h, 19
EXC_NOERR int_1Dh, 29
EXC_NOERR int_1Eh, 30
