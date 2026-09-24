# VECTORD OS - Developer Guide for AI Agents

## Quick Start

```bash
cd /home/llll/vectord-build
make clean && make          # Build kernel
qemu-system-i386 -kernel build/vectord-kernel.elf -nographic -serial mon:stdio  # Test
```

## Architecture

### Build System
- **Makefile** compiles 29 C modules + 1 ASM module
- Target: i386 (32-bit x86)
- Flags: `-m32 -ffreestanding -nostdlib`
- Output: `build/vectord-kernel.elf` (~156KB)

### Kernel Structure
```
boot.asm → kernel.c (main) → Hardware drivers
                             → Network stack
                             → Filesystems
                             → AI module
                             → Desktop environment
```

### Key Files
| File | Purpose |
|------|---------|
| `kernel/kernel.c` | Main kernel, shell, commands |
| `kernel/desktop_hd.c` | Premium HD desktop (Spotlight, Dock, etc) |
| `kernel/hyprland_compositor.c` | Window manager with animations |
| `kernel/vector_ai.c` | Vector AI (Akasha) integration |
| `kernel/vbe_fhd.c` | 1920x1080 framebuffer |
| `kernel/network_full.c` | TCP/IP stack |
| `include/vecord.h` | Master header with all declarations |

### Adding New Features

1. **Add source file** to `kernel/` directory
2. **Add to Makefile**: Update `KERNEL_C` variable
3. **Add declarations** to `include/vecord.h`
4. **Add command** to shell in `kernel/kernel.c`:
   ```c
   } else if (strcmp(command, "mycommand") == 0) {
       my_function();
   }
   ```

### Coding Standards
- Use `u8`, `u16`, `u32` for unsigned integers (from `vecord.h`)
- Prefix functions with module name (e.g., `hd_desktop_init`)
- Use `serial_puts()` for debug output
- Handle errors gracefully with early returns

## Testing

### QEMU Commands
```bash
# Basic serial test
qemu-system-i386 -kernel build/vectord-kernel.elf -nographic -serial mon:stdio

# With VGA (requires display)
qemu-system-i386 -kernel build/vectord-kernel.elf -vga std -display sdl

# With network
qemu-system-i386 -kernel build/vectord-kernel.elf -nographic -serial mon:stdio -net nic,model=e1000 -net user

# With disk image
qemu-system-i386 -kernel build/vectord-kernel.elf -nographic -serial mon:stdio -hda disk.img
```

### Shell Commands for Testing
```
help              # List all commands
ver               # Version info
mem               # Memory statistics
ps                # Process list
desktop-hd        # Test HD desktop
desktop-hypr      # Test compositor
ai stats          # AI module status
neural status     # Neural optimizer
netstat           # Network stats
```

## AI System (Akasha)

### Python Components (require GPU)
```bash
# God-Mode REPL
python3 akasha/god_repl.py

# Knowledge ingestion
python3 akasha/akasha_orchestrator.py

# Docker container
docker build -t vectord-akasha:v1 akasha/
docker run -it --gpus all vectord-akasha:v1
```

### Kernel AI Integration
- `vector_ai.c` provides basic concept storage
- `ai_init()` loads fundamental axioms at boot
- `ai_query_concept()` searches knowledge base
- `ai_ingest_concept()` adds new knowledge

## Common Tasks

### Adding a New Shell Command
```c
// In kernel/kernel.c, add to cmd_help():
serial_puts("  mycommand        - Description\n");

// Add to process_command():
} else if (strcmp(command, "mycommand") == 0) {
    my_command_handler();
}
```

### Adding a New Driver
1. Create `kernel/mydriver.c`
2. Add to Makefile: `KERNEL_C += kernel/mydriver.c`
3. Add `void mydriver_init(void);` to `vecord.h`
4. Call from kernel.c boot sequence

### Adding Desktop Features
1. Add rendering function to `kernel/desktop_hd.c`
2. Add mode to `desktop_mode_t` enum
3. Handle in `hd_desktop_keypress()` for shortcuts
4. Call from `hd_desktop_render()`

## Debugging

### Serial Output
All debug output goes to serial (COM1). Use:
```c
serial_puts("Debug message\n");
serial_puthex(value);  // Print hex
serial_putdec(value);  // Print decimal
```

### Common Issues
- **Kernel panic**: Check for NULL pointers, stack overflow
- **Build errors**: Check includes in vecord.h
- **Missing symbols**: Add function to header file

## Version History
- v2.0.0: HD Desktop, Hyprland compositor, Vector AI
- v1.0.0: Initial kernel with basic drivers

