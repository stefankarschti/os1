global startMultiTask
global task_switch_irq
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

panic:
    mov rsi, 0xB8000
    add ax, 0xF00
    mov word [rsi], ax
    hlt
    jmp panic

; set active task to RDI
; switch to this task
; start timer
startMultiTask:
    cli
    mov r15, rdi
    mov rsp, qword [rdi + 4 * 8]

	; start irq0 timer: 1193182 ticks/s divided by ax
    mov al, 0x34
    out 0x43, al
    mov ax, 1193    ; 1193182 ticks/s divided by ax
    out 0x40, al    ; low
    rol ax, 8
    out 0x40, al    ; high
    rol ax, 8

    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rsi
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    iretq

counter dw 0 ; counter (second level divider) for timer interrupts
task_switch_irq:
    cli
    push rax
    mov al, 0x20
    out 0x20, al
    inc word [counter]
    mov ax, word [counter]
    cmp ax, 1
    je .l2
    pop rax
    iretq

.l2: ; every second
    xor ax, ax
    mov word [counter], ax
    pop rax

    push rdi
    mov rdi, r15
    cmp r15, qword [rdi + 5 * 8]
    je .do
    pop rdi
    mov ax, 'E'
    call panic ; error stop - corrupted r15

.do:
    pop rdi

    ; save registers
    push rax
    push rbx
    push rcx
    push rdx
    push rdi
    push rsi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14

; task switch
    ; save current task stack
    mov rdi, r15
    mov qword [rdi + 4 * 8], rsp
    ; get next task
    mov r15, qword [rdi]
    mov rdi, r15
    ; switch stack
    mov rsp, qword [rdi + 4 * 8]

    ; restore registers
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rsi
    pop rdi
    pop rdx
    pop rcx
    pop rbx
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

