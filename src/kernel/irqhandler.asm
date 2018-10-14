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
global int_80h

global load_idt
 
global irq0_handler
global irq1_handler
global irq2_handler
global irq3_handler
global irq4_handler
global irq5_handler
global irq6_handler
global irq7_handler
global irq8_handler
global irq9_handler
global irq10_handler
global irq11_handler
global irq12_handler
global irq13_handler
global irq14_handler
global irq15_handler
global int_handler

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
extern int_handler

int_80h:
	push rax
	push rcx
	push rdx
	push r8
	push r9
	push r10
	push r11
	mov rdi, 80h
	call int_handler
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdx
	pop rcx
	pop rax
	iretq

irq0:
  push rax
  push rcx
  push rdx
  push r8
  push r9
  push r10
  push r11
  call irq0_handler
  pop r11
  pop r10
  pop r9
  pop r8
  pop rdx
  pop rcx
  pop rax
  iretq
 
irq1:
  push rax
  push rcx
  push rdx
  push r8
  push r9
  push r10
  push r11
  call irq1_handler
  pop r11
  pop r10
  pop r9
  pop r8
  pop rdx
  pop rcx
  pop rax
  iretq
 
irq2:
  push rax
  push rcx
  push rdx
  push r8
  push r9
  push r10
  push r11
  call irq2_handler
  pop r11
  pop r10
  pop r9
  pop r8
  pop rdx
  pop rcx
  pop rax
  iretq
 
irq3:
  push rax
  push rcx
  push rdx
  push r8
  push r9
  push r10
  push r11
  call irq3_handler
  pop r11
  pop r10
  pop r9
  pop r8
  pop rdx
  pop rcx
  pop rax
  iretq
 
irq4:
  push rax
  push rcx
  push rdx
  push r8
  push r9
  push r10
  push r11
  call irq4_handler
  pop r11
  pop r10
  pop r9
  pop r8
  pop rdx
  pop rcx
  pop rax
  iretq
 
irq5:
  push rax
  push rcx
  push rdx
  push r8
  push r9
  push r10
  push r11
  call irq5_handler
  pop r11
  pop r10
  pop r9
  pop r8
  pop rdx
  pop rcx
  pop rax
  iretq
 
irq6:
  push rax
  push rcx
  push rdx
  push r8
  push r9
  push r10
  push r11
  call irq6_handler
  pop r11
  pop r10
  pop r9
  pop r8
  pop rdx
  pop rcx
  pop rax
  iretq
 
irq7:
  push rax
  push rcx
  push rdx
  push r8
  push r9
  push r10
  push r11
  call irq7_handler
  pop r11
  pop r10
  pop r9
  pop r8
  pop rdx
  pop rcx
  pop rax
  iretq
 
irq8:
  push rax
  push rcx
  push rdx
  push r8
  push r9
  push r10
  push r11
  call irq8_handler
  pop r11
  pop r10
  pop r9
  pop r8
  pop rdx
  pop rcx
  pop rax
  iretq
 
irq9:
  push rax
  push rcx
  push rdx
  push r8
  push r9
  push r10
  push r11
  call irq9_handler
  pop r11
  pop r10
  pop r9
  pop r8
  pop rdx
  pop rcx
  pop rax
  iretq
 
irq10:
  push rax
  push rcx
  push rdx
  push r8
  push r9
  push r10
  push r11
  call irq10_handler
  pop r11
  pop r10
  pop r9
  pop r8
  pop rdx
  pop rcx
  pop rax
  iretq
 
irq11:
  push rax
  push rcx
  push rdx
  push r8
  push r9
  push r10
  push r11
  call irq11_handler
  pop r11
  pop r10
  pop r9
  pop r8
  pop rdx
  pop rcx
  pop rax
  iretq
 
irq12:
  push rax
  push rcx
  push rdx
  push r8
  push r9
  push r10
  push r11
  call irq12_handler
  pop r11
  pop r10
  pop r9
  pop r8
  pop rdx
  pop rcx
  pop rax
  iretq
 
irq13:
  push rax
  push rcx
  push rdx
  push r8
  push r9
  push r10
  push r11
  call irq13_handler
  pop r11
  pop r10
  pop r9
  pop r8
  pop rdx
  pop rcx
  pop rax
  iretq
 
irq14:
  push rax
  push rcx
  push rdx
  push r8
  push r9
  push r10
  push r11
  call irq14_handler
  pop r11
  pop r10
  pop r9
  pop r8
  pop rdx
  pop rcx
  pop rax
  iretq
 
irq15:
  push rax
  push rcx
  push rdx
  push r8
  push r9
  push r10
  push r11
  call irq15_handler
  pop r11
  pop r10
  pop r9
  pop r8
  pop rdx
  pop rcx
  pop rax
  iretq
 

 
load_idt:
	lea rax, [rsp + 8]
	lidt [rax]
	sti
	;int 0x80 ; test it
	ret

