#!/bin/bash
echo "Starting VECTORD OS with VNC graphics..."
echo "Connect with: vncviewer localhost:5900"
echo ""

# Start QEMU with VNC in background
qemu-system-i386 \
    -kernel build/vectord-kernel.elf \
    -m 128M \
    -vga std \
    -vnc :0 \
    -no-reboot &
    
QEMU_PID=$!
echo "QEMU PID: $QEMU_PID"
echo ""
sleep 3

# Send command to execute graphics demo
echo "Waiting for boot..."
sleep 5

# Kill QEMU
kill $QEMU_PID 2>/dev/null
echo "QEMU stopped"

