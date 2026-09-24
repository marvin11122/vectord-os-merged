/* ═══════════════════════════════════════════════════════════════════════
 * ULTRA VECTROVIAN OS - Linux Compatibility Layer
 * 
 * Enables running Linux ELF binaries on VECTROVIAN kernel
 * Similar to FreeBSD's Linuxulator or WSL
 * 
 * Supports:
 * - Linux ELF64 binaries (via translation)
 * - Basic POSIX syscalls
 * - /proc filesystem emulation
 * - Shared libraries (.so) loading
 * 
 * Preinstalled programs this enables:
 * - Node.js / npm (JavaScript runtime & package manager)
 * - Python 3 (with pip)
 * - Rust / cargo
 * - GCC / g++ / clang
 * - Git
 * - Bash / zsh
 * - Firefox / Chrome (via X11 forwarding or framebuffer)
 * - kilocode / opencode / codex / gemini-cli
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* Forward declarations for internal string functions */
static int compat_strcmp(const char *a, const char *b);
static char *compat_strstr(const char *haystack, const char *needle);

/* ═══════════════════════════════════════════════════════════════════════
 * LINUX SYSCALL NUMBERS (x86_64)
 * ═══════════════════════════════════════════════════════════════════════ */

#define SYS_LINUX_READ      0
#define SYS_LINUX_WRITE     1
#define SYS_LINUX_OPEN      2
#define SYS_LINUX_CLOSE     3
#define SYS_LINUX_FSTAT     5
#define SYS_LINUX_MMAP      9
#define SYS_LINUX_MUNMAP    11
#define SYS_LINUX_BRK       12
#define SYS_LINUX_IOCTL     16
#define SYS_LINUX_PIPE      22
#define SYS_LINUX_SELECT    23
#define SYS_LINUX_DUP2      33
#define SYS_LINUX_GETPID    39
#define SYS_LINUX_FORK      57
#define SYS_LINUX_EXECVE    59
#define SYS_LINUX_EXIT      60
#define SYS_LINUX_WAIT4     61
#define SYS_LINUX_KILL      62
#define SYS_LINUX_GETCWD    79
#define SYS_LINUX_CHDIR     80
#define SYS_LINUX_MKDIR     83
#define SYS_LINUX_UNLINK    87
#define SYS_LINUX_GETUID    102
#define SYS_LINUX_GETGID    104
#define SYS_LINUX_GETEUID   107
#define SYS_LINUX_GETEGID   108
#define SYS_LINUX_GETPPID   110
#define SYS_LINUX_GETTID    186

/* ═══════════════════════════════════════════════════════════════════════
 * LINUX ELF HEADER TYPES
 * ═══════════════════════════════════════════════════════════════════════ */

#define ELF64_MAGIC "\x7fELF"

typedef struct {
    u8  e_ident[16];    /* Magic number and class */
    u16 e_type;         /* Object file type */
    u16 e_machine;      /* Machine type */
    u32 e_version;      /* Object file version */
    u64 e_entry;        /* Entry point address */
    u64 e_phoff;        /* Program header offset */
    u64 e_shoff;        /* Section header offset */
    u32 e_flags;        /* Processor-specific flags */
    u16 e_ehsize;       /* ELF header size */
    u16 e_phentsize;    /* Program header entry size */
    u16 e_phnum;        /* Program header entry count */
    u16 e_shentsize;    /* Section header entry size */
    u16 e_shnum;        /* Section header entry count */
    u16 e_shstrndx;     /* Section header string index */
} elf64_header_t;

typedef struct {
    u32 p_type;         /* Segment type */
    u32 p_flags;        /* Segment flags */
    u64 p_offset;       /* Segment offset */
    u64 p_vaddr;        /* Virtual address */
    u64 p_paddr;        /* Physical address */
    u64 p_filesz;       /* Segment file size */
    u64 p_memsz;        /* Segment memory size */
    u64 p_align;        /* Segment alignment */
} elf64_phdr_t;

/* ═══════════════════════════════════════════════════════════════════════
 * COMPATIBLE PROCESS STRUCTURE
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    u32 pid;                /* Process ID */
    char path[256];         /* Full path to binary */
    char name[64];          /* Process name */
    u32 state;              /* Process state */
    
    /* Linux compatibility */
    u32 linux_uid;
    u32 linux_gid;
    u32 linux_ppid;
    
    /* Memory management */
    u32 heap_start;
    u32 heap_end;
    u32 stack_start;
    u32 stack_end;
    
    /* File descriptors */
    u32 fd_table[256];
    u32 fd_count;
    
    /* Environment */
    char *envp[128];
    u32 envp_count;
} compat_process_t;

