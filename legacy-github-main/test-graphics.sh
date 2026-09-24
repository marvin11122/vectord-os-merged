#!/bin/bash
echo "═══════════════════════════════════════════════════════"
echo "  VECTORD OS - Graphics Test"
echo "═══════════════════════════════════════════════════════"
echo ""
echo "Starting QEMU with VNC display on port 5900..."
echo "Connect using: vncviewer localhost:5900"
echo ""
echo "Alternative: Using SDL display..."
echo ""

# Kill any existing QEMU
pkill -f "qemu.*vectord" 2>/dev/null || true
sleep 1

# Start QEMU with VNC
qemu-system-i386 \
    -kernel build/vectord-kernel.elf \
    -m 128M \
    -display vnc=:0 \
    -vga std \
    -no-reboot \
    -serial stdio 2>&1 &
    
QEMU_PID=$!
echo "QEMU started with PID: $QEMU_PID"
echo ""
echo "VNC Server: localhost:5900"
echo "Serial output: current terminal"
echo ""
echo "Press Ctrl+C to stop"
echo "═══════════════════════════════════════════════════════"

wait $QEMU_PID

