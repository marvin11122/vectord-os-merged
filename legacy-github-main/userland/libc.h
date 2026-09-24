/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - Userland libc Header
 * Minimal libc for userspace programs using syscalls (int 0x80)
 * ═══════════════════════════════════════════════════════════════════════ */

#ifndef VECLIBC_H
#define VECLIBC_H

/* Basic types */
typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef signed int          i32;
typedef u32                 size_t;
typedef i32                 ssize_t;
typedef i32                 off_t;

#define bool  int
#define true  1
#define false 0
#define NULL  ((void*)0)

/* Syscall numbers - must match kernel */
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

/* Additional syscall numbers for userland */
#define SYS_GETPID  14
#define SYS_GETUID  15
#define SYS_BRK     16
#define SYS_GETCWD  17
#define SYS_CHDIR   18
#define SYS_MKDIR   19
#define SYS_RMDIR   20
#define SYS_UNLINK  21
#define SYS_RENAME  22
#define SYS_STAT    23
#define SYS_LSEEK   24
#define SYS_DUP     25
#define SYS_PIPE    26
#define SYS_WAITPID 27
#define SYS_KILL    28
#define SYS_GETENV  29
#define SYS_SETENV  30
#define SYS_TIMES   31
#define SYS_GETTIME 32
#define SYS_ALARM   33
#define SYS_SIGNAL  34
#define SYS_GETPPID 35
#define SYS_GETPGID 36
#define SYS_SETPGID 37
#define SYS_GETPGID 38

/* File descriptor constants */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* Open flags */
#define O_RDONLY    0
#define O_WRONLY    1
#define O_RDWR      2
#define O_CREAT     0x40
#define O_TRUNC     0x200
#define O_APPEND    0x400

/* File modes */
#define S_IFMT     0170000
#define S_IFREG    0100000
#define S_IFDIR    0040000
#define S_IFCHR    0020000
#define S_IFBLK    0060000
#define S_IFIFO    0010000
#define S_IFLNK    0120000
#define S_IFSOCK   0140000
#define S_ISUID    0004000
#define S_ISGID    0002000
#define S_ISVTX    0001000

/* Permission bits */
#define S_IRWXU    00700
#define S_IRUSR    00400
#define S_IWUSR    00200
#define S_IXUSR    00100
#define S_IRWXG    00070
#define S_IRGRP    00040
#define S_IWGRP    00020
#define S_IXGRP    00010
#define S_IRWXO    00007
#define S_IROTH    00004
#define S_IWOTH    00002
#define S_IXOTH    00001

/* Seek constants */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* Directory entry structure */
struct dirent {
    u32 d_ino;
    char d_name[256];
};

/* Stat structure */
struct stat {
    u32 st_dev;
    u32 st_ino;
    u16 st_mode;
    u16 st_nlink;
    u16 st_uid;
    u16 st_gid;
    u32 st_rdev;
    u32 st_size;
    u32 st_atime;
    u32 st_mtime;
    u32 st_ctime;
    u32 st_blksize;
    u32 st_blocks;
};

/* Process info for ps/top */
struct proc_info {
    u32 pid;
    u32 ppid;
    char name[32];
    u32 state;
    u32 cpu_time;
    u32 memory;
};

/* Path constants */
#define MAX_PATH_LEN 256

/* Additional functions */
char *basename(const char *path);

/* ═══════════════════════════════════════════════════════════════════════
 * SYSCALL WRAPPERS
 * ═══════════════════════════════════════════════════════════════════════ */

/* Basic I/O */
int sys_write(int fd, const void *buf, size_t count);
int sys_read(int fd, void *buf, size_t count);
int sys_puts(const char *str);
int sys_getc(void);

/* File operations */
int sys_open(const char *path, int flags, int mode);
int sys_close(int fd);
int sys_lseek(int fd, int offset, int whence);

/* Directory operations */
int sys_mkdir(const char *path, int mode);
int sys_rmdir(const char *path);
int sys_unlink(const char *path);
int sys_rename(const char *old, const char *newpath);
int sys_getcwd(char *buf, size_t size);
int sys_chdir(const char *path);
int sys_stat(const char *path, struct stat *buf);

/* Process management */
void sys_exit(int status);
u32 sys_getpid(void);
u32 sys_getppid(void);
u32 sys_fork(void);
int sys_exec(const char *path, char *const argv[]);
int sys_waitpid(int pid, int *status, int options);
int sys_kill(int pid, int sig);

/* Memory management */
void *sys_malloc(size_t size);
void sys_free(void *ptr);
void *sys_brk(void *addr);

/* Time */
u32 sys_time(void);
void sys_sleep(u32 seconds);

/* Environment */
char *sys_getenv(const char *name);
int sys_setenv(const char *name, const char *value, int overwrite);

/* ═══════════════════════════════════════════════════════════════════════
 * C LIBRARY FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════ */

/* String functions */
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *a, const void *b, size_t n);

/* Number conversion */
int atoi(const char *s);
long atol(const char *s);
char *itoa(int value, char *buf, int base);
char *utoa(u32 value, char *buf, int base);

/* Output */
void puts(const char *s);
void putc(char c);
int printf(const char *fmt, ...);

/* Memory */
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

/* File I/O */
int open(const char *path, int flags, int mode);
int close(int fd);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
off_t lseek(int fd, off_t offset, int whence);

/* Directory */
int mkdir(const char *path, int mode);
int rmdir(const char *path);
int unlink(const char *path);
int rename(const char *old, const char *newpath);
char *getcwd(char *buf, size_t size);
int chdir(const char *path);

/* Process */
void exit(int status);
u32 getpid(void);
u32 getppid(void);
u32 fork(void);
int exec(const char *path, char *const argv[]);
int waitpid(int pid, int *status, int options);
int kill(int pid, int sig);

/* Environment */
char *getenv(const char *name);
int setenv(const char *name, const char *value, int overwrite);

/* Utility */
void sleep_seconds(u32 seconds);
u32 get_time(void);
char *strdup(const char *s);
int dup(int oldfd);

/* ═══════════════════════════════════════════════════════════════════════
 * VARIADIC MACROS
 * ═══════════════════════════════════════════════════════════════════════ */

typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_end(ap) __builtin_va_end(ap)
#define va_arg(ap, type) __builtin_va_arg(ap, type)

#endif /* VECLIBC_H */

