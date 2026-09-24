/* ═══════════════════════════════════════════════════════════════════════
 * ULTRA VECTROVIAN OS - Standalone Kernel Header
 * Vector Semantic Operating System - NOT based on Linux/Unix
 * ═══════════════════════════════════════════════════════════════════════ */

#ifndef VECTORD_H
#define VECTORD_H

/* OS Identity */
#define OS_NAME         "VECTORD OS"
#define OS_VERSION      "2026.1.0"
#define OS_CODENAME     "The Thinking OS"

/* No standard library - define everything ourselves */
typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;
typedef signed int          i32;
typedef signed long long    i64;
typedef u32                 size_t;

/* ═══════════════════════════════════════════════════════════════════════
 * TYPE DEFINITIONS
 * ═══════════════════════════════════════════════════════════════════════ */

#define bool  int
#define true  1
#define false 0
#define NULL  ((void*)0)

/* ═══════════════════════════════════════════════════════════════════════
 * PCI BUS DRIVER
 * ═══════════════════════════════════════════════════════════════════════ */

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

#define PCI_MAX_DEVICES 64
extern pci_device_t pci_devices[PCI_MAX_DEVICES];
extern int pci_device_count;

/* ═══════════════════════════════════════════════════════════════════════
 * ATA/IDE DISK DRIVER
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    u16 base_port;      /* Base I/O port */
    u16 ctrl_port;      /* Control port */
    u8  slave;          /* 0=master, 1=slave */
    u8  present;
    u32 sectors;        /* Total sectors */
    char model[41];     /* Model string */
    char serial[21];    /* Serial number */
} ata_device_t;

#define ATA_MAX_DEVICES 4
extern ata_device_t ata_devices[ATA_MAX_DEVICES];
extern int ata_device_count;

/* ═══════════════════════════════════════════════════════════════════════
 * AHCI/SATA DRIVER
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    u32 base;           /* HBA memory base address */
    u32 ports_implemented;
    u8  irq;
    u8  present;
    int port_count;
} ahci_controller_t;

#define AHCI_MAX_CONTROLLERS 4
extern ahci_controller_t ahci_controllers[AHCI_MAX_CONTROLLERS];
extern int ahci_controller_count;

/* ═══════════════════════════════════════════════════════════════════════
 * USB CONTROLLER
 * ═══════════════════════════════════════════════════════════════════════ */

typedef enum {
    USB_UHCI = 0,
    USB_OHCI,
    USB_EHCI,
    USB_XHCI
} usb_type_t;

typedef struct {
    usb_type_t type;
    u32 base;
    u8  irq;
    u8  present;
    char type_name[8];
} usb_controller_t;

#define USB_MAX_CONTROLLERS 8
extern usb_controller_t usb_controllers[USB_MAX_CONTROLLERS];
extern int usb_controller_count;

/* ═══════════════════════════════════════════════════════════════════════
 * VESA/VBE FRAMEBUFFER
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    u32 base;           /* Framebuffer physical address */
    u32 width;
    u32 height;
    u32 pitch;          /* Bytes per scanline */
    u8  bpp;            /* Bits per pixel */
    u8  present;
    u32 size;           /* Total framebuffer size */
} vbe_mode_info_t;

extern vbe_mode_info_t vbe_info;

/* ═══════════════════════════════════════════════════════════════════════
 * HARDWARE INFO
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* CPU Info */
    char cpu_vendor[13];
    char cpu_brand[49];
    u32 cpu_family;
    u32 cpu_model;
    u32 cpu_stepping;
    u32 cpu_speed_mhz;
    
    /* Memory */
    u32 ram_total_mb;
    
    /* BIOS Info */
    char bios_vendor[65];
    char bios_version[65];
    char bios_date[65];
    
    /* motherboard */
    char mb_vendor[65];
    char mb_product[65];
    char mb_version[65];
} hardware_info_t;

extern hardware_info_t hw_info;

/* ═══════════════════════════════════════════════════════════════════════
 * VECTOR TYPES - Core of VECTORD philosophy
 * ═══════════════════════════════════════════════════════════════════════ */

typedef union {
    struct { float x, y; };
    float e[2];
} vec2_t;

typedef union {
    struct { float x, y, z; };
    float e[3];
} vec3_t;

typedef union {
    struct { float x, y, z, w; };
    float e[4];
} vec4_t;

/* 4D Semantic Address - replaces linear pointers */
typedef struct {
    vec4_t position;      /* x=page, y=frame, z=segment, w=class */
    u32 epoch;            /* Generation counter */
    u32 flags;            /* Permission bits */
} vec_addr_t;

/* ═══════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════ */

