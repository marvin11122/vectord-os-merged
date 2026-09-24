/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - grep - Search for patterns
 * ═══════════════════════════════════════════════════════════════════════ */

#include "libc.h"

static int show_filename = 0;
static int show_linenum = 0;
static int ignore_case = 0;
static int invert_match = 0;
static int count_only = 0;
static int line_number = 0;

static void print_usage(void) {
    puts("Usage: grep [OPTIONS] PATTERN [FILE...]");
    puts("Search for PATTERN in each FILE");
    puts("");
    puts("Options:");
    puts("  -i    Ignore case");
    puts("  -n    Show line numbers");
    puts("  -v    Invert match");
    puts("  -c    Print only count");
    puts("  -h    Display this help");
}

/* Simple pattern matching (no regex) */
static int match_pattern(const char *line, const char *pattern) {
    if (ignore_case) {
        char line_lower[512];
        char pat_lower[256];
        strcpy(line_lower, line);
        strcpy(pat_lower, pattern);
        
        /* Convert to lowercase */
        for (int i = 0; line_lower[i]; i++) {
            if (line_lower[i] >= 'A' && line_lower[i] <= 'Z') {
                line_lower[i] = line_lower[i] - 'A' + 'a';
            }
        }
        for (int i = 0; pat_lower[i]; i++) {
            if (pat_lower[i] >= 'A' && pat_lower[i] <= 'Z') {
                pat_lower[i] = pat_lower[i] - 'A' + 'a';
            }
        }
        
        return strstr(line_lower, pat_lower) != NULL;
    }
    
    return strstr(line, pattern) != NULL;
}

static void grep_file(const char *filename, const char *pattern) {
    int fd;
    char buf[512];
    ssize_t n;
    char line[512];
    int line_pos = 0;
    int match_count = 0;
    
    if (strcmp(filename, "-") == 0) {
        fd = STDIN_FILENO;
    } else {
        fd = open(filename, O_RDONLY, 0);
    }
    
    if (fd < 0) {
        puts("grep: ");
        puts(filename);
        puts(": No such file or directory");
        return;
    }
    
    line_number = 0;
    
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                line[line_pos] = '\0';
                line_number++;
                
                int matched = match_pattern(line, pattern);
                if (invert_match) matched = !matched;
                
                if (matched) {
                    match_count++;
                    if (!count_only) {
                        if (show_filename) {
                            puts(filename);
                            puts(":");
                        }
                        if (show_linenum) {
                            char num[16];
                            itoa(line_number, num, 10);
                            puts(num);
                            puts(":");
                        }
                        puts(line);
                    }
                }
                
                line_pos = 0;
            } else if (line_pos < sizeof(line) - 1) {
                line[line_pos++] = buf[i];
            }
        }
    }
    
    /* Handle last line without newline */
    if (line_pos > 0) {
        line[line_pos] = '\0';
        line_number++;
        
        int matched = match_pattern(line, pattern);
        if (invert_match) matched = !matched;
        
        if (matched) {
            match_count++;
            if (!count_only) {
                if (show_filename) {
                    puts(filename);
                    puts(":");
                }
                if (show_linenum) {
                    char num[16];
                    itoa(line_number, num, 10);
                    puts(num);
                    puts(":");
                }
                puts(line);
            }
        }
    }
    
    if (count_only) {
        if (show_filename) {
            puts(filename);
            puts(":");
        }
        char num[16];
        itoa(match_count, num, 10);
        puts(num);
    }
    
    if (fd != STDIN_FILENO) {
        close(fd);
    }
}

int main(int argc, char *argv[]) {
    const char *pattern = NULL;
    int file_start = 1;
    
    /* Parse options */
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            if (strcmp(argv[i], "-i") == 0) {
                ignore_case = 1;
            } else if (strcmp(argv[i], "-n") == 0) {
                show_linenum = 1;
            } else if (strcmp(argv[i], "-v") == 0) {
                invert_match = 1;
            } else if (strcmp(argv[i], "-c") == 0) {
                count_only = 1;
            } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                print_usage();
                return 0;
            }
        } else {
            if (!pattern) {
                pattern = argv[i];
            } else {
                break;
            }
            file_start++;
        }
    }
    
    if (!pattern) {
        puts("grep: missing pattern");
        return 1;
    }
    
    if (file_start >= argc) {
        /* Read from stdin */
        grep_file("-", pattern);
    } else {
        if (argc - file_start > 1) {
            show_filename = 1;
        }
        for (int i = file_start; i < argc; i++) {
            grep_file(argv[i], pattern);
        }
    }
    
    return 0;
}
