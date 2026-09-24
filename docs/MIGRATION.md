# Source and migration notes

The merged bundle keeps the small x86-64 Stage-2 kernel as its only active build. It also includes two separate historical source snapshots: the older tree extracted from `VecOS-FULL.zip` under `legacy-archive-snapshot/`, and the newer GitHub main-branch snapshot under `legacy-github-main/`.

Neither historical tree is part of the active Makefile. This avoids combining 32-bit assumptions and incomplete x86-64 transition code into the current kernel. Port individual subsystems only after reviewing their calling conventions, interrupt and memory model, and testing them independently.

The active Multiboot2 header, page-table setup, saved boot registers, and read-only MSR policy come from the selected x86-64 baseline. The µIR implementation is linked into the kernel but remains disconnected from the boot path. The host tests validate its standalone stream execution only.

Before describing legacy driver, networking, storage, UI, or userland features as part of the x86-64 OS, they need native ports and their own boot or integration tests. Current known limits are summarized in `README.md` and `MERGE_DECISIONS.md`.