#define VECTORD_VERSION "2.0.0"
#define PI 3.14159265358979f

#define VEC2_ZERO ((vec2_t){{0, 0}})
#define VEC3_ZERO ((vec3_t){{0, 0, 0}})
#define VEC4_ZERO ((vec4_t){{0, 0, 0, 0}})
#define VEC4_ONE  ((vec4_t){{1, 1, 1, 1}})

/* ═══════════════════════════════════════════════════════════════════════
 * GDT STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════ */

struct gdt_entry {
    u16 limit_low;
    u16 base_low;
    u8  base_middle;
    u8  access;
    u8  granularity;
    u8  base_high;
} __attribute__((packed));

struct gdt_ptr {
    u16 limit;
    u32 base;
} __attribute__((packed));

/* ═══════════════════════════════════════════════════════════════════════
 * IDT STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════ */

struct idt_entry {
    u16 base_low;
    u16 sel;
    u8  always0;
    u8  flags;
    u16 base_high;
} __attribute__((packed));

struct idt_ptr {
    u16 limit;
    u32 base;
} __attribute__((packed));

/* Interrupt frame */
struct interrupt_frame {
    /* Matches kernel/boot.asm isr_common_stub / irq_common_stub push order */
    u32 gs, fs, es, ds;
    u32 edi, esi, ebp, esp, ebx, edx, ecx, eax;
    u32 int_no, err_code;
    u32 eip, cs, eflags;
    u32 useresp, ss;
};

/* ═══════════════════════════════════════════════════════════════════════
 * PROCESS CONTROL BLOCK - Geometric process representation
 * ═══════════════════════════════════════════════════════════════════════ */

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
    
    /* Geometric properties */
    vec4_t position;       /* Position in 4D memory space */
    vec4_t velocity;       /* Execution velocity */
    vec4_t momentum;       /* Priority momentum */
    float rotation;        /* Rotation angle */
    
    /* CPU state */
    u32 esp;
    u32 ebp;
    u32 eip;
    u32 eax, ebx, ecx, edx;
    u32 esi, edi;
    u32 eflags;
    
    /* Memory */
    vec_addr_t code_addr;
    vec_addr_t stack_addr;
    u32 stack_base;
    u32 stack_size;
    
    /* State */
    proc_state_t state;
    u32 quantum;           /* Time slice */
    u32 cpu_time;          /* Total CPU time used */
    
    /* Navigation */
    struct process *next;
    struct process *prev;
} process_t;

/* ═══════════════════════════════════════════════════════════════════════
 * MEMORY BLOCK
 * ═══════════════════════════════════════════════════════════════════════ */

typedef enum {
    MEM_KERNEL = 0,
    MEM_USER,
    MEM_VIDEO,
    MEM_BUFFER,
    MEM_SHARED
} mem_type_t;

typedef struct mem_block {
    vec_addr_t addr;
    u32 size;
    u32 phys_addr;
    mem_type_t type;
    struct mem_block *next;
} mem_block_t;

/* ═══════════════════════════════════════════════════════════════════════
 * VGA TEXT MODE
 * ═══════════════════════════════════════════════════════════════════════ */

#define VGA_BUFFER  0xB8000
#define VGA_WIDTH   80
#define VGA_HEIGHT  25

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

/* ═══════════════════════════════════════════════════════════════════════
 * KERNEL API
 * ═══════════════════════════════════════════════════════════════════════ */

/* Port I/O */
void outb(u16 port, u8 val);
u8 inb(u16 port);
void outw(u16 port, u16 val);
u16 inw(u16 port);
void outl(u16 port, u32 val);
u32 inl(u16 port);
void io_wait(void);

/* Serial debug output */
void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *str);
void serial_puthex(u32 value);
void serial_putdec(u32 value);
int serial_getchar(void);

/* VGA */
void vga_init(void);
void vga_clear(u8 color);
void vga_putchar(int x, int y, char c, u8 color);
void vga_print(int x, int y, const char *str, u8 color);
void vga_print_colored(int x, int y, const char *str, u8 fg, u8 bg);
void vga_put_hex(int x, int y, u32 value, u8 color);
void vga_put_dec(int x, int y, u32 value, u8 color);
void vga_set_cursor(int x, int y);

/* GDT */
void gdt_init(void);

/* IDT */
void idt_init(void);
void idt_set_gate(u8 num, u32 base, u16 sel, u8 flags);

/* Interrupt handlers */
void isr_handler(struct interrupt_frame *frame);
void irq_handler(struct interrupt_frame *frame);

