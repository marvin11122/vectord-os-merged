# VECTORD OS - Standalone Operating System
# Supports both 32-bit (i386) and 64-bit (x86-64) builds

CC = gcc
AS = nasm
LD = ld

# Default to 32-bit
ARCH ?= i386

# 32-bit flags
CFLAGS_32 = -m32 -msse2 -ffreestanding -fno-stack-protector -fno-builtin \
            -nostdlib -nostdinc -fno-exceptions -fno-unwind-tables \
            -fno-pie -fno-pic -Wall -Wextra -O2 -Iinclude
CFLAGS_GUI = $(CFLAGS_32) -DVECTORD_DEFAULT_GUI=1

ASFLAGS_32 = -f elf32
LDFLAGS_32 = -m elf_i386 -T kernel/linker.ld -nostdlib

# 64-bit flags
CFLAGS_64 = -m64 -msse2 -ffreestanding -fno-stack-protector -fno-builtin \
            -nostdlib -nostdinc -fno-exceptions -fno-unwind-tables \
            -fno-pie -fno-pic -Wall -Wextra -O2 -Iinclude \
            -mcmodel=kernel -mno-red-zone

# 64-bit boot stage (32-bit code with Multiboot2, compiled as elf64)
ASFLAGS_64_BOOT = -f elf64
# 64-bit trampoline (pure 64-bit code)
ASFLAGS_64_TRAMP = -f elf64
LDFLAGS_64 = -m elf_x86_64 -T kernel/linker64.ld -nostdlib

# Select flags based on ARCH
ifeq ($(ARCH),x86_64)
    CFLAGS = $(CFLAGS_64)
    ASFLAGS = $(ASFLAGS_64_BOOT)
    LDFLAGS = $(LDFLAGS_64)
    KERNEL_NAME = vectord-kernel64.elf
    QEMU = qemu-system-x86_64
else
    CFLAGS = $(CFLAGS_32)
    ASFLAGS = $(ASFLAGS_32)
    LDFLAGS = $(LDFLAGS_32)
    KERNEL_NAME = vectord-kernel.elf
    QEMU = qemu-system-i386
endif

BUILD = build

# Kernel source files (32-bit)
KERNEL_C = kernel/kernel.c kernel/pci.c kernel/ata.c kernel/ahci.c \
           kernel/usb.c kernel/vbe.c kernel/hardware.c kernel/neural_hw.c \
           kernel/rtl8139.c kernel/e1000.c kernel/mouse.c kernel/rtc.c kernel/rng.c \
           kernel/network_full.c kernel/vector_ai.c kernel/vec_sse.c kernel/graphics.c kernel/vbe_fhd.c kernel/hyprland_compositor.c kernel/desktop_hd.c \
           kernel/init.c kernel/apps.c kernel/desktop.c kernel/elf.c \
           kernel/vfs.c kernel/ext2.c kernel/vectord_fs.c kernel/vga_gfx.c kernel/vga_screenshot.c kernel/compat.c \
           kernel/webserver.c kernel/vga_text.c

# 64-bit specific source (minimal - only kernel64_main.c)
KERNEL_C_64 = kernel/kernel64_main.c

# Object files (32-bit)
KERNEL_O = $(patsubst kernel/%.c,$(BUILD)/%.o,$(KERNEL_C))
KERNEL_O_NO_KERNEL = $(filter-out $(BUILD)/kernel.o,$(KERNEL_O))

# Object files (64-bit)
KERNEL_O_64 = $(patsubst kernel/%.c,$(BUILD)/64/%.o,$(KERNEL_C_64))

.PHONY: all all32 all64 gui iso-gui iso iso64 run run-gui run-iso run64 run-iso64 clean

# Select the architecture-specific default target
ifeq ($(ARCH),x86_64)
all: all64
else
all: all32
endif

# 32-bit builds
all32: $(BUILD)/vectord-kernel.elf

gui: ARCH=i386
gui: $(BUILD)/vectord-kernel-gui.elf

$(BUILD)/gui/kernel.o: kernel/kernel.c include/vecord.h | $(BUILD)
	@mkdir -p $(BUILD)/gui
	$(CC) $(CFLAGS_GUI) -c $< -o $@

$(BUILD)/vectord-kernel-gui.elf: $(BUILD)/boot.o $(BUILD)/gui/kernel.o $(KERNEL_O_NO_KERNEL) kernel/linker.ld
	$(LD) $(LDFLAGS_32) $(BUILD)/boot.o $(BUILD)/gui/kernel.o $(KERNEL_O_NO_KERNEL) -o $@
	@echo "✓ 32-bit GUI Kernel built: $@"

