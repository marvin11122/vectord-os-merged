CC ?= gcc
HOSTCC ?= cc
LD ?= ld
BUILD := build
CFLAGS := -m64 -ffreestanding -fno-stack-protector -fno-pie -fno-pic -mno-red-zone -mcmodel=kernel -nostdlib -Wall -Wextra -Werror -O2 -Iinclude
HOST_CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror
ASFLAGS := -m64 -ffreestanding -fno-pie -fno-pic -c
LDFLAGS := -m elf_x86_64 -nostdlib -T src/arch/x86_64/linker.ld
C_SRCS := src/kernel/main.c src/kernel/serial.c src/kernel/idt.c src/kernel/apic.c src/kernel/memory.c src/kernel/pci.c
AI_SRCS := src/kernel/ai/muir_vm.c src/kernel/ai/dense_ref.c
AI_OBJS := $(BUILD)/ai_muir_vm.o $(BUILD)/ai_dense_ref.o
C_OBJS := $(patsubst src/kernel/%.c,$(BUILD)/%.o,$(C_SRCS))
ASM_OBJS := $(BUILD)/boot.o $(BUILD)/isr.o

.PHONY: all clean check test-muir run iso
all: $(BUILD)/vectord.elf
$(BUILD):
	mkdir -p $(BUILD)
$(BUILD)/%.o: src/kernel/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/boot.o: src/arch/x86_64/boot.S | $(BUILD)
	$(CC) $(ASFLAGS) $< -o $@
$(BUILD)/isr.o: src/arch/x86_64/isr.S | $(BUILD)
	$(CC) $(ASFLAGS) $< -o $@
$(BUILD)/ai_muir_vm.o: src/kernel/ai/muir_vm.c | $(BUILD)
	$(CC) $(CFLAGS) -Isrc/kernel/ai -c $< -o $@
$(BUILD)/ai_dense_ref.o: src/kernel/ai/dense_ref.c | $(BUILD)
	$(CC) $(CFLAGS) -Isrc/kernel/ai -c $< -o $@
$(BUILD)/vectord.elf: $(ASM_OBJS) $(C_OBJS) $(AI_OBJS)
	$(LD) $(LDFLAGS) $^ -o $@
	@echo "BUILD OK: $@"
$(BUILD)/test_muir: tests/muir/test_muir.c src/kernel/ai/muir_vm.c src/kernel/ai/dense_ref.c src/kernel/ai/muir.h | $(BUILD)
	$(HOSTCC) $(HOST_CFLAGS) -Isrc/kernel/ai tests/muir/test_muir.c src/kernel/ai/muir_vm.c src/kernel/ai/dense_ref.c -o $@
test-muir: $(BUILD)/test_muir
	./$(BUILD)/test_muir
check: all test-muir
	@readelf -h $(BUILD)/vectord.elf | grep -E 'Class:|Machine:|Entry point'
	@readelf -S $(BUILD)/vectord.elf | grep multiboot2
	@echo "BUILD, µIR TESTS, STATIC CHECK: OK"
run: all
	qemu-system-x86_64 -kernel $(BUILD)/vectord.elf -m 256M -serial stdio -display none
iso: all
	rm -rf $(BUILD)/iso && mkdir -p $(BUILD)/iso/boot/grub
	cp $(BUILD)/vectord.elf $(BUILD)/iso/boot/vectord.elf
	cp boot/grub.cfg $(BUILD)/iso/boot/grub/grub.cfg
	grub-mkrescue -o $(BUILD)/vectord.iso $(BUILD)/iso
clean:
	rm -rf $(BUILD)
