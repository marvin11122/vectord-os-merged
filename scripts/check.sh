#!/usr/bin/env bash
set -euo pipefail
for c in gcc ld make readelf; do command -v "$c" >/dev/null || { echo "missing: $c"; exit 1; }; done
echo "toolchain: OK"
grep -q "0xE85250D6" src/arch/x86_64/boot.S
grep -q "mb2_end-mb2_start" src/arch/x86_64/boot.S
echo "multiboot2 header source: OK"
make clean all check
