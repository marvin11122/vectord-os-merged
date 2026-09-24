/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - find - Find files
 * ═══════════════════════════════════════════════════════════════════════ */

#include "libc.h"

static int show_type = 0;
static int case_insensitive = 0;
static int max_depth = -1;

static void print_usage(void) {
    puts("Usage: find [PATH...] [OPTIONS]");
    puts("Search for files in a directory hierarchy");
    puts("");
    puts("Options:");
    puts("  -name PATTERN   File name matches pattern");
    puts("  -type TYPE      File type (f=file, d=directory)");
    puts("  -maxdepth N     Maximum search depth");
    puts("  -i              Case insensitive name matching");
    puts("  -h              Display this help");
}

static void search_directory(const char *path, int depth) {
    if (max_depth >= 0 && depth > max_depth) {
        return;
    }
    
    /* Print path (simplified - would read directory entries) */
    puts(path);
    puts("");
    
    /* Would recurse into subdirectories */
}

int main(int argc, char *argv[]) {
    const char *path = ".";
    const char *name_pattern = NULL;
    const char *type_filter = NULL;
    
    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-name") == 0 && i + 1 < argc) {
            name_pattern = argv[++i];
        } else if (strcmp(argv[i], "-type") == 0 && i + 1 < argc) {
            type_filter = argv[++i];
        } else if (strcmp(argv[i], "-maxdepth") == 0 && i + 1 < argc) {
            max_depth = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-i") == 0) {
            case_insensitive = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        } else if (argv[i][0] != '-') {
            path = argv[i];
        }
    }
    
    (void)name_pattern;
    (void)type_filter;
    (void)case_insensitive;
    
    puts("Searching in: ");
    puts(path);
    puts("");
    
    search_directory(path, 0);
    
    return 0;
}
