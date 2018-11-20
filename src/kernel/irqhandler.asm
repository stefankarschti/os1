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
	call saveregs
	call irq0_handler
	call restoreregs
	iretq
 
global irq1
irq1:
	call saveregs
	call irq1_handler
	call restoreregs
	iretq

global irq2
irq2:
	call saveregs
	call irq2_handler
	call restoreregs
	iretq
 
global irq3
irq3:
  call irq3_handler
  iretq
 
global irq4
irq4:
  call irq4_handler
  iretq
 
global irq5
irq5:
  call irq5_handler
  iretq
 
global irq6
irq6:
  call irq6_handler
  iretq
 
global irq7
irq7:
  call irq7_handler
  iretq
 
global irq8
irq8:
  call irq8_handler
  iretq
 
global irq9
irq9:
  call irq9_handler
  iretq
 
global irq10
irq10:
  call irq10_handler
  iretq
 
global irq11
irq11:
  call irq11_handler
  iretq
 
global irq12
irq12:
  call irq12_handler
  iretq
 
global irq13
irq13:
  call irq13_handler
  iretq
 
global irq14
irq14:
  call irq14_handler
  iretq
 
global irq15
irq15:
  call irq15_handler
  iretq

