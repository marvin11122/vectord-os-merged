/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - Userland libc
 * Minimal libc for userspace programs using syscalls (int 0x80)
 * ═══════════════════════════════════════════════════════════════════════ */

#include "libc.h"

/* ═══════════════════════════════════════════════════════════════════════
 * SYSCALL IMPLEMENTATION
 * Uses int 0x80 to call kernel syscalls
 * ═══════════════════════════════════════════════════════════════════════ */

static inline u32 syscall0(u32 num) {
    u32 ret;
    asm volatile ("int $0x80" : "=a"(ret) : "a"(num));
    return ret;
}

static inline u32 syscall1(u32 num, u32 arg1) {
    u32 ret;
    asm volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1));
    return ret;
}

static inline u32 syscall2(u32 num, u32 arg1, u32 arg2) {
    u32 ret;
    asm volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2));
    return ret;
}

static inline u32 syscall3(u32 num, u32 arg1, u32 arg2, u32 arg3) {
    u32 ret;
    asm volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3));
    return ret;
}

static inline u32 syscall4(u32 num, u32 arg1, u32 arg2, u32 arg3, u32 arg4) {
    u32 ret;
    asm volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4));
    return ret;
}

/* ═══════════════════════════════════════════════════════════════════════
 * SYSCALL WRAPPERS
 * ═══════════════════════════════════════════════════════════════════════ */

int sys_write(int fd, const void *buf, size_t count) {
    return syscall3(SYS_WRITE, (u32)fd, (u32)buf, (u32)count);
}

int sys_read(int fd, void *buf, size_t count) {
    return syscall3(SYS_READ, (u32)fd, (u32)buf, (u32)count);
}

int sys_puts(const char *str) {
    return syscall1(SYS_PUTS, (u32)str);
}

int sys_getc(void) {
    return syscall0(SYS_GETC);
}

int sys_open(const char *path, int flags, int mode) {
    return syscall3(SYS_OPEN, (u32)path, (u32)flags, (u32)mode);
}

int sys_close(int fd) {
    return syscall1(SYS_CLOSE, (u32)fd);
}

int sys_lseek(int fd, int offset, int whence) {
    return syscall3(SYS_LSEEK, (u32)fd, (u32)offset, (u32)whence);
}

int sys_mkdir(const char *path, int mode) {
    return syscall2(SYS_MKDIR, (u32)path, (u32)mode);
}

int sys_rmdir(const char *path) {
    return syscall1(SYS_RMDIR, (u32)path);
}

int sys_unlink(const char *path) {
    return syscall1(SYS_UNLINK, (u32)path);
}

int sys_rename(const char *old, const char *newpath) {
    return syscall2(SYS_RENAME, (u32)old, (u32)newpath);
}

int sys_getcwd(char *buf, size_t size) {
    return syscall2(SYS_GETCWD, (u32)buf, (u32)size);
}

int sys_chdir(const char *path) {
    return syscall1(SYS_CHDIR, (u32)path);
}

int sys_stat(const char *path, struct stat *buf) {
    return syscall2(SYS_STAT, (u32)path, (u32)buf);
}

void sys_exit(int status) {
    syscall1(SYS_EXIT, (u32)status);
    __builtin_unreachable();
}

u32 sys_getpid(void) {
    return syscall0(SYS_GETPID);
}

u32 sys_getppid(void) {
    return syscall0(SYS_GETPPID);
}

u32 sys_fork(void) {
    return syscall0(SYS_FORK);
}

int sys_exec(const char *path, char *const argv[]) {
    return syscall2(SYS_EXEC, (u32)path, (u32)argv);
}

int sys_waitpid(int pid, int *status, int options) {
    return syscall3(SYS_WAITPID, (u32)pid, (u32)status, (u32)options);
}

int sys_kill(int pid, int sig) {
    return syscall2(SYS_KILL, (u32)pid, (u32)sig);
}

void *sys_malloc(size_t size) {
    return (void *)syscall1(SYS_MALLOC, (u32)size);
}

void sys_free(void *ptr) {
    syscall1(SYS_FREE, (u32)ptr);
}

void *sys_brk(void *addr) {
    return (void *)syscall1(SYS_BRK, (u32)addr);
}

u32 sys_time(void) {
    return syscall0(SYS_GETTIME);
}

void sys_sleep(u32 seconds) {
    syscall1(SYS_SLEEP, seconds);
}

char *sys_getenv(const char *name) {
    return (char *)syscall1(SYS_GETENV, (u32)name);
}

int sys_setenv(const char *name, const char *value, int overwrite) {
    return syscall3(SYS_SETENV, (u32)name, (u32)value, (u32)overwrite);
}

/* ═══════════════════════════════════════════════════════════════════════
 * C LIBRARY FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════ */

/* String functions */
size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *(unsigned char *)a - *(unsigned char *)b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *a == *b) {
        a++;
        b++;
        n--;
    }
    if (n == 0) return 0;
    return *(unsigned char *)a - *(unsigned char *)b;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    char *d = dest;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = '\0';
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest + strlen(dest);
    while ((*d++ = *src++));
    return dest;
}

