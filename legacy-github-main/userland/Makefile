# ═══════════════════════════════════════════════════════════════════════
# VECTORD OS - Userland Build System
# Builds all userspace utilities using i686-elf cross-compiler
# ═══════════════════════════════════════════════════════════════════════

CC = i686-elf-gcc
LD = i686-elf-ld
OBJCOPY = i686-elf-objcopy

# If cross-compiler not available, use system gcc with 32-bit
CC := $(shell which i686-elf-gcc 2>/dev/null || echo gcc)
LD := $(shell which i686-elf-ld 2>/dev/null || echo ld)
OBJCOPY := $(shell which i686-elf-objcopy 2>/dev/null || echo objcopy)

CFLAGS = -m32 -ffreestanding -fno-stack-protector -fno-builtin \
         -nostdlib -nostdinc -fno-exceptions -fno-unwind-tables \
         -fno-pie -fno-pic -Wall -Wextra -O2 \
         -I.

LDFLAGS = -m elf_i386 -nostdlib

BUILD = build
BIN = bin

# Source files
LIBC_SRC = libc.c
INIT_SRC = init.c
SHELL_SRC = shell.c

# Utility sources
UTILS_SRC = ls.c cat.c echo.c mkdir.c rm.c cp.c grep.c \
            find.c ps.c top.c netstat.c ifconfig.c ping.c vi.c

# Object files
LIBC_OBJ = $(patsubst %.c,$(BUILD)/%.o,$(LIBC_SRC))
INIT_OBJ = $(patsubst %.c,$(BUILD)/%.o,$(INIT_SRC))
SHELL_OBJ = $(patsubst %.c,$(BUILD)/%.o,$(SHELL_SRC))
UTILS_OBJ = $(patsubst %.c,$(BUILD)/%.o,$(UTILS_SRC))

# All targets
ALL_INIT = $(BIN)/init
ALL_SHELL = $(BIN)/shell
ALL_UTILS = $(BIN)/ls $(BIN)/cat $(BIN)/echo $(BIN)/mkdir \
            $(BIN)/rm $(BIN)/cp $(BIN)/grep $(BIN)/find \
            $(BIN)/ps $(BIN)/top $(BIN)/netstat $(BIN)/ifconfig \
            $(BIN)/ping $(BIN)/vi

.PHONY: all clean init shell utils

all: dirs init shell utils

dirs:
	@mkdir -p $(BUILD)
	@mkdir -p $(BIN)

# Build libc (static library)
$(BUILD)/libc.a: $(LIBC_OBJ)
	@ar rcs $@ $^ 2>/dev/null || echo "Note: Building without static library"
	@echo "✓ Built libc"

# Init process
$(ALL_INIT): $(INIT_OBJ) $(LIBC_OBJ)
	$(LD) $(LDFLAGS) -T ../kernel/linker.ld -o $@ $^ --entry main
	@echo "✓ Built init"

# Shell
$(ALL_SHELL): $(SHELL_OBJ) $(LIBC_OBJ)
	$(LD) $(LDFLAGS) -T ../kernel/linker.ld -o $@ $^ --entry main
	@echo "✓ Built shell"

# Individual utilities
$(BIN)/ls: $(BUILD)/ls.o $(LIBC_OBJ)
	$(LD) $(LDFLAGS) -T ../kernel/linker.ld -o $@ $^ --entry main
	@echo "✓ Built ls"

$(BIN)/cat: $(BUILD)/cat.o $(LIBC_OBJ)
	$(LD) $(LDFLAGS) -T ../kernel/linker.ld -o $@ $^ --entry main
	@echo "✓ Built cat"

$(BIN)/echo: $(BUILD)/echo.o $(LIBC_OBJ)
	$(LD) $(LDFLAGS) -T ../kernel/linker.ld -o $@ $^ --entry main
	@echo "✓ Built echo"

$(BIN)/mkdir: $(BUILD)/mkdir.o $(LIBC_OBJ)
	$(LD) $(LDFLAGS) -T ../kernel/linker.ld -o $@ $^ --entry main
	@echo "✓ Built mkdir"

$(BIN)/rm: $(BUILD)/rm.o $(LIBC_OBJ)
	$(LD) $(LDFLAGS) -T ../kernel/linker.ld -o $@ $^ --entry main
	@echo "✓ Built rm"

$(BIN)/cp: $(BUILD)/cp.o $(LIBC_OBJ)
	$(LD) $(LDFLAGS) -T ../kernel/linker.ld -o $@ $^ --entry main
	@echo "✓ Built cp"

$(BIN)/grep: $(BUILD)/grep.o $(LIBC_OBJ)
	$(LD) $(LDFLAGS) -T ../kernel/linker.ld -o $@ $^ --entry main
	@echo "✓ Built grep"

$(BIN)/find: $(BUILD)/find.o $(LIBC_OBJ)
	$(LD) $(LDFLAGS) -T ../kernel/linker.ld -o $@ $^ --entry main
	@echo "✓ Built find"

$(BIN)/ps: $(BUILD)/ps.o $(LIBC_OBJ)
	$(LD) $(LDFLAGS) -T ../kernel/linker.ld -o $@ $^ --entry main
	@echo "✓ Built ps"

$(BIN)/top: $(BUILD)/top.o $(LIBC_OBJ)
	$(LD) $(LDFLAGS) -T ../kernel/linker.ld -o $@ $^ --entry main
	@echo "✓ Built top"

$(BIN)/netstat: $(BUILD)/netstat.o $(LIBC_OBJ)
	$(LD) $(LDFLAGS) -T ../kernel/linker.ld -o $@ $^ --entry main
	@echo "✓ Built netstat"

$(BIN)/ifconfig: $(BUILD)/ifconfig.o $(LIBC_OBJ)
	$(LD) $(LDFLAGS) -T ../kernel/linker.ld -o $@ $^ --entry main
	@echo "✓ Built ifconfig"

$(BIN)/ping: $(BUILD)/ping.o $(LIBC_OBJ)
	$(LD) $(LDFLAGS) -T ../kernel/linker.ld -o $@ $^ --entry main
	@echo "✓ Built ping"

$(BIN)/vi: $(BUILD)/vi.o $(LIBC_OBJ)
	$(LD) $(LDFLAGS) -T ../kernel/linker.ld -o $@ $^ --entry main
	@echo "✓ Built vi"

# Compile rules
$(BUILD)/%.o: %.c libc.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# Target groups
init: dirs $(ALL_INIT)

shell: dirs $(ALL_SHELL)

utils: dirs $(ALL_UTILS)

clean:
	rm -rf $(BUILD) $(BIN)

