/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - vi - Simple text editor
 * Mode-based editor with basic functionality
 * ═══════════════════════════════════════════════════════════════════════ */

#include "libc.h"

#define MAX_LINES       1000
#define MAX_LINE_LEN    256
#define MAX_FILENAME    256

/* Editor modes */
#define MODE_NORMAL  0
#define MODE_INSERT  1
#define MODE_COMMAND 2

/* Editor state */
static char *lines[MAX_LINES];
static int line_count = 0;
static int cursor_x = 0;
static int cursor_y = 0;
static int screen_height = 24;
static int screen_width = 80;
static int scroll_offset = 0;
static int mode = MODE_NORMAL;
static char filename[MAX_FILENAME] = "";
static int modified = 0;
static int running = 1;

/* Command buffer */
static char cmd_buf[256];
static int cmd_pos = 0;

/* ═══════════════════════════════════════════════════════════════════════
 * UTILITY FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════ */

/* Allocate memory for a line */
static char *alloc_line(const char *content) {
    int len = strlen(content);
    char *line = malloc(len + 1);
    if (line) {
        strcpy(line, content);
    }
    return line;
}

/* Free a line */
static void free_line(char *line) {
    if (line) free(line);
}

/* Initialize a new line */
static void init_line(int index, const char *content) {
    if (index >= 0 && index < MAX_LINES) {
        if (lines[index]) free_line(lines[index]);
        lines[index] = alloc_line(content ? content : "");
    }
}

/* Insert a line at position */
static void insert_line(int index, const char *content) {
    if (line_count >= MAX_LINES) return;
    
    /* Shift lines down */
    for (int i = line_count; i > index; i--) {
        lines[i] = lines[i - 1];
    }
    
    lines[index] = alloc_line(content ? content : "");
    line_count++;
    modified = 1;
}

/* Delete a line at position */
static void delete_line(int index) {
    if (index < 0 || index >= line_count) return;
    
    free_line(lines[index]);
    
    /* Shift lines up */
    for (int i = index; i < line_count - 1; i++) {
        lines[i] = lines[i + 1];
    }
    
    line_count--;
    modified = 1;
    
    if (cursor_y >= line_count) {
        cursor_y = line_count - 1;
    }
    if (cursor_y < 0) cursor_y = 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * DISPLAY FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════ */

/* Clear screen */
static void clear_screen(void) {
    for (int i = 0; i < 25; i++) {
        puts("");
    }
}

/* Display status line */
static void show_status(void) {
    char buf[32];
    
    puts("── ");
    puts(filename[0] ? filename : "[No Name]");
    if (modified) puts(" [+]");
    puts(" ── Line ");
    itoa(cursor_y + 1, buf, 10);
    puts(buf);
    puts("/");
    itoa(line_count, buf, 10);
    puts(buf);
    puts(" ── Col ");
    itoa(cursor_x + 1, buf, 10);
    puts(buf);
    puts(" ── ");
    
    switch (mode) {
        case MODE_NORMAL: puts("-- NORMAL --"); break;
        case MODE_INSERT: puts("-- INSERT --"); break;
        case MODE_COMMAND: puts(":"); break;
    }
    puts("");
}

/* Display screen content */
static void refresh_screen(void) {
    clear_screen();
    
    int visible_lines = screen_height - 2;
    
    for (int i = 0; i < visible_lines && (scroll_offset + i) < line_count; i++) {
        int line_idx = scroll_offset + i;
        puts(lines[line_idx] ? lines[line_idx] : "");
        puts("");
    }
    
    /* Fill remaining lines with ~ */
    for (int i = line_count - scroll_offset; i < visible_lines; i++) {
        puts("~");
    }
    
    show_status();
    
    /* Position cursor (simplified) */
    (void)cursor_x;
    (void)cursor_y;
}

/* ═══════════════════════════════════════════════════════════════════════
 * FILE OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════ */

/* Load file */
static int load_file(const char *path) {
    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        return -1;
    }
    
    /* Clear existing content */
    for (int i = 0; i < line_count; i++) {
        free_line(lines[i]);
        lines[i] = NULL;
    }
    line_count = 0;
    
    /* Read file content */
    char buf[512];
    char line_buf[MAX_LINE_LEN];
    int line_pos = 0;
    ssize_t n;
    
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                line_buf[line_pos] = '\0';
                init_line(line_count, line_buf);
                line_count++;
                line_pos = 0;
            } else if (line_pos < MAX_LINE_LEN - 1) {
                line_buf[line_pos++] = buf[i];
            }
        }
    }
    
    /* Handle last line */
    if (line_pos > 0) {
        line_buf[line_pos] = '\0';
        init_line(line_count, line_buf);
        line_count++;
    }
    
    close(fd);
    
    if (line_count == 0) {
        init_line(0, "");
        line_count = 1;
    }
    
    cursor_x = 0;
    cursor_y = 0;
    scroll_offset = 0;
    modified = 0;
    
    strcpy(filename, path);
    
    return 0;
}

