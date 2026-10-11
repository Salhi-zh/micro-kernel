; isr.asm – Interrupt Service Routine stubs

; This macro creates a stub for an ISR that pushes a dummy error code (if needed)
; and then calls the common ISR handler.

%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    cli
    push 0          ; push dummy error code
    push %1         ; push interrupt number
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    cli
    push %1         ; push interrupt number (error code already on stack)
    jmp isr_common_stub
%endmacro

; Exceptions with error codes: 8, 10, 11, 12, 13, 14, 17, 21, 29, 30
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_ERRCODE   21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_ERRCODE   29
ISR_ERRCODE   30
ISR_NOERRCODE 31

; Common ISR stub: saves CPU state, calls C handler, restores state, returns
extern isr_handler
isr_common_stub:
    pusha                   ; push edi, esi, ebp, esp, ebx, edx, ecx, eax

    mov ax, ds              ; lower 16 bits of ds
    push eax                ; save ds

    mov ax, 0x10            ; load kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp                ; push pointer to registers_t (stack frame)
    call isr_handler
    add esp, 4              ; pop the argument

    pop eax                 ; restore ds
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa                    ; restore general purpose registers
    add esp, 8              ; clean up error code and interrupt number
    sti
    iret
