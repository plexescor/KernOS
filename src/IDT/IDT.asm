global idtLoad
global isrDivideByZero
global isrDebug
global isrNMI
global isrBreakpoint
global isrOverflow
global isrBoundRange
global isrInvalidOpcode
global isrDeviceNotAvailable
global isrDoubleFault
global isrGPF
global isrPageFault
global isrTimer
global isrKeyboard

extern isrHandler

idtLoad:
    lidt [rdi]
    ret

%macro ISR_NOERR 1
isr%1:
    push 0
    push %1
    jmp isrCommon
%endmacro

%macro ISR_ERR 1
isr%1:
    push %1
    jmp isrCommon
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 32
ISR_NOERR 33

isrCommon:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, [rsp + 15*8]
    mov rsi, [rsp + 16*8]
    call isrHandler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16
    iretq

; named aliases matching the extern "C" declarations
isrDivideByZero  equ isr0
isrDebug         equ isr1
isrNMI           equ isr2
isrBreakpoint    equ isr3
isrOverflow      equ isr4
isrBoundRange    equ isr5
isrInvalidOpcode equ isr6
isrDeviceNotAvailable equ isr7
isrDoubleFault   equ isr8
isrGPF           equ isr13
isrPageFault     equ isr14
isrTimer         equ isr32
isrKeyboard      equ isr33