# VecOS merged source bundle

This working bundle uses the compact x86-64 Stage-2 tree from `VecOS-FULL.zip` as its active build, incorporates and tests the archive's µIR v0 implementation, and keeps the newer GitHub sources available for comparison and future ports.

## Active build

- `src/arch/x86_64/`: Multiboot2 entry, long-mode setup, ISR stubs, and linker script.
- `src/kernel/`: serial diagnostics, exception IDT, existing paging, heap, read-only APIC discovery, and PCI enumeration.
- `src/kernel/ai/`: µIR v0 decoder/VM and scalar dense reference kernel.
- `tests/muir/`: host-side valid and malformed stream tests run by `make check`.

The µIR code is linked into the kernel ELF, but the boot path does not invoke it. The successful host test establishes parser and reference-kernel behavior on the test host, not kernel-side execution.

## Preserved source and provenance

- `legacy-archive-snapshot/`: extracted legacy tree and original archive included in the uploaded `VecOS-FULL.zip`.
- `legacy-github-main/`: source snapshot of `marvin11122/vectord-os` at commit `65de6f9d43f3dad93d42d82200b7abf6169990ae`.
- `docs/reference/`: historical project material from the selected archive.
- `agent/`, `fuzz/`, `qemu/`, `tools/`: µIR engineering scaffolding and boot helpers.

Neither legacy tree is linked into the active x86-64 kernel. The curated GitHub snapshot omits generated kernel objects, the committed kernel ELF, ISO images, and disk image; their sizes and paths are recorded in `legacy-github-main/SOURCE_SNAPSHOT.txt`. The committed ELF32 programs under `legacy-github-main/userland/bin/` are kept for review but are not inputs to the active Makefile.

## Verification

From the project root:

    make clean
    make -j2
    make check

The Makefile uses GCC's integrated assembler for `.S` files, so NASM is not required. `make check` builds the ELF64 kernel, executes the µIR host tests, and checks the ELF architecture and Multiboot2 section. `make run` requires QEMU; ISO creation also requires GRUB tools.

See `docs/MERGE_VALIDATION.md` for the result from this merged copy and `MERGE_DECISIONS.md` for the base selection and scope.
