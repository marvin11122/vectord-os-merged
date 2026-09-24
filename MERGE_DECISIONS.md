# Merge decisions

## Selected active base

The attached VecOS-FULL archive provides the better base for continued x86_64
work. It separates a compact Multiboot2/long-mode kernel from the legacy i386
tree, makes architecture-specific source selection explicit, and keeps the
legacy modules out of the 64-bit link.

The GitHub main branch at
65de6f9d43f3dad93d42d82200b7abf6169990ae remains the broader source of existing
drivers, UI, networking, storage, userland, and documentation. Those files are
preserved under legacy-github-main for porting and comparison. They are not
bulk-linked because the audit found 32-bit assumptions, incomplete context
switching, syscall and ELF-loader defects, and simulated network transmission.

The archive's older source tree and its original compressed archive remain
under legacy-archive-snapshot. Neither provenance snapshot was overlaid.

## Changes made in this merged working copy

- Made the README boot path and assembler requirements match the checked-in
  source and Makefile.
- Added a repeatable host-side µIR test target to make check.
- Added coverage for a known dense/ReLU/Add result and malformed headers, invalid
  tensor IDs and shapes, unknown/trailing opcodes, NaN values, and instruction
  budget exhaustion.
- Tightened µIR parsing: payload size must match the stream, the end opcode
  must be final, reserved flags must be zero, each execution clears prior
  tensor lengths, non-finite tensor/math values are rejected, and tolerances
  must be finite and non-negative.
- Updated the toolchain check to match the GCC .S assembler rules.
- Removed checked-in generated build outputs from the active build directory;
  make recreates them from source.

## Deliberately not represented as completed

The legacy driver/UI/userland sources are not native x86_64 ports. µIR is
linked into the kernel image but is not called by the boot path. A successful
host test does not establish QEMU or physical-hardware behavior.

## Source snapshot

- GitHub repository: marvin11122/vectord-os
- Commit: 65de6f9d43f3dad93d42d82200b7abf6169990ae
- Archive: user-provided VecOS-FULL.zip
