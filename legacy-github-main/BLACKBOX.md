# BLACKBOX.md

## Project Overview

**VECTORD OS** is a standalone 32-bit x86 operating system built from scratch (not Linux/Unix/BSD-based).
It includes a custom kernel, hardware drivers, networking, filesystems, graphics/desktop components, and an AI subsystem (Akasha).

### Core Stack
- **Languages:** C (kernel/modules), x86 Assembly (boot)
- **Target:** `i386` (32-bit), freestanding environment
- **Build tools:** `gcc`, `nasm`, `ld`, `make`
- **Boot/Emulation:** GRUB (ISO boot), QEMU (kernel/ISO testing)

### High-Level Architecture
- Boot path: `kernel/boot.asm` → `kernel/kernel.c` → subsystem init
- Main subsystems include:
  - Core kernel/runtime: interrupts, memory, scheduler, shell
  - Drivers: PCI, ATA/AHCI, USB, RTC, RNG, mouse, network NICs
  - Graphics/UI: VBE/VGA, compositor, desktop modules
  - Storage/FS: VFS, ext2, native vectord filesystem
  - Networking: full stack module (`network_full.c`)
  - AI: `vector_ai.c` + Python Akasha tooling under `akasha/`

---

## Repository Map (Important Paths)

- `Makefile` — canonical build/run/test targets
- `include/vecord.h` — master kernel header/types/APIs (central declarations)
- `kernel/` — kernel and subsystem source modules
- `boot/grub.cfg` — GRUB boot config for ISO
- `build/` — generated build artifacts (`vectord-kernel.elf`, ISO staging)
- `akasha/` — AI Python tooling (`god_repl.py`, orchestrator, Dockerfile)
- `LIVE_CD.md` — ISO/live boot usage
- `AGENTS.md` — practical developer guide for contributors/agents
- `DEVELOPMENT_LOG.md`, `VECTORD_OS_UPDATE.md` — status/history/roadmap context

---

## Building, Running, and Testing

Use the `Makefile` as source of truth.

### Build
```bash
make clean && make
```
Produces:
- `build/vectord-kernel.elf`

### Build ISO
```bash
make iso
```
Produces:
- `vectord-os.iso`

### Run kernel directly (headless serial)
```bash
make run
```
Equivalent to QEMU kernel boot with serial output.

### Run ISO (headless serial)
```bash
make run-iso
```

### Useful manual QEMU examples (from docs/scripts)
```bash
# Graphical live ISO
qemu-system-i386 -cdrom vectord-os.iso -boot d -m 256M -vga std -display sdl

# Serial live ISO
qemu-system-i386 -cdrom vectord-os.iso -boot d -m 256M -nographic -serial mon:stdio

# Kernel with network emulation
qemu-system-i386 -kernel build/vectord-kernel.elf -nographic -serial mon:stdio -net nic,model=e1000 -net user
```

### Helper scripts
- `run-graphical.sh` — starts QEMU with VNC/graphical profile
- `test-graphics.sh` — graphics-focused QEMU run
- `test-vnc.sh` — VNC smoke run
- `capture-screenshot.sh` — notes for VNC screenshot capture

---

## Toolchain / Environment Requirements

From repository docs and build config:
- `gcc` with 32-bit support
- `nasm`
- `ld`
- `qemu-system-i386`
- `grub-mkrescue`
- `xorriso`

If ISO creation fails, verify GRUB/xorriso availability first.

---

## Development Conventions

## 1) Kernel types and headers
- Use typedefs from `include/vecord.h` (`u8`, `u16`, `u32`, `u64`, etc.)
- Keep public declarations centralized in `include/vecord.h`
- Avoid standard library assumptions; this is freestanding code

## 2) Module integration workflow
When adding a new kernel module:
1. Add source file in `kernel/`
2. Add it to `KERNEL_C` in `Makefile`
3. Add declarations to `include/vecord.h`
4. Wire boot/init or shell entry points in relevant kernel files

## 3) Shell command integration
- Extend help text and command dispatch logic in `kernel/kernel.c`
- Keep command names short and consistent with existing style

## 4) Debugging style
- Prefer serial debugging helpers (`serial_puts`, numeric printers)
- Validate changes in QEMU headless mode first, then graphical mode if needed

## 5) Build characteristics
Compilation uses freestanding flags (from `Makefile`):
- `-m32 -ffreestanding -nostdlib -nostdinc`
- no PIE/PIC, no stack protector
- optimized C compilation with warnings enabled

---

## Testing Practices

### Recommended quick validation loop
1. `make clean && make`
2. `make run` and validate boot + shell responsiveness
3. Run key shell checks (example):
   - `help`
   - `ver`
   - `mem`
   - `ps`
   - `netstat` / `ai stats` (if subsystem touched)
4. If graphics/UI changed, run graphical test path (`test-graphics.sh` or live ISO SDL/VNC flow)
5. If boot/ISO packaging changed, run `make iso` and `make run-iso`

### Subsystem-specific verification
- **Drivers/network/filesystem:** verify module init logs on serial output
- **Desktop/compositor:** validate VBE/VGA mode behavior in graphical QEMU
- **AI subsystem:** validate kernel AI commands and Python-side Akasha tools separately

---

## Agent Notes for Future Interactions

- Start with `Makefile`, `AGENTS.md`, and `LIVE_CD.md` for operational context.
- For architecture/type/API understanding, inspect `include/vecord.h` and relevant `kernel/*.c` modules.
- Treat `DEVELOPMENT_LOG.md` and `VECTORD_OS_UPDATE.md` as useful but potentially aspirational; verify behavior against current code and build targets.
- Prefer minimal, isolated kernel changes and verify frequently with QEMU serial output.

---

## Known Gaps / Ongoing Work (from docs)

Documentation indicates ongoing/roadmap areas such as:
- deeper filesystem support/expansion
- ISO/boot process improvements
- additional hardware support and real-hardware hardening

Treat these as directional context; confirm current implementation status directly in code before planning major changes.

