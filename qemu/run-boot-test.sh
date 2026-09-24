#!/bin/sh
set -eu
# Integration hook: set VECOS_IMAGE to your bootable VecOS image.
: "${VECOS_IMAGE:?set VECOS_IMAGE to the VecOS boot image}"
LOG=${LOG:-qemu-serial.log}
timeout 20s qemu-system-x86_64 -m 256M -no-reboot -no-shutdown -serial file:"$LOG" -display none -drive format=raw,file="$VECOS_IMAGE" || true
grep -q '@VECOS:MUIR:PASS' "$LOG"
echo 'QEMU µIR boot invariant: PASS'
