
# Makefile – builds a bootable ISO with automatic network setup

CC = i686-linux-gnu-gcc
AS = nasm
LD = i686-linux-gnu-ld

CFLAGS = -std=c99 -ffreestanding -O2 -Wall -Wextra -Werror -Iinclude -nostdlib -fno-builtin -fno-stack-protector -m32 -mgeneral-regs-only -fno-pie
ASFLAGS = -f elf32
LDFLAGS = -m32 -T link.ld -ffreestanding -nostdlib -lgcc -no-pie -Wl,--no-gc-sections -Wl,--build-id=none
OBJDIR = obj
SRCDIR = src
INCDIR = include

# C source files
C_SOURCES = $(wildcard $(SRCDIR)/*.c)
C_OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(C_SOURCES))

# Assembly source files (explicit list)
ASM_SOURCES = boot.asm $(SRCDIR)/isr.asm $(SRCDIR)/irq.asm
# Generate distinct object names for assembly stubs
ASM_OBJECTS = $(OBJDIR)/boot.o $(OBJDIR)/isr_asm.o $(OBJDIR)/irq_asm.o

# All objects
OBJS = $(C_OBJECTS) $(ASM_OBJECTS)

# QEMU Settings
QEMU = qemu-system-i386
ISO = kernel.iso
QEMU_FLAGS = -cdrom $(ISO) \
             -serial stdio \
             -netdev tap,id=net0,ifname=tap0,script=no,downscript=no \
             -device rtl8139,netdev=net0,mac=52:54:00:12:34:56

.PHONY: all clean run setup_net

all: $(ISO)

# Compile C files
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble boot.asm
$(OBJDIR)/boot.o: boot.asm
	@mkdir -p $(OBJDIR)
	$(AS) $(ASFLAGS) $< -o $@

# Assemble isr.asm -> isr_asm.o
$(OBJDIR)/isr_asm.o: $(SRCDIR)/isr.asm
	@mkdir -p $(OBJDIR)
	$(AS) $(ASFLAGS) $< -o $@

# Assemble irq.asm -> irq_asm.o
$(OBJDIR)/irq_asm.o: $(SRCDIR)/irq.asm
	@mkdir -p $(OBJDIR)
	$(AS) $(ASFLAGS) $< -o $@

# Link kernel
$(OBJDIR)/kernel.bin: $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

# Build ISO
$(ISO): $(OBJDIR)/kernel.bin
	mkdir -p isodir/boot/grub
	cp $< isodir/boot/kernel.bin
	echo 'menuentry "MyKernel" { multiboot /boot/kernel.bin }' > isodir/boot/grub/grub.cfg
	grub-mkrescue -o $@ isodir
	rm -rf isodir

clean:
	rm -rf $(OBJDIR) $(ISO) isodir

setup_net:
	@echo "Checking network configuration..."
	@chmod +x setup_net.sh
	@./setup_net.sh

run: $(ISO) setup_net
	$(QEMU) $(QEMU_FLAGS)
