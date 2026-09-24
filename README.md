# VECTORD OS — Merged x86_64 Baseline

This tree keeps the small x86_64 Stage-2 kernel as the active build and
preserves both historical source trees for deliberate, one-subsystem-at-a-time
ports. The active kernel does not bulk-link the old i386 modules.

## Active implementation

src/arch/x86_64/boot.S enters through Multiboot2, builds bootstrap identity
maps, enables long mode, and calls src/kernel/main.c. The kernel initializes
serial diagnostics, an exception IDT, the existing page tables, a first-fit
heap, read-only local-APIC discovery, and read-only PCI enumeration.

The µIR v0 decoder and scalar dense reference are linked into the kernel ELF.
Their standalone host tests run from make check. The current boot path does
not yet call the µIR VM; linkage and host-side correctness tests are not a claim
of kernel-side AI execution.

## Build and checks

    make clean
    make -j2
    make check

Builds use GCC's integrated assembler for the .S boot and ISR sources; NASM
is not required. make check links the ELF64 kernel, runs µIR valid-input and
malformed-input tests, then checks the ELF header and Multiboot2 section.
make run additionally requires qemu-system-x86_64.

For Debian 13, the usual build tools are build-essential and binutils.
QEMU and GRUB tools are needed only for emulator runs and ISO creation.

## Merged source provenance

- src/, include/vectord/, and tests/ are the active x86_64 baseline and
  µIR tests.
- legacy-archive-snapshot/ preserves the older tree and original archive
  bundled with VecOS-FULL.zip.
- legacy-github-main/ preserves the newer GitHub source snapshot from commit
  65de6f9d43f3dad93d42d82200b7abf6169990ae.

Neither legacy tree is added to the active link. See MERGE_DECISIONS.md and
docs/MIGRATION.md before porting modules.

## Scope

This is a bootable-development baseline, not a complete operating system.
The x86_64 tree does not yet contain the legacy network stack, storage/VFS,
userspace, process context switching, or desktop. APIC interrupt routing and
SMP are not enabled. The heap and PCI paths are prototype implementations;
QEMU and physical-hardware boot validation remain separate checks.
