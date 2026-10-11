
#!/bin/bash

# This script installs the necessary build and runtime dependencies 
# for the micro-kernel project on Debian/Ubuntu-based systems.

echo "Updating package lists..."
sudo apt update

echo "Installing build tools, compilers, and assemblers..."
# nasm: Assembler
# gcc-i686-linux-gnu: Cross-compiler for 32-bit x86
# make: Build automation
# libc6-dev-i386: Headers for 32-bit development
sudo apt install -y make nasm gcc-i686-linux-gnu libc6-dev-i386

echo "Installing ISO creation utilities..."
# grub-pc-bin: GRUB bootloader files
# xorriso: Required by grub-mkrescue to create the ISO image
# mtools: Required for FAT filesystem manipulation by GRUB
sudo apt install -y grub-pc-bin xorriso mtools

echo "Installing emulation and networking tools..."
# qemu-system-x86: The emulator
# iproute2: For 'ip' commands used in setup_net.sh
# iptables: For NAT/Masquerade support
sudo apt install -y qemu-system-x86 iproute2 iptables

echo "Checking for libgcc for the cross-compiler..."
sudo apt install -y libgcc-s1-i386-cross || echo "Note: libgcc-s1-i386-cross might have a different name on your version, checking alternatives..."

echo "------------------------------------------------"
echo "Installation complete!"
echo "You can now build and run the kernel using:"
echo "  make run"
echo "------------------------------------------------"
