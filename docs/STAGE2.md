# Stage 2: x86-64 baseline

The active kernel contains:

- A 64-bit IDT with exception stubs 0..31 and a diagnostic fault dispatcher.
- Read-only local APIC discovery via CPUID and `IA32_APIC_BASE`.
- Bootstrap paging that identity-maps the first 1 GiB with 2 MiB pages.
- A 16 MiB first-fit/coalescing heap initialized after `_kernel_end`.
- A read-only PCI Configuration Mechanism #1 scanner.
- COM1 diagnostics for each boot stage.
- A µIR v0 VM and scalar dense reference implementation linked into the ELF; the boot path does not invoke them.

I/O APIC routing and SMP startup are not enabled. The heap remains inside the bootstrap's 1 GiB identity map. PCI enumeration does not size or write BARs. Older 32-bit and GitHub sources are retained under `legacy-archive-snapshot/` and `legacy-github-main/`; neither is linked into this kernel.

Build and µIR host-test results for this merged copy are recorded in `MERGE_VALIDATION.md`.