/* Memory */
void mem_init(u32 mem_size);
void *kmalloc(u32 size);
void kfree(void *ptr);
u32 mem_get_free(void);
u32 mem_get_used(void);
u32 mem_get_total(void);
u32 mem_get_free_blocks(void);

/* Vector memory operations */
vec_addr_t vec_alloc(u32 size, u32 semantic_class);
void vec_free(vec_addr_t addr);
void *vec_resolve(vec_addr_t addr);
u32 vec_get_counter(void);

/* Process */
void proc_init(void);
process_t *proc_create(const char *name, void (*entry)(void));
void proc_destroy(process_t *proc);
void proc_schedule(void);
void proc_yield(void);
process_t *proc_get_current(void);

/* Vector math */
vec2_t v2_add(vec2_t a, vec2_t b);
vec2_t v2_sub(vec2_t a, vec2_t b);
vec2_t v2_scale(vec2_t v, float s);
float v2_dot(vec2_t a, vec2_t b);
float v2_length(vec2_t v);
vec2_t v2_normalize(vec2_t v);

vec3_t v3_add(vec3_t a, vec3_t b);
vec3_t v3_cross(vec3_t a, vec3_t b);
float v3_dot(vec3_t a, vec3_t b);
float v3_length(vec3_t v);

vec4_t v4_add(vec4_t a, vec4_t b);
vec4_t v4_sub(vec4_t a, vec4_t b);
vec4_t v4_scale(vec4_t v, float s);
float v4_dot(vec4_t a, vec4_t b);
vec4_t v4_lerp(vec4_t a, vec4_t b, float t);

/* SSE2-accelerated vector math (vec_sse.c) */
/* Single-entry SSE2 */
float v3_dot_sse(vec3_t a, vec3_t b);
float v3_length_sse(vec3_t v);
vec4_t v4_add_sse(vec4_t a, vec4_t b);
vec4_t v4_sub_sse(vec4_t a, vec4_t b);
vec4_t v4_scale_sse(vec4_t v, float s);
float v4_dot_sse(vec4_t a, vec4_t b);
vec4_t v4_lerp_sse(vec4_t a, vec4_t b, float t);
/* Batch operations (process 4 vectors at a time via SSE2) */
void v2_dot_batch(const vec2_t *a, const vec2_t *b, float *out, int n);
void v2_length_batch(const vec2_t *v, float *out, int n);
void v3_dot_batch(const vec3_t *a, const vec3_t *b, float *out, int n);
void v3_length_batch(const vec3_t *v, float *out, int n);
void v4_dot_batch(const vec4_t *a, const vec4_t *b, float *out, int n);
void v4_length_batch(const vec4_t *v, float *out, int n);

/* Hardware drivers */
void pci_init(void);
void pci_scan_bus(void);
u32 pci_config_read(u8 bus, u8 slot, u8 func, u8 offset);
void ata_init(void);
void ata_detect(u16 base, u16 ctrl, u8 slave);
int ata_read_sectors(ata_device_t *dev, u32 lba, u8 count, u16 *buffer);
int ata_write_sectors(ata_device_t *dev, u32 lba, u8 count, const u16 *buffer);
void ahci_init(void);
void ahci_detect(u32 base, u8 irq);
void usb_init(void);
void usb_detect_controller(u16 vendor_id, u16 device_id, u8 prog_if, u32 bar0, u8 irq);
void vbe_init(void);
void vbe_test_pattern(void);
u16 vbe_read(u16 index);
void vga_show_status(void);
void hardware_detect(void);
void hardware_print_info(void);
void rtl8139_detect(pci_device_t *pci);
void e1000_detect(pci_device_t *pci);
int mouse_init(void);
vec2_t mouse_get_position(void);
u8 mouse_get_buttons(void);
int keyboard_getchar(void);  /* kernel.c */
void keyboard_init(void);    /* kernel.c */
u8 mouse_was_clicked(void);
void mouse_print_status(void);



/* Neural hardware optimizer */
void neural_hw_init(void);
void neural_hw_optimize(void);
float neural_hw_predict(u32 hw_type);
void neural_hw_learn(u32 hw_type, float performance);

/* RTC */
void rtc_init(void);
void rtc_print_time(void);

/* RNG */
void rng_init(void);
u32 random_u32(void);
u16 random_u16(void);
u8 random_u8(void);
u32 random_range(u32 max);
float random_float(void);
void random_fill(u8 *buf, u32 len);

/* Kernel main */
void kernel_main(u32 magic, u32 mboot_addr) __attribute__((noreturn));

/* Panic */
void panic(const char *msg);

/* ═══════════════════════════════════════════════════════════════════════
 * FILESYSTEM TYPES AND STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════ */

