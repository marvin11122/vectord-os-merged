# VECTORD OS API Reference
## Kernel Programming Interface

**Version:** 2.0.0  
**Date:** 2026-03-12  

---

## Table of Contents

1. [Types](#types)
2. [Port I/O](#port-io)
3. [VGA Display](#vga-display)
4. [Memory Management](#memory-management)
5. [Vector Memory](#vector-memory)
6. [Process Management](#process-management)
7. [Vector Math](#vector-math)
8. [Hardware Drivers](#hardware-drivers)
9. [Network Drivers](#network-drivers)
10. [Input Devices](#input-devices)
11. [Neural Network](#neural-network)

---

## Types

### Basic Types

```c
typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;
typedef signed int          i32;
typedef signed long long    i64;
typedef u32                 size_t;

#define bool  int
#define true  1
#define false 0
#define NULL  ((void*)0)
```

### Vector Types

```c
typedef union {
    struct { float x, y; };
    float v[2];
} vec2_t;

typedef union {
    struct { float x, y, z; };
    float v[3];
} vec3_t;

typedef union {
    struct { float x, y, z, w; };
    float v[4];
} vec4_t;

// 4D Vector Memory Address
typedef struct {
    float x;  // Page number
    float y;  // Frame offset
    float z;  // Segment
    float w;  // Semantic class
} vec_addr_t;
```

### Process Types

```c
typedef enum {
    PROC_DORMANT = 0,
    PROC_SPINNING,
    PROC_ORBITING,
    PROC_TRANSLATING,
    PROC_REFLECTING
} proc_state_t;

typedef struct process {
    u32 pid;
    char name[32];
    vec4_t position;
    vec4_t velocity;
    vec4_t momentum;
    float rotation;
    u32 esp, ebp, eip;
    u32 eax, ebx, ecx, edx;
    u32 esi, edi, eflags;
    vec_addr_t code_addr;
    vec_addr_t stack_addr;
    u32 stack_base, stack_size;
    proc_state_t state;
    u32 quantum;
    u32 cpu_time;
    struct process *next;
    struct process *prev;
} process_t;
```

### Hardware Types

```c
typedef struct {
    u16 vendor_id;
    u16 device_id;
    u8  class_code;
    u8  subclass;
    u8  prog_if;
    u8  revision;
    u8  header_type;
    u8  bus;
    u8  slot;
    u8  func;
    u32 bar[6];
    u8  interrupt_line;
    u8  interrupt_pin;
} pci_device_t;

typedef struct {
    u16 base_port;
    u16 ctrl_port;
    u8  slave;
    u8  present;
    u32 sectors;
    char model[41];
    char serial[21];
} ata_device_t;

typedef struct {
    usb_type_t type;
    u32 base;
    u8  irq;
    u8  present;
    char type_name[8];
} usb_controller_t;

typedef struct {
    u32 base;
    u32 width;
    u32 height;
    u32 pitch;
    u8  bpp;
    u8  present;
    u32 size;
} vbe_mode_info_t;

typedef struct {
    char cpu_vendor[13];
    char cpu_brand[49];
    u32 cpu_family;
    u32 cpu_model;
    u32 cpu_stepping;
    u32 cpu_speed_mhz;
    u32 ram_total_mb;
    char bios_vendor[65];
    char bios_version[65];
    char bios_date[65];
    char mb_vendor[65];
    char mb_product[65];
    char mb_version[65];
} hardware_info_t;
```

---

## Port I/O

### outb
Write a byte to an I/O port.

```c
void outb(u16 port, u8 val);
```

**Parameters:**
- `port`: I/O port address
- `val`: Byte value to write

**Example:**
```c
outb(0x3F8, 'A');  // Send 'A' to serial port
```

### inb
Read a byte from an I/O port.

```c
u8 inb(u16 port);
```

**Returns:** Byte value read from port

### outw
Write a 16-bit word to an I/O port.

```c
void outw(u16 port, u16 val);
```

### inw
Read a 16-bit word from an I/O port.

```c
u16 inw(u16 port);
```

### outl
Write a 32-bit dword to an I/O port.

```c
void outl(u16 port, u32 val);
```

### inl
Read a 32-bit dword from an I/O port.

```c
u32 inl(u16 port);
```

### io_wait
I/O wait for ISA bus timing.

```c
void io_wait(void);
```

---

## VGA Display

### vga_init
Initialize VGA text mode (80x25).

```c
void vga_init(void);
```

### vga_clear
Clear the screen with specified color.

```c
void vga_clear(u8 color);
```

**Colors:**
```c
#define VGA_BLACK       0
#define VGA_BLUE        1
#define VGA_GREEN       2
#define VGA_CYAN        3
#define VGA_RED         4
#define VGA_MAGENTA     5
#define VGA_BROWN       6
#define VGA_LIGHT_GRAY  7
#define VGA_DARK_GRAY   8
#define VGA_LIGHT_BLUE  9
#define VGA_LIGHT_GREEN 10
#define VGA_LIGHT_CYAN  11
#define VGA_LIGHT_RED   12
#define VGA_PINK        13
#define VGA_YELLOW      14
#define VGA_WHITE       15
```

### vga_putchar
Write a single character at position.

```c
void vga_putchar(int x, int y, char c, u8 color);
```

**Parameters:**
- `x`: Column (0-79)
- `y`: Row (0-24)
- `c`: Character to display
- `color`: Foreground/background color byte

### vga_print
Write a string at position.

```c
void vga_print(int x, int y, const char *str, u8 color);
```

### vga_put_hex
Display a hexadecimal value.

```c
void vga_put_hex(int x, int y, u32 value, u8 color);
```

### vga_put_dec
Display a decimal value.

```c
void vga_put_dec(int x, int y, u32 value, u8 color);
```

---

## Memory Management

### mem_init
Initialize the memory allocator.

```c
void mem_init(u32 mem_size);
```

**Parameters:**
- `mem_size`: Size of heap in bytes

### kmalloc
Allocate memory from kernel heap.

```c
void *kmalloc(u32 size);
```

**Parameters:**
- `size`: Number of bytes to allocate

**Returns:** Pointer to allocated memory, or NULL on failure

**Example:**
```c
void *buf = kmalloc(1024);
if (buf) {
    // Use buffer
}
```

### kfree
Free allocated memory.

```c
void kfree(void *ptr);
```

### mem_get_total
Get total heap size.

```c
u32 mem_get_total(void);
```

**Returns:** Total heap size in bytes

### mem_get_used
Get used heap memory.

```c
u32 mem_get_used(void);
```

**Returns:** Used memory in bytes

### mem_get_free
Get free heap memory.

```c
u32 mem_get_free(void);
```

**Returns:** Free memory in bytes

---

## Vector Memory

### vec_alloc
Allocate vector memory with semantic class.

```c
vec_addr_t vec_alloc(u32 size, u32 semantic_class);
```

**Parameters:**
- `size`: Allocation size in bytes
- `semantic_class`: Semantic classification (0=kernel, 1=user, 2=video, etc.)

**Returns:** Vector address

**Semantic Classes:**
```c
#define SEMANTIC_KERNEL   0
#define SEMANTIC_USER     1
#define SEMANTIC_VIDEO    2
#define SEMANTIC_NETWORK  3
#define SEMANTIC_BUFFER   4
```

### vec_resolve
Resolve vector address to physical pointer.

```c
void *vec_resolve(vec_addr_t addr);
```

### vec_free
Free vector memory.

```c
void vec_free(vec_addr_t addr);
```

---

## Process Management

### proc_init
Initialize the process scheduler.

```c
void proc_init(void);
```

### proc_create
Create a new process.

```c
process_t *proc_create(const char *name, void (*entry)(void));
```

**Parameters:**
- `name`: Process name (max 31 chars)
- `entry`: Entry point function

**Returns:** Pointer to process control block

### proc_destroy
Destroy a process.

```c
void proc_destroy(process_t *proc);
```

### proc_schedule
Run the scheduler (called by timer interrupt).

```c
void proc_schedule(void);
```

### proc_yield
Yield CPU to next process.

```c
void proc_yield(void);
```

### proc_get_current
Get current running process.

```c
process_t *proc_get_current(void);
```

---

## Vector Math

### vec2 Operations

```c
vec2_t v2_add(vec2_t a, vec2_t b);        // a + b
vec2_t v2_sub(vec2_t a, vec2_t b);        // a - b
vec2_t v2_scale(vec2_t v, float s);       // v * s
float  v2_dot(vec2_t a, vec2_t b);        // dot(a, b)
float  v2_length(vec2_t v);               // |v|
vec2_t v2_normalize(vec2_t v);            // v / |v|
```

### vec3 Operations

```c
vec3_t v3_add(vec3_t a, vec3_t b);        // a + b
vec3_t v3_cross(vec3_t a, vec3_t b);      // a × b
float  v3_dot(vec3_t a, vec3_t b);        // dot(a, b)
float  v3_length(vec3_t v);               // |v|
```

### vec4 Operations

```c
vec4_t v4_add(vec4_t a, vec4_t b);        // a + b
vec4_t v4_sub(vec4_t a, vec4_t b);        // a - b
vec4_t v4_scale(vec4_t v, float s);       // v * s
float  v4_dot(vec4_t a, vec4_t b);        // dot(a, b)
vec4_t v4_lerp(vec4_t a, vec4_t b, float t);  // linear interpolation
```

**Example:**
```c
vec2_t pos = {{100.0f, 200.0f}};
vec2_t vel = {{1.0f, 0.5f}};
pos = v2_add(pos, v2_scale(vel, dt));
```

---

## Hardware Drivers

### PCI

```c
// Initialize PCI and scan all buses
void pci_init(void);

// Scan all PCI buses
void pci_scan_bus(void);

// Read PCI config space
u32 pci_config_read(u8 bus, u8 slot, u8 func, u8 offset);

// Find device by class
pci_device_t *pci_find_class(u8 class, u8 subclass);

// Find device by vendor/device ID
pci_device_t *pci_find_vendor_device(u16 vendor, u16 device);
```

**PCI Class Codes:**
```c
#define PCI_CLASS_NONE              0x00
#define PCI_CLASS_STORAGE           0x01
#define PCI_CLASS_NETWORK           0x02
#define PCI_CLASS_DISPLAY           0x03
#define PCI_CLASS_MULTIMEDIA        0x04
#define PCI_CLASS_MEMORY            0x05
#define PCI_CLASS_BRIDGE            0x06
#define PCI_CLASS_COMMUNICATION     0x07
#define PCI_CLASS_SYSTEM            0x08
#define PCI_CLASS_INPUT             0x09
#define PCI_CLASS_SERIAL            0x0C
```

### ATA/IDE

```c
// Initialize ATA subsystem
void ata_init(void);

// Detect device on specified ports
void ata_detect(u16 base, u16 ctrl, u8 slave);

// Read sectors (PIO mode)
int ata_read_sectors(ata_device_t *dev, u32 lba, u8 count, u16 *buffer);

// Write sectors (PIO mode)
int ata_write_sectors(ata_device_t *dev, u32 lba, u8 count, const u16 *buffer);
```

### AHCI/SATA

```c
// Initialize AHCI subsystem
void ahci_init(void);

// Detect AHCI controller
void ahci_detect(u32 base, u8 irq);
```

### USB

```c
// Initialize USB subsystem
void usb_init(void);

// Detect USB controller from PCI
void usb_detect_controller(u16 vendor_id, u16 device_id, u8 prog_if, 
                           u32 bar0, u8 irq);
```

**USB Types:**
```c
typedef enum {
    USB_UHCI = 0,
    USB_OHCI,
    USB_EHCI,
    USB_XHCI
} usb_type_t;
```

### VESA/VBE

```c
// Initialize VBE subsystem
void vbe_init(void);
```

**Global variable:**
```c
extern vbe_mode_info_t vbe_info;
```

### Hardware Detection

```c
// Run full hardware detection
void hardware_detect(void);

// Print hardware info to VGA
void hardware_print_info(void);
```

**Global variable:**
```c
extern hardware_info_t hw_info;
```

---

## Network Drivers

### RTL8139

```c
// Detect and initialize RTL8139
void rtl8139_detect(pci_device_t *pci);

// Transmit packet
int rtl8139_transmit(const u8 *data, u16 len);

// Receive packet (returns 1 if packet available)
int rtl8139_receive(net_packet_t *packet);

// IRQ handler (called from interrupt)
void rtl8139_irq_handler(void);

// Print driver status
void rtl8139_print_status(void);
```

### Intel E1000/E1000E

```c
// Detect and initialize E1000
void e1000_detect(pci_device_t *pci);

// Transmit packet
int e1000_transmit(const u8 *data, u16 len);

// Receive packet
int e1000_receive(u8 *buf, u16 *len);

// IRQ handler
void e1000_irq_handler(void);

// Print driver status
void e1000_print_status(void);
```

### Network Packet Structure

```c
typedef struct {
    vec4_t semantic_class;  // 4D vector classification
    u32 length;
    u8  data[1518];
    u8  protocol;
    u8  priority;
} net_packet_t;
```

---

## Input Devices

### PS/2 Mouse

```c
// Initialize mouse
int mouse_init(void);

// IRQ handler (called from IRQ12)
void mouse_irq_handler(u8 scancode);

// Get current position
vec2_t mouse_get_position(void);

// Get button states
u8 mouse_get_buttons(void);

// Check for click event
u8 mouse_was_clicked(void);

// Check for release event
u8 mouse_was_released(void);

// Print mouse status
void mouse_print_status(void);
```

**Button masks:**
```c
#define MOUSE_LEFT_BTN   0x01
#define MOUSE_RIGHT_BTN  0x02
#define MOUSE_MIDDLE_BTN 0x04
```

### PS/2 Keyboard

```c
// Initialize keyboard (called from kernel_main)
void keyboard_init(void);

// Get character (non-blocking, returns -1 if no key)
int keyboard_getchar(void);

// Read character (blocking)
char keyboard_read(void);

// IRQ handler (called from IRQ1)
void keyboard_handler(u8 scancode);
```

---

## Neural Network

### Neural Hardware Optimizer

```c
// Initialize neural network
void neural_hw_init(void);

// Get prediction for hardware type
float neural_hw_predict(u32 hw_type);

// Train network with observation
void neural_hw_learn(u32 hw_type, float performance);

// Run optimization cycle
void neural_hw_optimize(void);
```

**Hardware types for prediction:**
```c
#define HW_CPU_LOAD       0
#define HW_MEMORY_USAGE   1
#define HW_DISK_ACTIVITY  2
#define HW_NETWORK_IO     3
```

**Example:**
```c
// Initialize
neural_hw_init();

// Predict optimal CPU frequency
float cpu_prediction = neural_hw_predict(HW_CPU_LOAD);
// cpu_prediction is 0.0-1.0 representing optimal utilization

// Record observed performance
neural_hw_learn(HW_CPU_LOAD, actual_performance);

// Run full optimization
neural_hw_optimize();
```

---

## System Calls

### Available Syscalls

```c
#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_READ    2
#define SYS_OPEN    3
#define SYS_CLOSE   4
#define SYS_FORK    5
#define SYS_EXEC    6
#define SYS_PUTS    7
#define SYS_GETC    8
#define SYS_MALLOC  9
#define SYS_FREE    10
#define SYS_SLEEP   11
#define SYS_PID     12
#define SYS_VEC_OP  13
```

### Invoke Syscall

```c
// From userspace:
asm volatile (
    "mov %1, %%eax\n"
    "mov %2, %%ebx\n"
    "int $0x80\n"
    : "=a"(result)
    : "r"(syscall_num), "r"(arg)
);
```

---

## Interrupts

### ISR Numbers (0-31)

| Vector | Exception |
|--------|-----------|
| 0 | Division By Zero |
| 1 | Debug |
| 2 | Non Maskable Interrupt |
| 3 | Breakpoint |
| 4 | Overflow |
| 5 | Bound Range Exceeded |
| 6 | Invalid Opcode |
| 7 | Device Not Available |
| 8 | Double Fault |
| 9 | Coprocessor Segment Overrun |
| 10 | Invalid TSS |
| 11 | Segment Not Present |
| 12 | Stack-Segment Fault |
| 13 | General Protection Fault |
| 14 | Page Fault |
| 15 | Reserved |
| 16 | x87 Floating-Point |
| 17 | Alignment Check |
| 18 | Machine Check |
| 19 | SIMD Floating-Point |

### IRQ Numbers (32-47)

| IRQ | Device |
|-----|--------|
| 0 | PIT Timer |
| 1 | Keyboard |
| 12 | Mouse |
| 14 | Primary ATA |
| 15 | Secondary ATA |

---

## Serial Debug Output

```c
// Initialize serial port (COM1, 38400 baud)
void serial_init(void);

// Write character
void serial_putc(char c);

// Write string
void serial_puts(const char *str);

// Write hexadecimal value
void serial_puthex(u32 value);

// Write decimal value
void serial_putdec(u32 value);
```

---

## Constants

### Kernel Version

```c
#define VECTORD_VERSION "2.0.0"
```

### VGA Constants

```c
#define VGA_BUFFER  0xB8000
#define VGA_WIDTH   80
#define VGA_HEIGHT  25
```

### Memory Constants

```c
#define PAGE_SIZE     4096
#define HEAP_DEFAULT  (4 * 1024 * 1024)  // 4MB
```

### Process Constants

```c
#define MAX_PROCESSES 64
#define DEFAULT_QUANTUM 100  // Timer ticks
```

---

*API Reference for VECTORD OS v2.0.0*