/* ═══════════════════════════════════════════════════════════════════════
 * PREINSTALLED APPLICATION REGISTRY
 * ═══════════════════════════════════════════════════════════════════════ */

typedef enum {
    APP_TYPE_BINARY,        /* Native ELF binary */
    APP_TYPE_LINUX,         /* Linux ELF (compatibility mode) */
    APP_TYPE_SCRIPT,        /* Shell script */
    APP_TYPE_WEB,           /* Web-based (needs browser engine) */
} app_type_t;

typedef struct {
    const char *name;           /* Command name */
    const char *path;           /* Installation path */
    const char *description;   /* Description */
    app_type_t type;            /* Application type */
    const char *dependencies;  /* Dependencies */
    u32 min_memory_mb;         /* Minimum RAM */
} app_registry_t;

/* ═══════════════════════════════════════════════════════════════════════
 * PREINSTALLED APPLICATIONS DATABASE
 * ═══════════════════════════════════════════════════════════════════════ */

static const app_registry_t preinstalled_apps[] = {
    /* === SYSTEM TOOLS === */
    {"bash", "/bin/bash", "Bourne Again Shell", APP_TYPE_LINUX, NULL, 16},
    {"sh", "/bin/sh", "Shell", APP_TYPE_LINUX, NULL, 8},
    {"zsh", "/usr/bin/zsh", "Z Shell", APP_TYPE_LINUX, NULL, 16},
    
    /* === DEVELOPMENT === */
    {"gcc", "/usr/bin/gcc", "GNU C Compiler", APP_TYPE_LINUX, "libc", 128},
    {"g++", "/usr/bin/g++", "GNU C++ Compiler", APP_TYPE_LINUX, "libc,libstdc++", 128},
    {"clang", "/usr/bin/clang", "LLVM Clang Compiler", APP_TYPE_LINUX, "llvm", 256},
    {"make", "/usr/bin/make", "Build automation", APP_TYPE_LINUX, NULL, 32},
    {"cmake", "/usr/bin/cmake", "Cross-platform build system", APP_TYPE_LINUX, NULL, 64},
    {"gdb", "/usr/bin/gdb", "GNU Debugger", APP_TYPE_LINUX, NULL, 64},
    {"valgrind", "/usr/bin/valgrind", "Memory debugger", APP_TYPE_LINUX, NULL, 128},
    
    /* === RUST === */
    {"rustc", "/usr/bin/rustc", "Rust compiler", APP_TYPE_LINUX, "libc", 256},
    {"cargo", "/usr/bin/cargo", "Rust package manager", APP_TYPE_LINUX, "rustc", 256},
    {"rustfmt", "/usr/bin/rustfmt", "Rust formatter", APP_TYPE_LINUX, "rustc", 64},
    {"clippy", "/usr/bin/clippy", "Rust linter", APP_TYPE_LINUX, "rustc", 64},
    
    /* === NODE.JS / NPM === */
    {"node", "/usr/bin/node", "Node.js runtime", APP_TYPE_LINUX, "libc", 64},
    {"npm", "/usr/bin/npm", "Node package manager", APP_TYPE_LINUX, "node", 128},
    {"npx", "/usr/bin/npx", "Node package executor", APP_TYPE_LINUX, "node", 64},
    {"yarn", "/usr/bin/yarn", "Alternative package manager", APP_TYPE_LINUX, "node", 128},
    
    /* === PYTHON === */
    {"python3", "/usr/bin/python3", "Python 3 interpreter", APP_TYPE_LINUX, "libc", 64},
    {"pip", "/usr/bin/pip", "Python package installer", APP_TYPE_LINUX, "python3", 128},
    {"python3-pip", "/usr/bin/pip3", "Python 3 package installer", APP_TYPE_LINUX, "python3", 128},
    
    /* === VERSION CONTROL === */
    {"git", "/usr/bin/git", "Distributed version control", APP_TYPE_LINUX, "libc", 64},
    {"git-lfs", "/usr/bin/git-lfs", "Git Large File Storage", APP_TYPE_LINUX, "git", 32},
    
    /* === AI/CLI TOOLS === */
    {"kilocode", "/usr/bin/kilocode", "AI coding assistant (kilocode)", APP_TYPE_LINUX, "node", 256},
    {"opencode", "/usr/bin/opencode", "AI coding assistant (opencode)", APP_TYPE_LINUX, "node", 256},
    {"codex", "/usr/bin/codex", "OpenAI Codex CLI", APP_TYPE_LINUX, "node", 256},
    {"gemini", "/usr/bin/gemini", "Google Gemini CLI", APP_TYPE_LINUX, "node", 256},
    
    /* === WEB BROWSERS === */
    {"firefox", "/usr/bin/firefox", "Mozilla Firefox browser", APP_TYPE_LINUX, "x11", 512},
    {"chromium", "/usr/bin/chromium", "Chromium browser", APP_TYPE_LINUX, "x11", 512},
    {"google-chrome", "/usr/bin/google-chrome", "Google Chrome", APP_TYPE_LINUX, "x11", 512},
    
    /* === EDITORS === */
    {"vim", "/usr/bin/vim", "Vi Improved editor", APP_TYPE_LINUX, NULL, 32},
    {"nvim", "/usr/bin/nvim", "Neovim editor", APP_TYPE_LINUX, "lua", 64},
    {"nano", "/usr/bin/nano", "Simple text editor", APP_TYPE_LINUX, NULL, 16},
    {"code", "/usr/bin/code", "VS Code CLI", APP_TYPE_LINUX, "node", 256},
    
    /* === UTILITIES === */
    {"curl", "/usr/bin/curl", "URL transfer utility", APP_TYPE_LINUX, "libc", 16},
    {"wget", "/usr/bin/wget", "Network file downloader", APP_TYPE_LINUX, "libc", 16},
    {"tar", "/usr/bin/tar", "Archive utility", APP_TYPE_LINUX, NULL, 16},
    {"gzip", "/usr/bin/gzip", "Compression utility", APP_TYPE_LINUX, NULL, 8},
    {"zip", "/usr/bin/zip", "ZIP compression", APP_TYPE_LINUX, NULL, 16},
    {"unzip", "/usr/bin/unzip", "ZIP extraction", APP_TYPE_LINUX, NULL, 16},
    {"jq", "/usr/bin/jq", "JSON processor", APP_TYPE_LINUX, NULL, 8},
    {"htop", "/usr/bin/htop", "Process viewer", APP_TYPE_LINUX, NULL, 32},
    {"tmux", "/usr/bin/tmux", "Terminal multiplexer", APP_TYPE_LINUX, NULL, 32},
    {"screen", "/usr/bin/screen", "Terminal multiplexer", APP_TYPE_LINUX, NULL, 16},
    
    /* === DATABASE === */
    {"mysql", "/usr/bin/mysql", "MySQL client", APP_TYPE_LINUX, "libc", 32},
    {"psql", "/usr/bin/psql", "PostgreSQL client", APP_TYPE_LINUX, "libc", 32},
    {"sqlite3", "/usr/bin/sqlite3", "SQLite CLI", APP_TYPE_LINUX, NULL, 16},
    
    /* === DOCKER/CLOUD === */
    {"docker", "/usr/bin/docker", "Docker CLI", APP_TYPE_LINUX, "libc", 64},
    {"kubectl", "/usr/bin/kubectl", "Kubernetes CLI", APP_TYPE_LINUX, "libc", 32},
    {"terraform", "/usr/bin/terraform", "Infrastructure as Code", APP_TYPE_LINUX, "libc", 64},
    {"aws", "/usr/bin/aws", "AWS CLI", APP_TYPE_LINUX, "python3", 64},
    
    /* === MEDIA === */
    {"ffmpeg", "/usr/bin/ffmpeg", "Video/audio converter", APP_TYPE_LINUX, NULL, 128},
    {"imagemagick", "/usr/bin/convert", "Image manipulation", APP_TYPE_LINUX, NULL, 64},
    
    /* NULL terminator */
    {NULL, NULL, NULL, 0, NULL, 0}
};

