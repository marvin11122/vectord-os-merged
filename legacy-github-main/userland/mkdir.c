/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - mkdir - Create directories
 * ═══════════════════════════════════════════════════════════════════════ */

#include "libc.h"

static void print_usage(void) {
    puts("Usage: mkdir [OPTIONS] DIRECTORY...");
    puts("Create directories");
    puts("");
    puts("Options:");
    puts("  -m MODE   Set file permission mode (default: 0755)");
    puts("  -p        Create parent directories as needed");
    puts("  -v        Print a message for each created directory");
    puts("  -h        Display this help");
}

int main(int argc, char *argv[]) {
    int create_parents = 0;
    int verbose = 0;
    int mode = 0755;
    
    /* Parse options */
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-p") == 0) {
                create_parents = 1;
            } else if (strcmp(argv[i], "-v") == 0) {
                verbose = 1;
            } else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
                mode = atoi(argv[++i]);
            } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                print_usage();
                return 0;
            } else {
                puts("mkdir: invalid option '");
                puts(argv[i]);
                puts("'");
                return 1;
            }
        } else {
            const char *path = argv[i];
            
            if (create_parents) {
                /* Create parent directories */
                char tmp[MAX_PATH_LEN];
                strcpy(tmp, path);
                
                for (char *p = tmp + 1; *p; p++) {
                    if (*p == '/') {
                        *p = '\0';
                        mkdir(tmp, mode);
                        *p = '/';
                    }
                }
            }
            
            if (mkdir(path, mode) < 0) {
                puts("mkdir: cannot create directory '");
                puts(path);
                puts("'");
                return 1;
            }
            
            if (verbose) {
                puts("mkdir: created directory '");
                puts(path);
                puts("'");
            }
        }
    }
    
    if (argc == 1) {
        puts("mkdir: missing operand");
        print_usage();
        return 1;
    }
    
    return 0;
}