/* Filesystem type identifiers */
#define FS_TYPE_UNKNOWN     0
#define FS_TYPE_VECTORD     1
#define FS_TYPE_EXT2        2
#define FS_TYPE_FAT32       3

/* File types */
#define FILE_TYPE_REGULAR   0
#define FILE_TYPE_DIRECTORY 1
#define FILE_TYPE_SYMLINK   2
#define FILE_TYPE_DEVICE    3

/* File open flags */
#define O_RDONLY    0x00
#define O_WRONLY    0x01
#define O_RDWR      0x02
#define O_CREAT     0x04
#define O_TRUNC     0x08
#define O_APPEND    0x10

/* Seek whence */
#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

/* Maximum path length */
#define MAX_PATH    256

/* Maximum filename length */
#define MAX_NAME    255

/* Maximum open files */
#define MAX_OPEN_FILES  64

/* Maximum mounted filesystems */
#define MAX_MOUNTS   8

/* Maximum filesystem drivers */
#define MAX_FS_DRIVERS  8

/* Forward declarations */
struct vfs_node;
struct vfs_mount;
struct fs_driver;

/* File stat structure */
typedef struct {
    u32 dev_id;         /* Device ID */
    u32 inode;          /* Inode number */
    u32 mode;           /* File mode and permissions */
    u32 uid;            /* Owner user ID */
    u32 gid;            /* Owner group ID */
    u32 size;           /* File size in bytes */
    u32 blocks;         /* Number of blocks */
    u32 atime;          /* Last access time */
    u32 mtime;          /* Last modification time */
    u32 ctime;          /* Creation time */
    u8  type;           /* File type */
} stat_t;

/* Directory entry structure */
typedef struct {
    u32 inode;
    u32 type;
    char name[MAX_NAME + 1];
} dirent_t;

/* VFS node structure */
typedef struct vfs_node {
    char name[MAX_NAME + 1];
    u8   type;              /* File type */
    u32  size;              /* File size */
    u32  inode;             /* Inode number */
    u32  mode;              /* Permissions */
    u32  uid;               /* Owner */
    u32  gid;               /* Group */
    u32  atime;             /* Access time */
    u32  mtime;             /* Modify time */
    u32  ctime;             /* Create time */
    
    /* Mounted filesystem */
    struct vfs_mount *mount;
    
    /* Internal data for filesystem driver */
    void *fs_data;
    
    /* Directory linkage */
    struct vfs_node *parent;
    struct vfs_node *first_child;
    struct vfs_node *next_sibling;
    
    /* Reference count */
    u32 refcount;
} vfs_node_t;

/* File handle for open files */
typedef struct {
    vfs_node_t *node;
    u32  offset;            /* Current read/write offset */
    u32  flags;             /* Open flags */
    u32  refcount;          /* Reference count */
} file_handle_t;

/* Mount point structure */
typedef struct vfs_mount {
    char path[MAX_PATH];
    vfs_node_t *root;
    struct fs_driver *driver;
    void *fs_private;       /* Filesystem-specific data */
    u32  dev_id;            /* Device ID */
    u8   mounted;
} vfs_mount_t;

/* Filesystem driver interface */
typedef struct fs_driver {
    char name[16];
    u32  type;
    
    /* Mount/unmount */
    int (*mount)(vfs_mount_t *mount, u32 dev_id);
    int (*unmount)(vfs_mount_t *mount);
    
    /* Node operations */
    int (*read_node)(vfs_mount_t *mount, u32 inode, vfs_node_t *node);
    int (*write_node)(vfs_mount_t *mount, vfs_node_t *node);
    int (*create_node)(vfs_mount_t *mount, vfs_node_t *parent, const char *name, u8 type, vfs_node_t **result);
    int (*delete_node)(vfs_mount_t *mount, vfs_node_t *node);
    
    /* File operations */
    int (*read)(vfs_mount_t *mount, vfs_node_t *node, u32 offset, u32 size, void *buffer);
    int (*write)(vfs_mount_t *mount, vfs_node_t *node, u32 offset, u32 size, const void *buffer);
    int (*truncate)(vfs_mount_t *mount, vfs_node_t *node, u32 size);
    
    /* Directory operations */
    int (*read_dir)(vfs_mount_t *mount, vfs_node_t *node, u32 index, dirent_t *entry);
    int (*lookup)(vfs_mount_t *mount, vfs_node_t *parent, const char *name, vfs_node_t **result);
} fs_driver_t;

/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD FS - Custom Filesystem Constants
 * ═══════════════════════════════════════════════════════════════════════ */

