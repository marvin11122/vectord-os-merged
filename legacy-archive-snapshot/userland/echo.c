/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - echo - Display text
 * ═══════════════════════════════════════════════════════════════════════ */

#include "libc.h"

static void print_usage(void) {
    puts("Usage: echo [OPTIONS] [STRING...]");
    puts("Display text");
    puts("");
    puts("Options:");
    puts("  -n    Do not output trailing newline");
    puts("  -e    Enable interpretation of escape sequences");
    puts("  -E    Disable interpretation of escape sequences");
    puts("  -h    Display this help");
}

/* Process escape sequences */
static void process_escape(const char *s) {
    while (*s) {
        if (*s == '\\' && *(s + 1)) {
            s++;
            switch (*s) {
                case 'n': putc('\n'); break;
                case 't': putc('\t'); break;
                case 'v': putc('\v'); break;
                case 'b': putc('\b'); break;
                case 'r': putc('\r'); break;
                case '\\': putc('\\'); break;
                case '0':
                    {
                        /* Octal escape */
                        int val = 0;
                        for (int i = 0; i < 3 && *(s + 1); i++) {
                            s++;
                            if (*s >= '0' && *s <= '7') {
                                val = val * 8 + (*s - '0');
                            } else {
                                s--;
                                break;
                            }
                        }
                        if (val) putc((char)val);
                    }
                    break;
                case 'x':
                    {
                        /* Hex escape */
                        s++;
                        int val = 0;
                        for (int i = 0; i < 2 && *s; i++) {
                            if (*s >= '0' && *s <= '9') {
                                val = val * 16 + (*s - '0');
                            } else if (*s >= 'a' && *s <= 'f') {
                                val = val * 16 + (*s - 'a' + 10);
                            } else if (*s >= 'A' && *s <= 'F') {
                                val = val * 16 + (*s - 'A' + 10);
                            } else {
                                s--;
                                break;
                            }
                            s++;
                        }
                        s--;
                        if (val) putc((char)val);
                    }
                    break;
                default:
                    putc('\\');
                    putc(*s);
                    break;
            }
        } else {
            putc(*s);
        }
        s++;
    }
}

int main(int argc, char *argv[]) {
    int no_newline = 0;
    int interpret_escapes = 0;
    int first_arg = 1;
    
    /* Parse options */
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && first_arg) {
            if (strcmp(argv[i], "-n") == 0) {
                no_newline = 1;
            } else if (strcmp(argv[i], "-e") == 0) {
                interpret_escapes = 1;
            } else if (strcmp(argv[i], "-E") == 0) {
                interpret_escapes = 0;
            } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                print_usage();
                return 0;
            } else {
                break;
            }
        } else {
            first_arg = 0;
            if (i > 1 + (no_newline ? 1 : 0) + (interpret_escapes ? 1 : 0)) {
                putc(' ');
            }
            if (interpret_escapes) {
                process_escape(argv[i]);
            } else {
                puts(argv[i]);
            }
        }
    }
    
    if (!no_newline) {
        putc('\n');
    }
    
    return 0;
}
