/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - cat - Concatenate and display files
 * ═══════════════════════════════════════════════════════════════════════ */

#include "libc.h"

static void print_usage(void) {
    puts("Usage: cat [OPTIONS] [FILE...]");
    puts("Concatenate files and print to standard output");
    puts("");
    puts("Options:");
    puts("  -n    Number all output lines");
    puts("  -b    Number non-blank output lines");
    puts("  -v    Show non-printing characters");
    puts("  -h    Display this help");
}

static void cat_file(const char *filename, int show_line_numbers, int number_nonblank) {
    int fd;
    int line_num = 0;
    int at_line_start = 1;
    char buf[512];
    ssize_t n;
    
    if (strcmp(filename, "-") == 0) {
        fd = STDIN_FILENO;
    } else {
        fd = open(filename, O_RDONLY, 0);
    }
    
    if (fd < 0) {
        puts("cat: ");
        puts(filename);
        puts(": No such file or directory");
        return;
    }
    
    while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        
        for (int i = 0; i < n; i++) {
            if (at_line_start) {
                if (show_line_numbers || (number_nonblank && buf[i] != '\n')) {
                    line_num++;
                    char num_buf[16];
                    itoa(line_num, num_buf, 10);
                    /* Pad to 6 digits */
                    int len = strlen(num_buf);
                    for (int j = len; j < 6; j++) putc(' ');
                    puts(num_buf);
                    puts("  ");
                }
                at_line_start = 0;
            }
            
            putc(buf[i]);
            
            if (buf[i] == '\n') {
                at_line_start = 1;
            }
        }
    }
    
    if (fd != STDIN_FILENO) {
        close(fd);
    }
}

int main(int argc, char *argv[]) {
    int show_line_numbers = 0;
    int number_nonblank = 0;
    int file_count = 0;
    
    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-n") == 0) {
                show_line_numbers = 1;
            } else if (strcmp(argv[i], "-b") == 0) {
                number_nonblank = 1;
            } else if (strcmp(argv[i], "-v") == 0) {
                /* Show non-printing - not implemented */
            } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                print_usage();
                return 0;
            } else if (argv[i][1] == '\0') {
                /* - means stdin */
                cat_file("-", show_line_numbers, number_nonblank);
                file_count++;
            } else {
                puts("cat: invalid option '");
                puts(argv[i]);
                puts("'");
                print_usage();
                return 1;
            }
        } else {
            cat_file(argv[i], show_line_numbers, number_nonblank);
            file_count++;
        }
    }
    
    /* If no files, read from stdin */
    if (file_count == 0) {
        cat_file("-", show_line_numbers, number_nonblank);
    }
    
    return 0;
}

