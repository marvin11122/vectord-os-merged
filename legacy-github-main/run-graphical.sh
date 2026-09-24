#!/bin/bash
echo "Starting VECTORD OS with graphical display..."
echo "VNC server will be available on port 5900"
echo "Connect with: vncviewer localhost:5900"
echo ""
echo "Or use: qemu-system-i386 -kernel build/vectord-kernel.elf -vnc :0"
echo ""
qemu-system-i386 \
    -kernel build/vectord-kernel.elf \
    -m 128M \
    -vga std \
    -vnc :0 \
    -rtc local-time \
    -netdev user,id=net0 \
    -device e1000,netdev=net0 \
    -drive file=test.img,format=raw,if=virtio 2>/dev/null || true
    
echo "QEMU ended. Starting without disk image..."
qemu-system-i386 \
    -kernel build/vectord-kernel.elf \
    -m 128M \
    -vga std \
    -vnc :0 &
    
QEMU_PID=$!
echo "QEMU started with PID: $QEMU_PID"
echo "VNC available at localhost:5900"
sleep 2

