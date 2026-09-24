/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - rm - Remove files and directories
 * ═══════════════════════════════════════════════════════════════════════ */

#include "libc.h"

static int force = 0;
static int recursive = 0;
static int verbose = 0;

static void print_usage(void) {
    puts("Usage: rm [OPTIONS] FILE...");
    puts("Remove files and directories");
    puts("");
    puts("Options:");
    puts("  -f        Ignore nonexistent files, never prompt");
    puts("  -i        Prompt before every removal");
    puts("  -r, -R    Remove directories recursively");
    puts("  -v        Explain what is being done");
    puts("  -h        Display this help");
}

static int remove_file(const char *path) {
    struct stat st;
    
    if (sys_stat(path, &st) < 0) {
        if (!force) {
            puts("rm: cannot remove '");
            puts(path);
            puts("': No such file or directory");
        }
        return -1;
    }
    
    if ((st.st_mode & S_IFMT) == S_IFDIR) {
        if (!recursive) {
            puts("rm: cannot remove '");
            puts(path);
            puts("': Is a directory");
            return -1;
        }
        
        /* Remove directory contents first */
        int fd = open(path, O_RDONLY, 0);
        if (fd >= 0) {
            /* Would read directory entries and remove them */
            close(fd);
        }
        
        if (rmdir(path) < 0) {
            puts("rm: cannot remove '");
            puts(path);
            puts("'");
            return -1;
        }
    } else {
        if (unlink(path) < 0) {
            puts("rm: cannot remove '");
            puts(path);
            puts("'");
            return -1;
        }
    }
    
    if (verbose) {
        puts("removed '");
        puts(path);
        puts("'");
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    /* Parse options */
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            for (char *p = argv[i] + 1; *p; p++) {
                switch (*p) {
                    case 'f': force = 1; break;
                    case 'i': break; /* Interactive mode - not implemented */
                    case 'r':
                    case 'R': recursive = 1; break;
                    case 'v': verbose = 1; break;
                    case 'h':
                        print_usage();
                        return 0;
                    default:
                        puts("rm: invalid option '");
                        putc('-');
                        putc(*p);
                        puts("'");
                        return 1;
                }
            }
        } else {
            if (remove_file(argv[i]) < 0 && !force) {
                return 1;
            }
        }
    }
    
    if (argc == 1) {
        puts("rm: missing operand");
        print_usage();
        return 1;
    }
    
    return 0;
}