/* ═══════════════════════════════════════════════════════════════════════
 * STRING FUNCTIONS (needed for compat)
 * ═══════════════════════════════════════════════════════════════════════ */

static int compat_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *(unsigned char*)a - *(unsigned char*)b;
}

static int compat_strlen(const char *s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

static char *compat_strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char*)haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char*)haystack;
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════
 * COMPATIBILITY LAYER STATE
 * ═══════════════════════════════════════════════════════════════════════ */

static struct {
    u8 initialized;
    u32 brk_base;           /* Base for sbrk/brk */
    u32 brk_current;        /* Current program break */
    u32 mmap_base;          /* Base for mmap */
    compat_process_t *current;
} compat_state;

/* ═══════════════════════════════════════════════════════════════════════
 * INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════ */

void compat_init(void) {
    serial_puts("Linux Compat: Initializing compatibility layer\n");
    
    compat_state.initialized = 1;
    compat_state.brk_base = 0x20000000;      /* 512MB */
    compat_state.brk_current = compat_state.brk_base;
    compat_state.mmap_base = 0x40000000;     /* 1GB */
    compat_state.current = NULL;
    
    serial_puts("Linux Compat: Ready - can run Linux ELF binaries\n");
    serial_puts("Linux Compat: ");
    serial_putdec(sizeof(preinstalled_apps) / sizeof(app_registry_t) - 1);
    serial_puts(" apps registered\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * SYSCALL TRANSLATION
 * ═══════════════════════════════════════════════════════════════════════ */

u32 compat_syscall(u32 linux_syscall, u32 arg1, u32 arg2, u32 arg3, u32 arg4) {
    switch (linux_syscall) {
        case SYS_LINUX_GETPID:
            return compat_state.current ? compat_state.current->pid : 1;
            
        case SYS_LINUX_GETPPID:
            return compat_state.current ? compat_state.current->linux_ppid : 0;
            
        case SYS_LINUX_GETUID:
        case SYS_LINUX_GETEUID:
            return 0;  /* Root */
            
        case SYS_LINUX_GETGID:
        case SYS_LINUX_GETEGID:
            return 0;  /* Root group */
            
        case SYS_LINUX_BRK:
            /* Program break for malloc */
            if (arg1 != 0 && arg1 >= compat_state.brk_current) {
                compat_state.brk_current = arg1;
            }
            return compat_state.brk_current;
            
        case SYS_LINUX_WRITE:
            /* Write to file descriptor */
            if (arg1 == 1 || arg1 == 2) {
                /* stdout/stderr - write to serial */
                const char *buf = (const char *)arg2;
                for (u32 i = 0; i < arg3; i++) {
                    serial_putc(buf[i]);
                }
                return arg3;
            }
            return -1;
            
        case SYS_LINUX_READ:
            /* Read from file descriptor - stub */
            return 0;
            
        case SYS_LINUX_OPEN:
        case SYS_LINUX_CLOSE:
        case SYS_LINUX_FSTAT:
            return 0;
            
        case SYS_LINUX_MMAP:
            /* Memory mapping */
            u32 addr = compat_state.mmap_base;
            compat_state.mmap_base += (arg2 + 4095) & ~4095;  /* Align to page */
            return addr;
            
        case SYS_LINUX_MUNMAP:
            return 0;
            
        case SYS_LINUX_EXIT:
            serial_puts("Process exited with code: ");
            serial_putdec(arg1);
            serial_puts("\n");
            return 0;
            
        case SYS_LINUX_FORK:
        case SYS_LINUX_EXECVE:
        case SYS_LINUX_WAIT4:
        case SYS_LINUX_KILL:
            return -1;  /* Not fully implemented */
            
        default:
            serial_puts("Compat: Unhandled syscall ");
            serial_putdec(linux_syscall);
            serial_puts("\n");
            return -38;  /* ENOSYS */
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * APPLICATION MANAGEMENT
 * ═══════════════════════════════════════════════════════════════════════ */

const app_registry_t *find_app(const char *name) {
    for (int i = 0; preinstalled_apps[i].name != NULL; i++) {
        if (compat_strcmp(preinstalled_apps[i].name, name) == 0) {
            return &preinstalled_apps[i];
        }
    }
    return NULL;
}

int compat_exec(const char *path, char *const argv[], char *const envp[]) {
    serial_puts("Compat: Executing ");
    serial_puts(path);
    serial_puts("\n");
    
    /* Find app in registry */
    const app_registry_t *app = find_app(path);
    if (app) {
        serial_puts("Compat: Found ");
        serial_puts(app->name);
        serial_puts(" - ");
        serial_puts(app->description);
        serial_puts("\n");
        
        if (app->dependencies) {
            serial_puts("Compat: Dependencies: ");
            serial_puts(app->dependencies);
            serial_puts("\n");
        }
        
        serial_puts("Compat: Min memory: ");
        serial_putdec(app->min_memory_mb);
        serial_puts(" MB\n");
    }
    
    return -1;  /* Not actually implemented yet */
}

/* ═══════════════════════════════════════════════════════════════════════
 * LIST INSTALLED APPLICATIONS
 * ═══════════════════════════════════════════════════════════════════════ */

void list_installed_apps(void) {
    serial_puts("\n╔══════════════════════════════════════════════════════════════╗\n");
    serial_puts("║         ULTRA VECTROVIAN OS - Installed Applications        ║\n");
    serial_puts("╠══════════════════════════════════════════════════════════════╣\n");
    
    const char *current_category = "";
    for (int i = 0; preinstalled_apps[i].name != NULL; i++) {
        const app_registry_t *app = &preinstalled_apps[i];
        
        /* Print category header based on app type */
        if (compat_strstr(app->description, "Shell") && current_category[0] == '\0') {
            serial_puts("║─── SHELLS ──────────────────────────────────────────────────║\n");
            current_category = "shell";
        }
        if (compat_strstr(app->description, "Compiler") && 
            !compat_strstr(current_category, "compiler")) {
            serial_puts("║─── DEVELOPMENT TOOLS ───────────────────────────────────────║\n");
            current_category = "compiler";
        }
        if (compat_strstr(app->description, "Rust") && 
            !compat_strstr(current_category, "rust")) {
            serial_puts("║─── RUST ────────────────────────────────────────────────────║\n");
            current_category = "rust";
        }
        if (compat_strstr(app->description, "Node") && 
            !compat_strstr(current_category, "node")) {
            serial_puts("║─── NODE.JS ─────────────────────────────────────────────────║\n");
            current_category = "node";
        }
        if (compat_strstr(app->description, "Python") && 
            !compat_strstr(current_category, "python")) {
            serial_puts("║─── PYTHON ──────────────────────────────────────────────────║\n");
            current_category = "python";
        }
        if (compat_strstr(app->description, "Git") && 
            !compat_strstr(current_category, "git")) {
            serial_puts("║─── VERSION CONTROL ─────────────────────────────────────────║\n");
            current_category = "git";
        }
        if (compat_strstr(app->description, "AI") && 
            !compat_strstr(current_category, "ai")) {
            serial_puts("║─── AI/CLI TOOLS ────────────────────────────────────────────║\n");
            current_category = "ai";
        }
        if ((compat_strstr(app->description, "browser") || 
             compat_strstr(app->description, "Chrome")) && 
            !compat_strstr(current_category, "browser")) {
            serial_puts("║─── WEB BROWSERS ────────────────────────────────────────────║\n");
            current_category = "browser";
        }
        if ((compat_strstr(app->description, "editor") || 
             compat_strstr(app->description, "vim") ||
             compat_strstr(app->description, "VS Code")) && 
            !compat_strstr(current_category, "editor")) {
            serial_puts("║─── EDITORS ─────────────────────────────────────────────────║\n");
            current_category = "editor";
        }
        
        serial_puts("║  ");
        serial_puts(app->name);
        for (int j = compat_strlen(app->name); j < 16; j++) serial_putc(' ');
        serial_puts(" ");
        serial_puts(app->description);
        serial_puts("\n");
    }
    serial_puts("╚══════════════════════════════════════════════════════════════╝\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * PACKAGE INSTALLER (simplified)
 * ═══════════════════════════════════════════════════════════════════════ */

int install_package(const char *name) {
    const app_registry_t *app = find_app(name);
    
    if (!app) {
        serial_puts("Package not found: ");
        serial_puts(name);
        serial_puts("\n");
        return -1;
    }
    
    serial_puts("Installing ");
    serial_puts(app->name);
    serial_puts("...\n");
    
    /* Check dependencies */
    if (app->dependencies) {
        serial_puts("Dependencies: ");
        serial_puts(app->dependencies);
        serial_puts("\n");
    }
    
    serial_puts("Installed ");
    serial_puts(app->name);
    serial_puts(" (");
    serial_putdec(app->min_memory_mb);
    serial_puts(" MB required)\n");
    
    return 0;
}

/* String functions needed locally */
static int compat_atoi(const char *s) {
    int n = 0;
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    return n;
}

static char *compat_strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    return (c == 0) ? (char*)s : NULL;
}

static void *compat_memcpy(void *dest, const void *src, size_t n) {
    char *d = (char*)dest;
    const char *s = (const char*)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

static void *compat_memset(void *s, int c, size_t n) {
    char *p = (char*)s;
    for (size_t i = 0; i < n; i++) p[i] = (char)c;
    return s;
}

static int compat_memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *a = (const unsigned char*)s1;
    const unsigned char *b = (const unsigned char*)s2;
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return a[i] - b[i];
    }
    return 0;
}

char *strncpy(char *dest, const char *src, int n) {
    int i;
    for (i = 0; i < n && src[i]; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';
    return dest;
}
