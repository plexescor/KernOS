global gdtLoad

gdtLoad:
    lgdt [rdi]              ; rdi = &gdtr (first arg, System V ABI)

    ; can't mov into CS directly, do far return trick
    push 0x08               ; kernel code selector
    lea rax, [rel .done]
    push rax
    retfq                   ; pops rip then cs atomically

.done:
    mov ax, 0x10            ; kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret