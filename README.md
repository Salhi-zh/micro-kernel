# micro-kernel

A small **x86 (i686) operating-system kernel written from scratch** in C and assembly — a
Multiboot kernel that boots under QEMU, brings up the CPU, drives real hardware, and even
talks to the network with its own **RTL8139 driver and a tiny TCP/UDP + DNS stack**, all
driven from an interactive serial/VGA shell.

## What it does

- **Boot & CPU setup** — Multiboot header (`boot.asm`), a flat **GDT**, **IDT**, and full
  **ISR/IRQ** handling (`isr.asm`, `irq.asm` + C handlers).
- **Memory** — paging and a simple physical/heap allocator (`memory.c`, `paging.h`).
- **Console I/O** — VGA text output (`vga.c`), PS/2 **keyboard** driver (`keyboard.c`),
  and **16550 serial** for debugging (`serial.c`).
- **PCI** — bus enumeration (`pci.c`) to discover devices.
- **Networking** — a **Realtek RTL8139 NIC driver** (`rtl8139.c`), a network interface
  layer (`netif.c`), an Ethernet/IP stack (`network.c`) and a **DNS resolver** (`dns.c`).
- **Shell** — an interactive command shell (`shell.c`) over serial/VGA.
- **Fault handling** — a panic path with register dumps (`panic.c`).

## Layout

```
boot.asm        Multiboot entry
link.ld         linker script (kernel load layout)
Makefile        build ISO + run under QEMU
include/        public headers (gdt, idt, isr, irq, memory, pci, rtl8139, network, dns, ...)
src/            implementations (*.c) + low-level stubs (*.asm)
install_deps.sh cross-compiler / QEMU setup
setup_net.sh    host-side TAP/bridge networking for QEMU
```

## Build & run

```bash
./install_deps.sh        # i686 cross toolchain (nasm, i686 gcc), grub, qemu
make                     # builds kernel.iso
make run                 # boots it in QEMU (with networking via setup_net)
```

> Requires an `i686-elf`/`i686-linux-gnu` cross-compiler and `nasm`; see `install_deps.sh`.

## Why

Building an OS end-to-end — from the boot handshake to a working NIC driver and DNS lookups —
is the best way I've found to actually understand interrupts, paging, PCI, and how the network
stack sits on top of raw hardware.
