# Merged build validation

Validated on 2026-09-24 with:

```sh
./scripts/check.sh
```

The script completed successfully. It checked the Multiboot2 header in source, built the kernel and host tests, ran the tests, and inspected the resulting ELF.

- Toolchain: GCC 13.3.0 and GNU ld 2.42.
- Kernel output: ELF64, x86-64, entry point `0x101000`, `.multiboot2` section present.
- µIR tests: known Dense → ReLU → Add result passed, plus ten malformed/boundary cases.
- ELF file size: 21,896 bytes.
- SHA-256: `b0bf7e7308652e7ef0d333b557484d3489da959949559563ea0ff31eacbd5d52`.
- The link completed without the previous GNU-stack warning.

This verifies compilation and static ELF properties. QEMU and NASM are unavailable here, so no emulator boot or legacy i386 build was performed. Hardware boot, driver integration, and kernel-side µIR execution remain unverified. The µIR VM is linked into the ELF but is not called by `kernel_main()`.