#define VECTORD_FS_MAGIC    0x56454354  /* "VECT" */
#define VECTORD_BLOCK_SIZE  4096
#define VECTORD_INODE_SIZE  64
#define VECTORD_INODES_PER_BLOCK (VECTORD_BLOCK_SIZE / VECTORD_INODE_SIZE)

/* VECTORD FS Inode flags */
#define VECTORD_INODE_USED      0x0001
#define VECTORD_INODE_DIRECTORY 0x0002
#define VECTORD_INODE_SYMLINK   0x0004
#define VECTORD_INODE_DEVICE    0x0008

/* Direct/indirect block addressing */
#define VECTORD_DIRECT_BLOCKS   12
#define VECTORD_INDIRECT_BLOCK  12      /* Single indirect */
#define VECTORD_DINDIRECT_BLOCK 13      /* Double indirect */
#define VECTORD_TINDIRECT_BLOCK 14      /* Triple indirect */

/* ═══════════════════════════════════════════════════════════════════════
 * VFS API FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════ */

/* VFS initialization */
void vfs_init(void);
int vfs_register_fs(fs_driver_t *driver);
int vfs_mount_fs(const char *path, u32 dev_id, u32 fs_type);
int vfs_unmount(const char *path);

/* File operations */
file_handle_t *vfs_open(const char *path, u32 flags);
int vfs_close(file_handle_t *handle);
int vfs_read(file_handle_t *handle, void *buffer, u32 size);
int vfs_write(file_handle_t *handle, const void *buffer, u32 size);
int vfs_seek(file_handle_t *handle, u32 offset, u32 whence);
int vfs_stat(const char *path, stat_t *stat);
int vfs_fstat(file_handle_t *handle, stat_t *stat);

/* Directory operations */
int vfs_mkdir(const char *path);
int vfs_rmdir(const char *path);
int vfs_ls(const char *path, dirent_t *entries, u32 max_entries);

/* Path operations */
vfs_node_t *vfs_resolve_path(const char *path);

/* ═══════════════════════════════════════════════════════════════════════
 * EXT2 FILESYSTEM
 * ═══════════════════════════════════════════════════════════════════════ */

#define EXT2_SUPERBLOCK_MAGIC   0xEF53
#define EXT2_BLOCK_SIZE_1024    0
#define EXT2_BLOCK_SIZE_2048    1
#define EXT2_BLOCK_SIZE_4096    2

/* ext2 inode types */
#define EXT2_S_IFSOCK   0xC000
#define EXT2_S_IFLNK    0xA000
#define EXT2_S_IFREG    0x8000
#define EXT2_S_IFBLK    0x6000
#define EXT2_S_IFDIR    0x4000
#define EXT2_S_IFCHR    0x2000
#define EXT2_S_IFIFO    0x1000

/* ext2 driver initialization */
void ext2_init(void);
int ext2_mount(vfs_mount_t *mount, u32 dev_id);
int ext2_unmount(vfs_mount_t *mount);

/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD FS FILESYSTEM
 * ═══════════════════════════════════════════════════════════════════════ */

/* VECTORD FS driver initialization and operations */
void vectord_fs_init(void);
int vectord_fs_mount(vfs_mount_t *mount, u32 dev_id);
int vectord_fs_unmount(vfs_mount_t *mount);
int vectord_fs_format(ata_device_t *device, const char *label);

/* ═══════════════════════════════════════════════════════════════════════
 * SYSCALL NUMBERS
 * ═══════════════════════════════════════════════════════════════════════ */

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
#define SYS_VEC_OP  13  /* Vector math syscall */
#define SYS_MOUNT   14
#define SYS_STAT    15
#define SYS_MKDIR   16
#define SYS_LS      17
#define SYS_WAIT    18
#define SYS_GETCWD  19
#define SYS_CHDIR   20
#define SYS_RMDIR   21
#define SYS_UNLINK  22
#define SYS_LSEEK   23
#define SYS_DUP     24
#define SYS_PIPE    25
#define SYS_SIGNAL  26
#define SYS_KILL    27
#define SYS_GETPID  28
#define SYS_GETUID  29
#define SYS_SETUID  30
#define SYS_TIME    31
#define SYS_BRK     32

/* ═══════════════════════════════════════════════════════════════════════
 * ELF32 BINARY FORMAT
 * ═══════════════════════════════════════════════════════════════════════ */

