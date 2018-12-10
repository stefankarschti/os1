extern irq_handler
extern saveregs
extern restoreregs

global irq0
global irq1
global irq2
global irq3
global irq4
global irq5
global irq6
global irq7
global irq8
global irq9
global irq10
global irq11
global irq12
global irq13
global irq14
global irq15

irq0:
cli
call saveregs
push qword 0
jmp irq_common
irq1:
cli
call saveregs
push qword 1
jmp irq_common
irq2:
cli
call saveregs
push qword 2
jmp irq_common
irq3:
cli
call saveregs
push qword 3
jmp irq_common
irq4:
cli
call saveregs
push qword 4
jmp irq_common
irq5:
cli
call saveregs
push qword 5
jmp irq_common
irq6:
cli
call saveregs
push qword 6
jmp irq_common
irq7:
cli
call saveregs
push qword 7
jmp irq_common
irq8:
cli
call saveregs
push qword 8
jmp irq_common
irq9:
cli
call saveregs
push qword 9
jmp irq_common
irq10:
cli
call saveregs
push qword 10
jmp irq_common
irq11:
cli
call saveregs
push qword 11
jmp irq_common
irq12:
cli
call saveregs
push qword 12
jmp irq_common
irq13:
cli
call saveregs
push qword 13
jmp irq_common
irq14:
cli
call saveregs
push qword 14
jmp irq_common
irq15:
cli
call saveregs
push qword 15
jmp irq_common

irq_common:
	mov rdi, [rsp]
	call irq_handler
	pop rdi
	mov al, 0x20
	cmp rdi, 8
	jl .l1
	out 0xA0, al
.l1
	out 0x20, al
	call restoreregs
	iretq
