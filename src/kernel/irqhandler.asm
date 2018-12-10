extern irq0_handler
extern irq1_handler
extern irq2_handler
extern irq3_handler
extern irq4_handler
extern irq5_handler
extern irq6_handler
extern irq7_handler
extern irq8_handler
extern irq9_handler
extern irq10_handler
extern irq11_handler
extern irq12_handler
extern irq13_handler
extern irq14_handler
extern irq15_handler

extern saveregs
extern restoreregs

global irq0
irq0:
	cli
	call saveregs
	call irq0_handler
	call restoreregs
	iretq
 
global irq1
irq1:
	cli
	call saveregs
	call irq1_handler
	call restoreregs
	push rax
	mov al, 0x20
	out 0x20, al
	pop rax
	iretq

global irq2
irq2:
	cli
	call saveregs
	call irq2_handler
	call restoreregs
	iretq
 
global irq3
irq3:
	cli
	call saveregs
	call irq3_handler
	call restoreregs
	iretq
 
global irq4
irq4:
	cli
	call saveregs
	call irq4_handler
	call restoreregs
	iretq
 
global irq5
irq5:
	cli
	call saveregs
	call irq5_handler
	call restoreregs
	iretq
 
global irq6
irq6:
	cli
	call saveregs
	call irq6_handler
	call restoreregs
	iretq
 
global irq7
irq7:
	cli
	call saveregs
	call irq7_handler
	call restoreregs
	iretq
 
global irq8
irq8:
	cli
	call saveregs
	call irq8_handler
	call restoreregs
	iretq
 
global irq9
irq9:
	cli
	call saveregs
	call irq9_handler
	call restoreregs
	iretq
 
global irq10
irq10:
	cli
	call saveregs
	call irq10_handler
	call restoreregs
	iretq
 
global irq11
irq11:
	cli
	call saveregs
	call irq11_handler
	call restoreregs
	iretq
 
global irq12
irq12:
	cli
	call saveregs
	call irq12_handler
	call restoreregs
	iretq
 
global irq13
irq13:
	cli
	call saveregs
	call irq13_handler
	call restoreregs
	iretq
 
global irq14
irq14:
	cli
	call saveregs
	call irq14_handler
	call restoreregs
	iretq
 
global irq15
irq15:
	cli
	call saveregs
	call irq15_handler
	call restoreregs
	iretq