/* ELF header */
typedef struct {
    u8  e_ident[16];    /* ELF identification */
    u16 e_type;         /* Object file type */
    u16 e_machine;      /* Machine type */
    u32 e_version;      /* Object file version */
    u32 e_entry;        /* Entry point address */
    u32 e_phoff;        /* Program header offset */
    u32 e_shoff;        /* Section header offset */
    u32 e_flags;        /* Processor-specific flags */
    u16 e_ehsize;       /* ELF header size */
    u16 e_phentsize;    /* Program header entry size */
    u16 e_phnum;        /* Program header entry count */
    u16 e_shentsize;    /* Section header entry size */
    u16 e_shnum;        /* Section header entry count */
    u16 e_shstrndx;     /* Section header string table index */
} elf32_header_t;

/* Program header */
typedef struct {
    u32 p_type;         /* Segment type */
    u32 p_offset;       /* Segment offset */
    u32 p_vaddr;        /* Virtual address */
    u32 p_paddr;        /* Physical address */
    u32 p_filesz;       /* Segment size in file */
    u32 p_memsz;        /* Segment size in memory */
    u32 p_flags;        /* Segment flags */
    u32 p_align;        /* Alignment */
} elf32_phdr_t;

/* Section header */
typedef struct {
    u32 sh_name;        /* Section name */
    u32 sh_type;        /* Section type */
    u32 sh_flags;       /* Section flags */
    u32 sh_addr;        /* Virtual address */
    u32 sh_offset;      /* Offset in file */
    u32 sh_size;        /* Section size */
    u32 sh_link;        /* Link to other section */
    u32 sh_info;        /* Additional info */
    u32 sh_addralign;   /* Alignment */
    u32 sh_entsize;     /* Entry size if section has table */
} elf32_shdr_t;

/* ELF constants */
#define ELF_MAG0 0x7F
#define ELF_MAG1 'E'
#define ELF_MAG2 'L'
#define ELF_MAG3 'F'

#define ELF_CLASS32 1
#define ELF_DATA2LSB 1

#define ELF_TYPE_EXEC 2
#define ELF_TYPE_DYN  3
#define ELF_TYPE_REL  1
#define ELF_TYPE_CORE 4

#define ELF_MACHINE_386 3

#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5
#define PT_PHDR 6

#define PF_R 4
#define PF_W 2
#define PF_X 1

#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_HASH 5
#define SHT_DYNAMIC 6
#define SHT_NOTE 7
#define SHT_NOBITS 8
#define SHT_REL 9
#define SHT_SHLIB 10
#define SHT_DYNSYM 11

/* ELF loader API */
int elf_load(const u8 *buffer, u32 size, process_t *proc);
void elf_jump_to_entry(u32 entry, u32 user_stack);

/* ═══════════════════════════════════════════════════════════════════════
 * INIT SYSTEM (PID 1)
 * ═══════════════════════════════════════════════════════════════════════ */

/* Init process */
void init_start(void);
void init_load_binary(const char *path);
void init_reap_children(void);
void init_signal_handler(int signum);
void init_spawn_shell(void);
void print_prompt_init(void);
const char *init_get_current_user(void);
const char *init_get_current_dir(void);
void init_set_current_dir(const char *dir);

/* Orphan process handling */
void init_adopt_orphans(void);
void init_wait_for_children(void);

/* ═══════════════════════════════════════════════════════════════════════
 * SYSTEM APPLICATIONS
 * ═══════════════════════════════════════════════════════════════════════ */

void app_calculator(void);
void app_clock(void);
void app_system_info(void);
void app_file_browser(void);
void app_settings(void);
void app_file_manager(void);
void app_task_manager(void);
void app_network_config(void);
void app_display_settings(void);
void app_kernel_info(void);
void app_memory_viewer(void);
void app_system_monitor(void);
void app_list(void);

/* ═══════════════════════════════════════════════════════════════════════
 * NETWORK TYPES
 * ═══════════════════════════════════════════════════════════════════════ */

/* Network address types */
typedef struct {
    u8 addr[6];
} mac_addr_t;

typedef struct {
    u8 addr[4];
} ip_addr_t;

/* Network packet structure */
typedef struct net_packet {
    mac_addr_t src_mac;
    mac_addr_t dst_mac;
    u16 ethertype;
    u8 *data;
    u16 length;
    struct net_packet *next;
} net_packet_t;

/* Network interface */
typedef struct {
    u8 index;
    mac_addr_t mac;
    ip_addr_t ip;
    ip_addr_t netmask;
    ip_addr_t gateway;
    u8 present;
} net_if_t;

/* Socket types */
typedef enum {
    SOCKET_UDP = 0,
    SOCKET_TCP,
    SOCKET_RAW
} socket_type_t;

typedef enum {
    SOCK_STATE_CLOSED = 0,
    SOCK_STATE_OPEN,
    SOCK_STATE_LISTEN,
    SOCK_STATE_CONNECTING,
    SOCK_STATE_CONNECTED,
    SOCK_STATE_CLOSING
} sock_state_t;