/* Save file */
static int save_file(void) {
    if (!filename[0]) {
        puts("No filename specified");
        return -1;
    }
    
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        puts("Cannot create file");
        return -1;
    }
    
    for (int i = 0; i < line_count; i++) {
        if (lines[i]) {
            write(fd, lines[i], strlen(lines[i]));
        }
        write(fd, "\n", 1);
    }
    
    close(fd);
    modified = 0;
    
    puts("Written ");
    
    char buf[16];
    itoa(line_count, buf, 10);
    puts(buf);
    puts(" lines");
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * COMMAND MODE
 * ═══════════════════════════════════════════════════════════════════════ */

static void execute_command(const char *cmd) {
    char buf[MAX_LINE_LEN];
    strcpy(buf, cmd);
    
    /* Parse command */
    char *arg = strchr(buf, ' ');
    if (arg) {
        *arg = '\0';
        arg++;
    }
    
    if (strcmp(buf, "q") == 0) {
        if (modified) {
            puts("File modified; use :q! to force quit");
        } else {
            running = 0;
        }
    } else if (strcmp(buf, "q!") == 0) {
        running = 0;
    } else if (strcmp(buf, "w") == 0) {
        if (arg) {
            strcpy(filename, arg);
        }
        save_file();
    } else if (strcmp(buf, "wq") == 0) {
        save_file();
        running = 0;
    } else if (strcmp(buf, "e") == 0 && arg) {
        if (modified) {
            puts("File modified; use :e! to force edit");
        } else {
            load_file(arg);
        }
    } else if (strcmp(buf, "e!") == 0 && arg) {
        load_file(arg);
    } else if (strcmp(buf, "set") == 0) {
        /* Set options - simplified */
        puts("Options:");
        puts("  set number     - Show line numbers");
        puts("  set nonumber   - Hide line numbers");
    } else if (strcmp(buf, "help") == 0 || strcmp(buf, "h") == 0) {
        puts("Vi commands:");
        puts("  :q       Quit");
        puts("  :w       Write file");
        puts("  :wq      Write and quit");
        puts("  :e FILE  Edit file");
        puts("  :set     Set options");
    } else if (strlen(buf) > 0) {
        puts("Unknown command: ");
        puts(buf);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * INSERT MODE
 * ═══════════════════════════════════════════════════════════════════════ */

static void handle_insert(int c) {
    if (c == 27) {
        /* Escape - return to normal mode */
        mode = MODE_NORMAL;
        if (cursor_x > 0) cursor_x--;
        return;
    }
    
    if (c == '\b' || c == 127) {
        /* Backspace */
        if (cursor_x > 0) {
            /* Delete character before cursor */
            char *line = lines[cursor_y];
            int len = strlen(line);
            for (int i = cursor_x - 1; i < len; i++) {
                line[i] = line[i + 1];
            }
            cursor_x--;
            modified = 1;
        } else if (cursor_y > 0) {
            /* Join with previous line */
            cursor_x = strlen(lines[cursor_y - 1]);
            strcat(lines[cursor_y - 1], lines[cursor_y]);
            delete_line(cursor_y);
            cursor_y--;
        }
    } else if (c == '\n') {
        /* New line */
        char *current = lines[cursor_y];
        char *rest = current + cursor_x;
        
        /* Create new line with rest of current line */
        insert_line(cursor_y + 1, rest);
        
        /* Truncate current line */
        current[cursor_x] = '\0';
        
        cursor_y++;
        cursor_x = 0;
        modified = 1;
    } else if (c >= 32 && c < 127) {
        /* Insert character */
        char *line = lines[cursor_y];
        int len = strlen(line);
        
        /* Make room */
        for (int i = len + 1; i > cursor_x; i--) {
            line[i] = line[i - 1];
        }
        
        line[cursor_x] = (char)c;
        cursor_x++;
        modified = 1;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * NORMAL MODE
 * ═══════════════════════════════════════════════════════════════════════ */

static void handle_normal(int c) {
    char buf[16];
    
    switch (c) {
        case 'i':
            mode = MODE_INSERT;
            break;
            
        case 'a':
            /* Append after cursor */
            if (cursor_x < (int)strlen(lines[cursor_y])) {
                cursor_x++;
            }
            mode = MODE_INSERT;
            break;
            
        case 'A':
            /* Append at end of line */
            cursor_x = strlen(lines[cursor_y]);
            mode = MODE_INSERT;
            break;
            
        case 'o':
            /* Open line below */
            insert_line(cursor_y + 1, "");
            cursor_y++;
            cursor_x = 0;
            mode = MODE_INSERT;
            break;
            
        case 'O':
            /* Open line above */
            insert_line(cursor_y, "");
            cursor_x = 0;
            mode = MODE_INSERT;
            break;
            
        case 'h':
            /* Left */
            if (cursor_x > 0) cursor_x--;
            break;
            
        case 'j':
            /* Down */
            if (cursor_y < line_count - 1) cursor_y++;
            if (cursor_x > (int)strlen(lines[cursor_y])) {
                cursor_x = strlen(lines[cursor_y]);
            }
            break;
            
        case 'k':
            /* Up */
            if (cursor_y > 0) cursor_y--;
            if (cursor_x > (int)strlen(lines[cursor_y])) {
                cursor_x = strlen(lines[cursor_y]);
            }
            break;
            
        case 'l':
            /* Right */
            if (cursor_x < (int)strlen(lines[cursor_y])) cursor_x++;
            break;
            
        case '0':
            /* Beginning of line */
            cursor_x = 0;
            break;
            
        case '$':
            /* End of line */
            cursor_x = strlen(lines[cursor_y]);
            break;
            
        case 'G':
            /* Go to last line */
            cursor_y = line_count - 1;
            cursor_x = 0;
            break;
            
        case 'd':
            /* Delete */
            /* Would need to handle 'dd' */
            break;
            
        case 'y':
            /* Yank */
            /* Would need to handle 'yy' */
            break;
            
        case 'p':
            /* Paste */
            break;
            
        case ':':
            /* Enter command mode */
            mode = MODE_COMMAND;
            cmd_pos = 0;
            cmd_buf[0] = '\0';
            break;
            
        case 'x':
            /* Delete character under cursor */
            {
                char *line = lines[cursor_y];
                int len = strlen(line);
                if (cursor_x < len) {
                    for (int i = cursor_x; i < len; i++) {
                        line[i] = line[i + 1];
                    }
                    modified = 1;
                }
            }
            break;
            
        case 'u':
            /* Undo - not implemented */
            puts("Undo not implemented");
            break;
            
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            /* Count prefix - simplified */
            itoa(c - '0', buf, 10);
            puts(buf);
            break;
            
        case '/':
            /* Search - not implemented */
            puts("Search not implemented");
            break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * MAIN EDITOR LOOP
 * ═══════════════════════════════════════════════════════════════════════ */

int main(int argc, char *argv[]) {
    /* Parse filename argument */
    if (argc > 1) {
        strcpy(filename, argv[1]);
        if (load_file(filename) < 0) {
            /* New file */
            init_line(0, "");
            line_count = 1;
            puts("New file");
        }
    } else {
        init_line(0, "");
        line_count = 1;
    }
    
    /* Main loop */
    while (running) {
        refresh_screen();
        
        int c = sys_getc();
        
        if (c <= 0) {
            sys_sleep(1);
            continue;
        }
        
        switch (mode) {
            case MODE_NORMAL:
                handle_normal(c);
                break;
                
            case MODE_INSERT:
                handle_insert(c);
                break;
                
            case MODE_COMMAND:
                if (c == '\n') {
                    cmd_buf[cmd_pos] = '\0';
                    execute_command(cmd_buf);
                    mode = MODE_NORMAL;
                } else if (c == '\b' || c == 127) {
                    if (cmd_pos > 0) cmd_pos--;
                } else if (c == 27) {
                    mode = MODE_NORMAL;
                } else if (c >= 32 && c < 127 && cmd_pos < 255) {
                    cmd_buf[cmd_pos++] = (char)c;
                }
                break;
        }
    }
    
    /* Cleanup */
    for (int i = 0; i < line_count; i++) {
        if (lines[i]) free_line(lines[i]);
    }
    
    return 0;
}