char *strncat(char *dest, const char *src, size_t n) {
    char *d = dest + strlen(dest);
    while (n && (*d++ = *src++)) n--;
    *d = '\0';
    return dest;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    if (c == 0) return (char *)s;
    return NULL;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if (c == 0) return (char *)s;
    return (char *)last;
}

char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    size_t nlen = strlen(needle);
    while (*haystack) {
        if (strncmp(haystack, needle, nlen) == 0) return (char *)haystack;
        haystack++;
    }
    return NULL;
}

void *memset(void *s, int c, size_t n) {
    u8 *p = (u8 *)s;
    while (n--) *p++ = (u8)c;
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
    u8 *d = (u8 *)dest;
    const u8 *s = (const u8 *)src;
    while (n--) *d++ = *s++;
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    u8 *d = (u8 *)dest;
    const u8 *s = (const u8 *)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

int memcmp(const void *a, const void *b, size_t n) {
    const u8 *pa = (const u8 *)a;
    const u8 *pb = (const u8 *)b;
    while (n--) {
        if (*pa != *pb) return *pa - *pb;
        pa++;
        pb++;
    }
    return 0;
}

/* Number conversion */
int atoi(const char *s) {
    int n = 0;
    int neg = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    return neg ? -n : n;
}

long atol(const char *s) {
    return (long)atoi(s);
}

char *itoa(int value, char *buf, int base) {
    char *p = buf;
    char *first_digit = buf;
    char tmp;
    u32 uvalue;
    int negative = 0;
    
    if (base == 10 && value < 0) {
        negative = 1;
        uvalue = (u32)(-value);
    } else {
        uvalue = (u32)value;
    }
    
    do {
        u32 digit = uvalue % base;
        *p++ = (digit < 10) ? '0' + digit : 'a' + digit - 10;
    } while (uvalue /= base);
    
    if (negative) *p++ = '-';
    *p = '\0';
    
    /* Reverse */
    p--;
    while (first_digit < p) {
        tmp = *first_digit;
        *first_digit = *p;
        *p = tmp;
        first_digit++;
        p--;
    }
    
    return buf;
}

char *utoa(u32 value, char *buf, int base) {
    char *p = buf;
    char *first_digit = buf;
    char tmp;
    
    do {
        u32 digit = value % base;
        *p++ = (digit < 10) ? '0' + digit : 'a' + digit - 10;
    } while (value /= base);
    
    *p = '\0';
    
    /* Reverse */
    p--;
    while (first_digit < p) {
        tmp = *first_digit;
        *first_digit = *p;
        *p = tmp;
        first_digit++;
        p--;
    }
    
    return buf;
}

/* Output - uses VGA directly at 0xB8000 for now */
static volatile u16 *vga_mem = (u16 *)0xB8000;
static int vga_x = 0, vga_y = 0;
static u8 vga_color = 0x0F;

static void vga_putchar_user(char c) {
    if (c == '\n') {
        vga_x = 0;
        vga_y++;
    } else if (c == '\r') {
        vga_x = 0;
    } else if (c == '\b') {
        if (vga_x > 0) {
            vga_x--;
            vga_mem[vga_y * 80 + vga_x] = (u16)' ' | ((u16)vga_color << 8);
        }
    } else {
        vga_mem[vga_y * 80 + vga_x] = (u16)c | ((u16)vga_color << 8);
        vga_x++;
    }
    
    if (vga_x >= 80) {
        vga_x = 0;
        vga_y++;
    }
    
    if (vga_y >= 25) {
        /* Scroll */
        for (int i = 0; i < 24 * 80; i++) {
            vga_mem[i] = vga_mem[i + 80];
        }
        for (int i = 24 * 80; i < 25 * 80; i++) {
            vga_mem[i] = (u16)' ' | ((u16)vga_color << 8);
        }
        vga_y = 24;
    }
}

void putc(char c) {
    vga_putchar_user(c);
}

void puts(const char *s) {
    while (*s) {
        vga_putchar_user(*s++);
    }
    vga_putchar_user('\n');
}

/* Simple printf - limited functionality */
int printf(const char *fmt, ...) {
    va_list ap;
    int count = 0;
    
    va_start(ap, fmt);
    
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            char buf[32];
            
            switch (*fmt) {
                case 'd':
                case 'i':
                    itoa(va_arg(ap, int), buf, 10);
                    puts(buf);
                    count += strlen(buf);
                    break;
                case 'u':
                    utoa(va_arg(ap, u32), buf, 10);
                    puts(buf);
                    count += strlen(buf);
                    break;
                case 'x':
                    utoa(va_arg(ap, u32), buf, 16);
                    puts(buf);
                    count += strlen(buf);
                    break;
                case 'X':
                    utoa(va_arg(ap, u32), buf, 16);
                    puts(buf);
                    count += strlen(buf);
                    break;
                case 'o':
                    utoa(va_arg(ap, u32), buf, 8);
                    puts(buf);
                    count += strlen(buf);
                    break;
                case 's':
                    {
                        const char *str = va_arg(ap, const char *);
                        if (str) {
                            puts(str);
                            count += strlen(str);
                        } else {
                            puts("(null)");
                            count += 6;
                        }
                    }
                    break;
                case 'c':
                    {
                        char c = (char)va_arg(ap, int);
                        putc(c);
                        count++;
                    }
                    break;
                case 'p':
                    putc('0');
                    putc('x');
                    utoa(va_arg(ap, u32), buf, 16);
                    puts(buf);
                    count += strlen(buf) + 2;
                    break;
                case '%':
                    putc('%');
                    count++;
                    break;
                default:
                    putc('%');
                    putc(*fmt);
                    count += 2;
                    break;
            }
        } else {
            vga_putchar_user(*fmt);
            count++;
        }
        fmt++;
    }
    
    va_end(ap);
    return count;
}

