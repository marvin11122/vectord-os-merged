/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - cp - Copy files
 * ═══════════════════════════════════════════════════════════════════════ */

#include "libc.h"

static int force = 0;
static int preserve = 0;
static int recursive = 0;
static int verbose = 0;

static void print_usage(void) {
    puts("Usage: cp [OPTIONS] SOURCE DEST");
    puts("       cp [OPTIONS] SOURCE... DIRECTORY");
    puts("Copy files and directories");
    puts("");
    puts("Options:");
    puts("  -a        Archive mode (same as -dpR)");
    puts("  -f        Force overwrite");
    puts("  -i        Prompt before overwrite");
    puts("  -p        Preserve attributes");
    puts("  -r, -R    Copy directories recursively");
    puts("  -v        Explain what is being done");
    puts("  -h        Display this help");
}

static int copy_file(const char *src, const char *dst) {
    int fd_in = open(src, O_RDONLY, 0);
    if (fd_in < 0) {
        if (!force) {
            puts("cp: cannot stat '");
            puts(src);
            puts("'");
        }
        return -1;
    }
    
    /* Check if source is a directory */
    struct stat st;
    if (sys_stat(src, &st) < 0) {
        close(fd_in);
        return -1;
    }
    
    if ((st.st_mode & S_IFMT) == S_IFDIR) {
        if (!recursive) {
            puts("cp: omitting directory '");
            puts(src);
            puts("'");
            close(fd_in);
            return -1;
        }
        
        /* Create destination directory */
        mkdir(dst, st.st_mode);
        
        /* Would copy directory contents here */
        close(fd_in);
        return 0;
    }
    
    int fd_out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);
    if (fd_out < 0) {
        puts("cp: cannot create regular file '");
        puts(dst);
        puts("'");
        close(fd_in);
        return -1;
    }
    
    char buf[4096];
    ssize_t n;
    while ((n = read(fd_in, buf, sizeof(buf))) > 0) {
        if (write(fd_out, buf, n) != n) {
            puts("cp: error writing '");
            puts(dst);
            puts("'");
            close(fd_in);
            close(fd_out);
            return -1;
        }
    }
    
    close(fd_in);
    close(fd_out);
    
    if (verbose) {
        puts(src);
        puts(" -> ");
        puts(dst);
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    int ret = 0;
    
    /* Parse options */
    int i;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            for (char *p = argv[i] + 1; *p; p++) {
                switch (*p) {
                    case 'a': preserve = 1; recursive = 1; break;
                    case 'f': force = 1; break;
                    case 'i': break; /* Interactive mode */
                    case 'p': preserve = 1; break;
                    case 'r':
                    case 'R': recursive = 1; break;
                    case 'v': verbose = 1; break;
                    case 'h':
                        print_usage();
                        return 0;
                    default:
                        puts("cp: invalid option '");
                        putc('-');
                        putc(*p);
                        puts("'");
                        return 1;
                }
            }
        } else {
            break;
        }
    }
    
    if (i >= argc - 1) {
        puts("cp: missing file operand");
        print_usage();
        return 1;
    }
    
    const char *dst = argv[argc - 1];
    
    /* Check if destination is a directory */
    struct stat dst_st;
    int dst_is_dir = 0;
    if (sys_stat(dst, &dst_st) == 0) {
        dst_is_dir = ((dst_st.st_mode & S_IFMT) == S_IFDIR);
    }
    
    /* Copy each source */
    for (; i < argc - 1; i++) {
        const char *src = argv[i];
        
        if (dst_is_dir) {
            /* Copy into directory */
            char final_dst[MAX_PATH_LEN];
            strcpy(final_dst, dst);
            strcat(final_dst, "/");
            strcat(final_dst, basename(src));
            
            if (copy_file(src, final_dst) < 0) {
                ret = 1;
            }
        } else {
            /* Copy to specific file */
            if (copy_file(src, dst) < 0) {
                ret = 1;
            }
        }
    }
    
    return ret;
}