/* Socket structure */
typedef struct socket {
    socket_type_t type;
    sock_state_t state;
    ip_addr_t remote_ip;
    u16 remote_port;
    u16 local_port;
    void *internal;      /* Protocol-specific internal data */
    struct socket *next;
} socket_t;

/* ═══════════════════════════════════════════════════════════════════════
 * NETWORK API FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════ */

void net_init(void);
int net_add_interface(mac_addr_t mac, ip_addr_t ip, ip_addr_t netmask, ip_addr_t gateway);
net_if_t *net_get_interface(int index);
int net_send_packet(net_if_t *ifc, mac_addr_t dst, u16 ethertype, const u8 *data, u16 len);
int net_receive_packet(net_if_t *ifc, net_packet_t *pkt);
void net_process_packet(net_packet_t *pkt);

/* ARP */
void arp_init(void);
void arp_request(ip_addr_t ip);
void arp_reply(mac_addr_t src_mac, ip_addr_t src_ip, mac_addr_t dst_mac, ip_addr_t dst_ip);
int arp_resolve(ip_addr_t ip, mac_addr_t *mac);
void arp_table_add(ip_addr_t ip, mac_addr_t mac);
void arp_handle(net_packet_t *pkt);

/* IP */
void ip_init(void);
int ip_send(ip_addr_t dst, u8 protocol, const u8 *data, u16 len);
int ip_receive(net_packet_t *pkt);
int ip_addr_equal(ip_addr_t a, ip_addr_t b);
void ip_addr_print(ip_addr_t addr);
int ip_addr_parse(const char *str, ip_addr_t *addr);

/* ICMP */
void icmp_init(void);
void icmp_echo_request(ip_addr_t dst, u16 id, u16 seq);
void icmp_echo_reply(ip_addr_t src, u16 id, u16 seq);
void icmp_handle(net_packet_t *pkt);

/* UDP */
void udp_init(void);
int udp_send(ip_addr_t dst, u16 dst_port, u16 src_port, const u8 *data, u16 len);
int udp_receive(socket_t *sock, u8 *buf, u16 *len);
int udp_socket_create(u16 port);
int udp_socket_close(int sockfd);
void udp_process_packet(net_packet_t *pkt, ip_addr_t src);

/* TCP */
void tcp_init(void);
int tcp_connect(ip_addr_t dst, u16 dst_port);
int tcp_listen(u16 port);
int tcp_send(socket_t *sock, const u8 *data, u16 len);
int tcp_receive(socket_t *sock, u8 *buf, u16 *len);
void tcp_close(socket_t *sock);
void tcp_process_packet(net_packet_t *pkt, ip_addr_t src);

/* DHCP */
void dhcp_init(void);
void dhcp_discover(void);
void dhcp_handle_offer(net_packet_t *pkt);
void dhcp_handle_ack(net_packet_t *pkt);

/* Socket API */
int socket_create(socket_type_t type);
int socket_bind(int sockfd, u16 port);
int socket_listen(int sockfd);
int socket_accept(int sockfd);
int socket_connect(int sockfd, ip_addr_t dst, u16 port);
int socket_send(int sockfd, const u8 *data, u16 len);
int socket_receive(int sockfd, u8 *buf, u16 *len);
int socket_close(int sockfd);

/* Utility */
u16 htons(u16 val);
u16 ntohs(u16 val);
u32 htonl(u32 val);
u32 ntohl(u32 val);
u16 checksum(const u8 *data, u16 len);
void net_print_status(void);

/* Kernel main extension for network */
void network_main(void);

/* Network initialization */
void network_init(void);

/* Ethernet */
void ethernet_init(void);
int ethernet_parse(const u8 *raw, u16 len, void *frame);
int ethernet_build(const void *frame, u8 *raw, u16 *len);
void ethernet_print_frame(const void *frame);

/* Forward declaration for ethernet_frame_t */
typedef struct {
    mac_addr_t dst;
    mac_addr_t src;
    u16 ethertype;
    u8 data[1504];
} __attribute__((packed)) ethernet_frame_t;

/* ARP table print */
void arp_print_table(void);

/* Network shell commands */
void ifconfig(void);
void ipconfig(void);
void mac_addr_print(mac_addr_t addr);
void print_hex_dump(const u8 *data, u16 len);

/* Desktop GUI */
void desktop_init(void);
void desktop_draw_titlebar(void);
void desktop_draw_taskbar(void);
void desktop_draw_icon(int x, int y, const char* name);
void desktop_main_loop(void);
void desktop_clear_screen(u8 color);
void desktop_draw_box(int x, int y, int w, int h, u8 color);

