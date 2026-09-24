/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - Init System (PID 1)
 * First userspace process, manages system startup and shell
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* Forward declarations */
static void init_main(void);
static void init_setup_environment(void);
static void init_print_welcome(void);
static int init_simulate_login(void);
void init_spawn_shell(void);
void print_prompt_init(void);

/* Global init process pointer */
static process_t *init_proc = NULL;

/* User session info */
static char current_user[32] = "root";
static char current_dir[256] = "/";

/* ═══════════════════════════════════════════════════════════════════════
 * INIT START - Create PID 1 process and begin init sequence
 * ═══════════════════════════════════════════════════════════════════════ */
void init_start(void) {
    serial_puts("\n");
    serial_puts("╔══════════════════════════════════════════════════════════════╗\n");
    serial_puts("║  VECTORD OS Init System Starting                           ║\n");
    serial_puts("║  PID 1 - System Initialization                             ║\n");
    serial_puts("╚══════════════════════════════════════════════════════════════╝\n");
    serial_puts("\n");

    /* Create the init process */
    init_proc = proc_create("init", init_main);
    if (!init_proc) {
        serial_puts("init: CRITICAL - Failed to create init process\n");
        panic("init: Cannot create PID 1");
        return;
    }

    /* Ensure it's PID 1 */
    init_proc->pid = 1;
    serial_puts("init: Init process created with PID 1\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * INIT MAIN - Init process entry point (runs as PID 1)
 * ═══════════════════════════════════════════════════════════════════════ */
static void init_main(void) {
    serial_puts("init: Init process running\n");

    /* Phase 1: Setup environment */
    init_setup_environment();

    /* Phase 2: Print welcome banner */
    init_print_welcome();

    /* Phase 3: Simulate user login */
    init_simulate_login();

    /* Phase 4: Start the shell */
    init_spawn_shell();

    /* Should never reach here */
    serial_puts("init: Shell returned unexpectedly, halting.\n");
    while (1) { asm volatile("hlt"); }

}

/* ═══════════════════════════════════════════════════════════════════════
 * INIT SETUP ENVIRONMENT - Configure system environment
 * ═══════════════════════════════════════════════════════════════════════ */
static void init_setup_environment(void) {
    serial_puts("init: Setting up environment...\n");

    /* Set default paths */
    serial_puts("init:   PATH=/bin:/sbin:/usr/bin:/usr/sbin\n");
    serial_puts("init:   HOME=/home/root\n");
    serial_puts("init:   SHELL=/bin/shell\n");
    serial_puts("init:   USER=root\n");
    serial_puts("init:   TERM=vt100\n");

    /* Create standard file descriptors */
    serial_puts("init:   fd0=stdin (serial)\n");
    serial_puts("init:   fd1=stdout (serial)\n");
    serial_puts("init:   fd2=stderr (serial)\n");

    /* Initialize system directories */
    serial_puts("init: Environment setup complete\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * INIT PRINT WELCOME - Display system welcome message
 * ═══════════════════════════════════════════════════════════════════════ */
static void init_print_welcome(void) {
    serial_puts("\n");
    serial_puts("╔══════════════════════════════════════════════════════════════════╗\n");
    serial_puts("║                                                                  ║\n");
    serial_puts("║   ██╗   ██╗████████╗ ██████╗ ███████╗██████╗ ██████╗            ║\n");
    serial_puts("║   ╚██╗ ██╔╝╚══██╔══╝██╔═══██╗██╔════╝██╔══██╗██╔══██╗           ║\n");
    serial_puts("║    ╚████╔╝    ██║   ██║   ██║███████╗██████╔╝██████╔╝           ║\n");
    serial_puts("║     ╚██╔╝     ██║   ██║   ██║╚════██║██╔═══╝ ██╔══██╗           ║\n");
    serial_puts("║      ██║      ██║   ╚██████╔╝███████║██║     ██║  ██║           ║\n");
    serial_puts("║      ╚═╝      ╚═╝    ╚═════╝ ╚══════╝╚═╝     ╚═╝  ╚═╝           ║\n");
    serial_puts("║                                                                  ║\n");
    serial_puts("║   ULTRA VECTROVIAN OS v3.0.0                                     ║\n");
    serial_puts("║   Codename: NEURAL VECTOR                                        ║\n");
    serial_puts("║   Standalone Kernel - No Linux - No Unix                         ║\n");
    serial_puts("║                                                                  ║\n");
    serial_puts("╚══════════════════════════════════════════════════════════════════╝\n");
    serial_puts("\n");

    serial_puts("System boot complete at ");
    serial_putdec(15); serial_puts("/"); serial_putdec(3); serial_puts("/2026\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * INIT SIMULATE LOGIN - Handle user login simulation
 * ═══════════════════════════════════════════════════════════════════════ */
static int init_simulate_login(void) {
    serial_puts("init: Simulating user login...\n");

    /* In a real system, we would prompt for username/password */
    /* For now, auto-login as root */
    serial_puts("init: User 'root' logged in\n");
    serial_puts("init: Session started\n");

    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * INIT SPAWN SHELL - Start the interactive shell
 * ═══════════════════════════════════════════════════════════════════════ */
void init_spawn_shell(void) {
    serial_puts("\n");
    serial_puts("═══════════════════════════════════════════════════════════════════\n");
    serial_puts("  Starting VECTORD Shell...\n");
    serial_puts("═══════════════════════════════════════════════════════════════════\n");
    serial_puts("\n");

    /* Display shell info */
    serial_puts("VECTORD Shell v1.0.0\n");
    serial_puts("Type 'help' for available commands\n");
    serial_puts("Type 'apps' to see available applications\n");
    serial_puts("\n");

    /* The shell is actually integrated into kernel.c */
    /* This function signals that init is complete */
    serial_puts("init: Shell started successfully\n");
    serial_puts("init: System ready for user interaction\n");
    serial_puts("\n");

    /* Print the prompt */
    print_prompt_init();
}

/* ═══════════════════════════════════════════════════════════════════════
 * PRINT PROMPT - Display shell prompt
 * ═══════════════════════════════════════════════════════════════════════ */
void print_prompt_init(void) {
    serial_puts(current_user);
    serial_puts("@vectord:");
    serial_puts(current_dir);
    serial_puts("$ ");
}

/* ═══════════════════════════════════════════════════════════════════════
 * INIT LOAD BINARY - Load and execute an ELF binary
 * ═══════════════════════════════════════════════════════════════════════ */
void init_load_binary(const char *path) {
    serial_puts("init: Attempting to load ");
    serial_puts(path);
    serial_puts("\n");

    /* Open the file using VFS */
    file_handle_t *file = vfs_open(path, O_RDONLY);
    if (!file) {
        serial_puts("init: Failed to open ");
        serial_puts(path);
        serial_puts("\n");
        return;
    }

    /* Get file size */
    stat_t st;
    if (vfs_fstat(file, &st) != 0) {
        serial_puts("init: Failed to stat file\n");
        vfs_close(file);
        return;
    }

    if (st.size == 0) {
        serial_puts("init: File is empty\n");
        vfs_close(file);
        return;
    }

    /* Allocate buffer for file contents */
    u8 *buffer = (u8 *)kmalloc(st.size);
    if (!buffer) {
        serial_puts("init: Failed to allocate memory for file\n");
        vfs_close(file);
        return;
    }

    /* Read file into buffer */
    if (vfs_read(file, buffer, st.size) != (int)st.size) {
        serial_puts("init: Failed to read file\n");
        kfree(buffer);
        vfs_close(file);
        return;
    }

    vfs_close(file);

    /* Load ELF binary */
    serial_puts("init: Loading ELF binary\n");
    if (elf_load(buffer, st.size, init_proc) != 0) {
        serial_puts("init: ELF load failed\n");
        kfree(buffer);
        return;
    }

    kfree(buffer);

    /* Jump to entry point (this will not return if successful) */
    serial_puts("init: Jumping to entry point\n");
    elf_jump_to_entry(init_proc->eip, init_proc->stack_base + init_proc->stack_size);

    /* Should never reach here */
    serial_puts("init: Unexpected return from elf_jump_to_entry\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * INIT REAP CHILDREN - Wait for and clean up child processes
 * ═══════════════════════════════════════════════════════════════════════ */
void init_reap_children(void) {
    serial_puts("init: Reaping children (stub)\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * INIT SIGNAL HANDLER - Handle signals sent to init
 * ═══════════════════════════════════════════════════════════════════════ */
void init_signal_handler(int signum) {
    (void)signum;
    serial_puts("init: Signal handler called (stub)\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * INIT ADOPT ORPHANS - Adopt orphan processes
 * ═══════════════════════════════════════════════════════════════════════ */
void init_adopt_orphans(void) {
    serial_puts("init: Adopting orphans (stub)\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * INIT WAIT FOR CHILDREN - Wait for child process termination
 * ═══════════════════════════════════════════════════════════════════════ */
void init_wait_for_children(void) {
    serial_puts("init: Waiting for children (stub)\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * GET CURRENT USER - Return current username
 * ═══════════════════════════════════════════════════════════════════════ */
const char *init_get_current_user(void) {
    return current_user;
}

/* ═══════════════════════════════════════════════════════════════════════
 * GET CURRENT DIR - Return current directory
 * ═══════════════════════════════════════════════════════════════════════ */
const char *init_get_current_dir(void) {
    return current_dir;
}

/* ═══════════════════════════════════════════════════════════════════════
 * SET CURRENT DIR - Change current directory
 * ═══════════════════════════════════════════════════════════════════════ */
void init_set_current_dir(const char *dir) {
    int i;
    for (i = 0; i < 255 && dir[i]; i++) {
        current_dir[i] = dir[i];
    }
    current_dir[i] = '\0';
}