/* Memory allocation - uses syscall */
void *malloc(size_t size) {
    return sys_malloc(size);
}

void free(void *ptr) {
    sys_free(ptr);
}

void *calloc(size_t nmemb, size_t size) {
    void *ptr = malloc(nmemb * size);
    if (ptr) memset(ptr, 0, nmemb * size);
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    (void)ptr;
    (void)size;
    /* Simple allocator - no realloc support for now */
    return NULL;
}

/* File I/O wrappers */
int open(const char *path, int flags, int mode) {
    return sys_open(path, flags, mode);
}

int close(int fd) {
    return sys_close(fd);
}

ssize_t read(int fd, void *buf, size_t count) {
    return sys_read(fd, buf, count);
}

ssize_t write(int fd, const void *buf, size_t count) {
    return sys_write(fd, buf, count);
}

off_t lseek(int fd, off_t offset, int whence) {
    return sys_lseek(fd, offset, whence);
}

/* Directory wrappers */
int mkdir(const char *path, int mode) {
    return sys_mkdir(path, mode);
}

int rmdir(const char *path) {
    return sys_rmdir(path);
}

int unlink(const char *path) {
    return sys_unlink(path);
}

int rename(const char *old, const char *newpath) {
    return sys_rename(old, newpath);
}

char *getcwd(char *buf, size_t size) {
    return (char *)sys_getcwd(buf, size);
}

int chdir(const char *path) {
    return sys_chdir(path);
}

/* Process wrappers */
void exit(int status) {
    sys_exit(status);
}

u32 getpid(void) {
    return sys_getpid();
}

u32 getppid(void) {
    return sys_getppid();
}

u32 fork(void) {
    return sys_fork();
}

int exec(const char *path, char *const argv[]) {
    return sys_exec(path, argv);
}

int waitpid(int pid, int *status, int options) {
    return sys_waitpid(pid, status, options);
}

int kill(int pid, int sig) {
    return sys_kill(pid, sig);
}

/* Environment wrappers */
char *getenv(const char *name) {
    return sys_getenv(name);
}

int setenv(const char *name, const char *value, int overwrite) {
    return sys_setenv(name, value, overwrite);
}

/* Time wrappers */
void sleep_seconds(u32 seconds) {
    sys_sleep(seconds);
}

u32 get_time(void) {
    return sys_time();
}

/* ═══════════════════════════════════════════════════════════════════════
 * ADDITIONAL HELPERS
 * ═══════════════════════════════════════════════════════════════════════ */

/* Read a line from stdin */
char *fgets(char *buf, int size, int fd) {
    int i = 0;
    int c;
    
    while (i < size - 1) {
        c = sys_read(fd, &c, 1);
        if (c <= 0) break;
        
        buf[i++] = (char)c;
        
        if (c == '\n') break;
    }
    
    buf[i] = '\0';
    return (i > 0) ? buf : NULL;
}

/* Parse command line arguments */
int parse_args(char *line, char **argv, int max_args) {
    int argc = 0;
    int in_word = 0;
    int in_quote = 0;
    
    while (*line && argc < max_args) {
        if (*line == '"') {
            in_quote = !in_quote;
            if (!in_word) {
                argv[argc++] = line + 1;
                in_word = 1;
            }
            /* Remove quote */
            char *p = line;
            while (*p) {
                *p = *(p + 1);
                p++;
            }
            continue;
        }
        
        if (*line == ' ' || *line == '\t') {
            if (in_word && !in_quote) {
                *line = '\0';
                in_word = 0;
            }
        } else {
            if (!in_word) {
                argv[argc++] = line;
                in_word = 1;
            }
        }
        line++;
    }
    
    if (in_quote) {
        /* Unterminated quote - remove it */
        char *p = argv[argc - 1];
        while (*p) {
            *p = *(p + 1);
            p++;
        }
    }
    
    return argc;
}

/* Convert string to lowercase */
void str_to_lower(char *s) {
    while (*s) {
        if (*s >= 'A' && *s <= 'Z') {
            *s = *s - 'A' + 'a';
        }
        s++;
    }
}

/* Check if string ends with suffix */
int str_ends_with(const char *s, const char *suffix) {
    size_t slen = strlen(s);
    size_t suffix_len = strlen(suffix);
    if (slen < suffix_len) return 0;
    return strcmp(s + slen - suffix_len, suffix) == 0;
}

/* Skip leading whitespace */
char *skip_whitespace(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

