section .text
global _start
extern kernelMain

_start:
    mov rsp, stackTop
    and rsp, -16

    sub rsp, 8
    call kernelMain
.hang:
    cli
    hlt
    jmp .hang

section .bss
align 16
stackBottom:
    resb 16384
align 16
stackTop: