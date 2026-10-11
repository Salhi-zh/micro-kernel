; boot.asm – Multiboot header and entry point

section .multiboot
align 4
    dd 0x1BADB002          ; magic
    dd 0x03                ; flags (page align, memory info)
    dd -(0x1BADB002 + 0x03); checksum

section .bootstrap_stack
align 16
stack_bottom:
    times 16384 db 0       ; 16 KB stack
stack_top:

section .text
global _start
_start:
    mov esp, stack_top     ; set stack pointer

    ; push Multiboot info (GRUB passes pointer in ebx)
    push ebx
    push eax

    extern kernel_main
    call kernel_main       ; never returns

    cli
    hlt