$(BUILD)/boot.o: kernel/boot.asm | $(BUILD)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/%.o: kernel/%.c include/vecord.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/vectord-kernel.elf: $(BUILD)/boot.o $(KERNEL_O) kernel/linker.ld
	$(LD) $(LDFLAGS) $(BUILD)/boot.o $(KERNEL_O) -o $@
	@echo "✓ 32-bit Kernel built: $@ ($(words $(KERNEL_O)) modules)"

# 64-bit builds - single ELF with mixed 32/64-bit code
all64: $(BUILD)/vectord-kernel64.elf

# Boot64: 32-bit code that switches to 64-bit (compiled as elf64)
$(BUILD)/64/boot64.o: kernel/boot64.asm | $(BUILD)
	@mkdir -p $(BUILD)/64
	$(AS) $(ASFLAGS_64_BOOT) $< -o $@

# 64-bit kernel C code
$(BUILD)/64/%.o: kernel/%.c include/vecord.h | $(BUILD)
	@mkdir -p $(BUILD)/64
	$(CC) $(CFLAGS_64) -c $< -o $@

# Link boot64.o + kernel64_main.o as single 64-bit ELF
$(BUILD)/vectord-kernel64.elf: $(BUILD)/64/boot64.o $(KERNEL_O_64) kernel/linker64.ld
	$(LD) $(LDFLAGS_64) $(BUILD)/64/boot64.o $(KERNEL_O_64) -o $@
	@echo "✓ 64-bit Kernel built: $@"

# ISO builds
iso: $(BUILD)/vectord-kernel.elf
	@mkdir -p $(BUILD)/iso/boot/grub
	cp $(BUILD)/vectord-kernel.elf $(BUILD)/iso/boot/vectord-kernel
	cp boot/grub.cfg $(BUILD)/iso/boot/grub/
	grub-mkrescue -o vectord-os.iso $(BUILD)/iso 2>&1 | grep -v "^xorriso" | tail -2
	@echo "✓ ISO: vectord-os.iso ($(shell du -h vectord-os.iso | cut -f1))"

iso-gui: ARCH=i386
iso-gui: gui
	@mkdir -p $(BUILD)/iso-gui/boot/grub
	cp $(BUILD)/vectord-kernel-gui.elf $(BUILD)/iso-gui/boot/vectord-kernel
	cp boot/grub-gui.cfg $(BUILD)/iso-gui/boot/grub/grub.cfg
	grub-mkrescue -o vectord-os-gui.iso $(BUILD)/iso-gui 2>&1 | grep -v "^xorriso" | tail -2
	@echo "✓ GUI ISO: vectord-os-gui.iso ($(shell du -h vectord-os-gui.iso | cut -f1))"

iso64: $(BUILD)/vectord-kernel64.elf
	@mkdir -p $(BUILD)/iso64/boot/grub
	cp $(BUILD)/vectord-kernel64.elf $(BUILD)/iso64/boot/vectord-kernel
	cp boot/grub64.cfg $(BUILD)/iso64/boot/grub/grub.cfg
	grub-mkrescue -o vectord-os64.iso $(BUILD)/iso64 2>&1 | grep -v "^xorriso" | tail -2
	@echo "✓ ISO: vectord-os64.iso ($(shell du -h vectord-os64.iso | cut -f1))"

# Run targets
run: $(BUILD)/vectord-kernel.elf
	qemu-system-i386 -kernel $(BUILD)/vectord-kernel.elf -serial stdio -m 64M -display none

run-gui: iso-gui
	qemu-system-i386 -cdrom vectord-os-gui.iso -boot d -m 256M -nographic -serial mon:stdio

run64: iso64
	qemu-system-x86_64 -cdrom vectord-os64.iso -boot d -m 256M -nographic -serial mon:stdio

run-iso: iso
	qemu-system-i386 -cdrom vectord-os.iso -boot d -m 64M -nographic -serial mon:stdio

run-iso64: iso64
	qemu-system-x86_64 -cdrom vectord-os64.iso -boot d -m 64M -nographic -serial mon:stdio

# Convenience targets
build32:
	$(MAKE) all32

build64:
	$(MAKE) all64 ARCH=x86_64

# Create build directory
$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD) vectord-os.iso vectord-os64.iso