#endif /* VECTORD_H */

/* Network enhancements */
void net_stats_print(void);
void ping(ip_addr_t ip, int count);
void netstat(void);
u32 dns_resolve(const char *hostname);
int http_get(const char *hostname, const char *path, char *buf, int buflen);

/* String functions (implemented in compat.c) */
int strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, int n);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, int n);
char *strstr(const char *haystack, const char *needle);
char *strchr(const char *s, int c);
int atoi(const char *s);
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

/* VGA Graphics Mode 13h */
void vga_gfx_set_mode13h(void);
void vga_gfx_set_palette(void);
void vga_gfx_clear(u8 color);
void vga_gfx_put_pixel(int x, int y, u8 color);
void vga_gfx_fill_rect(int x, int y, int w, int h, u8 color);
void vga_gfx_draw_rect(int x, int y, int w, int h, u8 color);
void vga_gfx_draw_line(int x1, int y1, int x2, int y2, u8 color);
void vga_gfx_draw_circle(int cx, int cy, int r, u8 color);
void vga_gfx_fill_circle(int cx, int cy, int r, u8 color);
void vga_gfx_draw_char(int x, int y, char c, u8 color);
void vga_gfx_draw_string(int x, int y, const char *str, u8 color);
void vga_gfx_demo_desktop(void);
void vga_gfx_demo_animation(void);

/* Screenshot functions */
void vga_screenshot_ppm(void);
void vga_screenshot_ascii(void);

/* ═══════════════════════════════════════════════════════════════════════
 * WEB SERVER (HTTPD)
 * ═══════════════════════════════════════════════════════════════════════ */

void httpd_init(void);
void httpd_start(void);
void httpd_stop(void);
void httpd_handler(void);
void httpd_status(void);
int httpd_is_running(void);
void cmd_webserver(const char *arg);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Vector AI (Akasha) Module
 * ═══════════════════════════════════════════════════════════════════════════════ */

void ai_init(void);
int ai_ingest_concept(const char *name, const char *text, u8 type);
int ai_query_concept(const char *query, int top_k);
void ai_stats(void);

/* Concept types */
#define AI_TYPE_MATHEMATICS  1
#define AI_TYPE_COMPUTER_SCI 2
#define AI_TYPE_CLIFFORD     3
#define AI_TYPE_PHYSICS      4
#define AI_TYPE_PHILOSOPHY   5

/* ═══════════════════════════════════════════════════════════════════════════════
 * Full HD Framebuffer (1920x1080)
 * ═══════════════════════════════════════════════════════════════════════════════ */

void fhd_init(void);
void fhd_fill_rect(int x, int y, int w, int h, u32 color);
void fhd_fill_rounded_rect(int x, int y, int w, int h, int radius, u32 color);
void fhd_draw_text(int x, int y, const char *text, u32 color);
void fhd_draw_wallpaper(void);
void fhd_flip(void);
void fhd_clear(void);
void fhd_render_desktop(void);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Hyprland-style Compositor
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct window window_t;

void compositor_init(void);
void compositor_demo(void);
window_t *window_create(const char *title, int x, int y, int w, int h);
void window_destroy(window_t *win);
void window_move(window_t *win, int new_x, int new_y);
void window_resize(window_t *win, int new_w, int new_h);
void window_maximize(window_t *win);
void window_minimize(window_t *win);
void window_close(window_t *win);
void window_set_active(window_t *win);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Premium HD Desktop Environment
 * ═══════════════════════════════════════════════════════════════════════════════ */

void hd_desktop_init(void);
void hd_desktop_demo(void);
void hd_flush_to_vbe(void);
void hd_dump_serial(const char *label);

/* Graphics engine (graphics.c) */
void gfx_init(void);
void gfx_desktop_launch(void);
void gfx_desktop_tick(void);
void gfx_flush(void);
void gfx_run(void);              /* Interactive GUI event loop */
void gfx_installer(void);        /* Disk installer */
void gfx_set_mouse_pos(int x, int y);
int  gfx_get_mouse_x(void);
int  gfx_get_mouse_y(void);
void gfx_set_mouse_btn(int b);
int  gfx_get_mouse_btn(void);
void gfx_handle_mouse_move(int x, int y);
void gfx_handle_mouse_click(int btn);
void gfx_handle_key(char c);
int  gfx_window_create(const char *title, int x, int y, int w, int h, u32 bg);
void gfx_window_focus(int wid);
void gfx_window_close(int wid);
void hd_desktop_render(void);
void hd_desktop_update(void);
void hd_desktop_keypress(char c);
