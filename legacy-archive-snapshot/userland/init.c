/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - Init Process (PID 1)
 * First userspace process, spawns shell and manages system services
 * ═══════════════════════════════════════════════════════════════════════ */

#include "libc.h"

/* VGA colors for init output */
#define COLOR_INFO    0x0B  /* Light Cyan */
#define COLOR_OK      0x0A  /* Light Green */
#define COLOR_WARN    0x0E  /* Yellow */
#define COLOR_ERR     0x0C  /* Light Red */
#define COLOR_DEFAULT 0x0F  /* White */

/* Set text color */
static void set_color(u8 color) {
    /* In a real implementation, this would use a syscall */
    (void)color;
}

/* Print with prefix */
static void init_print(const char *prefix, const char *msg, u8 color) {
    (void)color;
    puts(prefix);
    puts(msg);
}

/* Run a program */
static int run_program(const char *path) {
    u32 pid = fork();
    
    if (pid == 0) {
        /* Child process */
        char *argv[] = { (char *)path, NULL };
        exec(path, argv);
        
        /* If exec fails, exit */
        exit(1);
    }
    
    return (int)pid;
}

/* Mount virtual filesystems */
static void mount_vfs(void) {
    /* Placeholder for mounting proc, sys, dev */
}

/* Set hostname */
static void set_hostname(void) {
    setenv("HOSTNAME", "vectord", 1);
}

/* Set PATH */
static void set_path(void) {
    setenv("PATH", "/bin:/usr/bin:/sbin:/usr/sbin", 1);
}

/* Set HOME */
static void set_home(void) {
    setenv("HOME", "/root", 1);
}

/* Set SHELL */
static void set_shell(void) {
    setenv("SHELL", "/bin/shell", 1);
}

/* Set user */
static void set_user(void) {
    setenv("USER", "root", 1);
    setenv("LOGNAME", "root", 1);
}

/* Initialize system */
static void init_system(void) {
    puts("VECTORD OS Init - Starting system services...");
    puts("");
    
    /* Set environment */
    set_hostname();
    set_path();
    set_home();
    set_shell();
    set_user();
    
    /* Mount filesystems */
    mount_vfs();
    
    /* Create necessary directories */
    mkdir("/tmp", 0777);
    mkdir("/var", 0777);
    mkdir("/var/log", 0755);
    mkdir("/home", 0755);
    mkdir("/home/user", 0755);
    
    puts("[OK] Environment configured");
    puts("[OK] Directories created");
    puts("");
}

/* Start shell */
static void start_shell(void) {
    puts("Starting shell...");
    
    u32 shell_pid = fork();
    
    if (shell_pid == 0) {
        /* Child - run shell */
        char *argv[] = { "/bin/shell", NULL };
        exec("/bin/shell", argv);
        
        /* If shell fails to start, try alternative */
        char *argv2[] = { "/shell", NULL };
        exec("/shell", argv2);
        
        /* If all fails, print error and exit */
        puts("Failed to start shell!");
        exit(1);
    }
    
    /* Parent - wait for shell to exit */
    while (1) {
        int status = 0;
        int pid = waitpid(-1, &status, 0);
        
        if (pid == (int)shell_pid) {
            /* Shell exited, restart it */
            puts("Shell exited, restarting...");
            shell_pid = fork();
            
            if (shell_pid == 0) {
                char *argv[] = { "/bin/shell", NULL };
                exec("/bin/shell", argv);
                exit(1);
            }
        }
        
        /* Reap zombie processes */
        if (pid > 0) continue;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * INIT MAIN - Entry point for userspace
 * ═══════════════════════════════════════════════════════════════════════ */

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    puts("╔══════════════════════════════════════════════════════════════════╗");
    puts("║           ULTRA VECTROVIAN OS - INIT SYSTEM                     ║");
    puts("╚══════════════════════════════════════════════════════════════════╝");
    puts("");
    
    /* Initialize the system */
    init_system();
    
    /* Start shell and manage it */
    start_shell();
    
    /* Should never reach here */
    return 0;
}
