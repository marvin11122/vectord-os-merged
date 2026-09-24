/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - ls - List directory contents
 * ═══════════════════════════════════════════════════════════════════════ */

#include "libc.h"

static int show_all = 0;
static int show_long = 0;

/* Color codes for ls output */
#define COLOR_DIR   0x0B  /* Light Cyan */
#define COLOR_FILE  0x0F  /* White */
#define COLOR_EXEC  0x0A  /* Light Green */
#define COLOR_LINK  0x0E  /* Yellow */

static void print_usage(void) {
    puts("Usage: ls [OPTIONS] [DIRECTORY]");
    puts("List directory contents");
    puts("");
    puts("Options:");
    puts("  -a    Show hidden files");
    puts("  -l    Long listing format");
    puts("  -la   Show all files in long format");
}

static void print_entry(const char *name, int is_dir, int is_exec, int size) {
    if (!show_all && name[0] == '.') {
        return;
    }
    
    if (show_long) {
        /* Permissions */
        if (is_dir) {
            puts("d");
        } else {
            puts("-");
        }
        
        /* Owner/Group/Size/Date - simplified */
        puts("rwxr-xr-x  ");
        
        /* Size */
        char buf[16];
        itoa(size, buf, 10);
        /* Pad */
        int len = strlen(buf);
        for (int i = len; i < 8; i++) putc(' ');
        puts(buf);
        
        puts("  ");
    }
    
    puts(name);
    
    if (show_long) {
        puts("");
    } else {
        puts("  ");
    }
}

static void list_directory(const char *path) {
    int fd = open(path, O_RDONLY, 0);
    
    if (fd < 0) {
        puts("ls: cannot access '");
        puts(path);
        puts("': No such file or directory");
        return;
    }
    
    /* Check if it's a directory */
    struct stat st;
    if (sys_stat(path, &st) < 0) {
        close(fd);
        return;
    }
    
    if ((st.st_mode & S_IFMT) != S_IFDIR) {
        /* Not a directory, just print the file */
        print_entry(path, 0, 0, st.st_size);
        puts("");
        close(fd);
        return;
    }
    
    if (show_long) {
        puts("total 0");
    }
    
    /* For now, print . and .. */
    print_entry(".", 1, 0, 0);
    print_entry("..", 1, 0, 0);
    
    /* In real implementation, would read directory entries */
    
    close(fd);
}

int main(int argc, char *argv[]) {
    const char *path = ".";
    
    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strchr(argv[i], 'a')) show_all = 1;
            if (strchr(argv[i], 'l')) show_long = 1;
            if (strchr(argv[i], 'h') || strcmp(argv[i], "--help") == 0) {
                print_usage();
                return 0;
            }
        } else {
            path = argv[i];
        }
    }
    
    list_directory(path);
    
    return 0;
}
