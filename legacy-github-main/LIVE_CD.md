# VECTORD OS - Live CD / Installation Guide

## What Is This?

VECTORD OS is a standalone operating system kernel - no Linux, no Unix, no BSD.
It boots directly from ISO and provides a Full HD graphical desktop.

## Quick Start (Live Test)

### Boot from ISO in QEMU
```bash
# Full graphical desktop (recommended)
qemu-system-i386 -cdrom vectord-os.iso -boot d -m 256M -vga std -display sdl

# Serial mode (headless)
qemu-system-i386 -cdrom vectord-os.iso -boot d -m 256M -nographic -serial mon:stdio

# With network
qemu-system-i386 -cdrom vectord-os.iso -boot d -m 256M -vga std -display sdl \
    -net nic,model=e1000 -net user
```

### Boot from Real Hardware
1. Burn `vectord-os.iso` to USB or CD
2. Set BIOS to boot from USB/CD
3. GRUB menu appears:
   - **VECTORD OS v2.0.1 - Live Desktop** (default, graphical)
   - **VECTORD OS v2.0.1 - Serial Mode** (for debugging)
4. Desktop launches automatically

## Live Desktop Features

### Desktop Environment
- 1920x1080 Full HD framebuffer
- Glassmorphism window decorations
- Gradient wallpaper with glow effects
- Taskbar with Start button, window tabs, live clock
- Mouse cursor with save/restore
- Draggable, focusable windows

### Windows
| Window | Description |
|--------|-------------|
| Terminal | Green-on-black terminal, type to interact |
| System | CPU, RAM, framebuffer, uptime display |
| About | VECTORD OS info panel |

### Controls
| Input | Action |
|-------|--------|
| Mouse | Move cursor, click windows, drag title bars |
| Keyboard | Type in focused terminal window |
| ESC | Exit GUI back to serial shell |
| Click X | Close window |

## Shell Commands (ESC from GUI)

```
help        - List all commands
gui         - Relaunch GUI desktop
desktop-hd  - ASCII art desktop demo (serial)
desktop-hypr- Hyprland compositor demo
install     - Install to hard disk
netstat     - Network statistics
ai stats    - Vector AI status
mem         - Memory statistics
ps          - Process list
vbetest     - VGA test pattern
```

## Disk Installation

```bash
# Create a disk image
qemu-img create -f raw vectord-disk.img 512M

# Boot with disk attached
qemu-system-i386 -cdrom vectord-os.iso -boot d -hda vectord-disk.img \
    -m 256M -vga std -display sdl

# In the OS, run:
install
```

## What's Included

- **Kernel**: 241KB standalone, 31 C modules + 1 ASM
- **Graphics**: 1920x1080 framebuffer, window manager, mouse, keyboard
- **Networking**: Full TCP/IP stack (ARP, IPv4, ICMP, UDP, TCP, DHCP, DNS)
- **Filesystems**: VECTORD FS, ext2 (read), VFS layer
- **AI**: Akasha vector AI with heap-backed paged concept storage
- **Math**: SSE2-accelerated vector operations
- **Drivers**: PCI, ATA/IDE, AHCI, RTL8139, E1000, PS/2, RTC, VGA, VBE

## System Requirements

- CPU: x86/i386 with SSE2
- RAM: 256MB minimum
- Graphics: VESA VBE compatible (QEMU std-vga)
- Storage: Optional (for installation)

## Architecture

- **No POSIX**: This is NOT Unix. No syscalls, no fork, no pipes.
- **Monolithic kernel**: Everything runs in ring 0.
- **Multiboot compliant**: Boots via GRUB.
- **Real mode init**: GDT, IDT, paging, SSE enabled at boot.
- **Heap-based**: kmalloc/kfree for dynamic memory.

## File Structure

```
vectord-os.iso          # Bootable ISO image
build/vectord-kernel.elf # Kernel ELF (for QEMU -kernel boot)
boot/grub.cfg           # GRUB configuration
kernel/                 # Source code
include/vecord.h        # Master header
docs/                   # Documentation
```

## Building from Source

```bash
make clean && make       # Build kernel
make iso                 # Create bootable ISO
```

Requires: gcc (32-bit), nasm, ld, grub-mkrescue, xorriso

