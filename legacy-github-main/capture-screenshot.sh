#!/bin/bash
# This would require vncsnapshot or similar tool
echo "To capture screenshot:"
echo "1. Run: qemu-system-i386 -kernel build/vectord-kernel.elf -display vnc=:0 &"
echo "2. Use: vncsnapshot localhost:5900 screenshot.jpg"
echo "3. Or: vncdotool screenshot"

