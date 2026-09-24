/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - Virtual Filesystem (VFS)
 * Minimal working implementation with in-memory fake files
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════ */

#define VFS_MAX_MOUNTS      8
#define VFS_MAX_OPEN_FILES  64
#define VFS_MAX_FS_DRIVERS  8
#define VFS_MAX_FAKE_FILES  16
#define VFS_MAX_PATH        256

/* ═══════════════════════════════════════════════════════════════════════
 * IN-MEMORY FAKE FILE DEFINITIONS
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char *path;
    const char *content;
    u32 size;
    u8  type;       /* 0=regular, 1=directory */
    u32 inode;
} fake_file_t;

/* Fake file data - stored in read-only memory */
static const char config_content[] = "VECTORD OS Config v3.0\n";
#define CONFIG_SIZE 23
static const char motd_content[] = "Welcome to VECTORD OS\n";
#define MOTD_SIZE 24
static const char readme_content[] = "Hello from VECTORD OS!\n";
#define README_SIZE 25
static const char sh_content[] = "VECTORD SHELL BINARY - Minimal stub\n";
#define SH_SIZE 39

/* Directory entries - used in ls functionality */

/* Fake filesystem table */
static const fake_file_t fake_files[VFS_MAX_FAKE_FILES] = {
    /* Directories first */
    {"/etc",             NULL, 0, 1, 1001},
    {"/bin",             NULL, 0, 1, 1002},
    {"/home",            NULL, 0, 1, 1003},
    {"/home/user",       NULL, 0, 1, 1004},
    {"/home/user/.bashrc", "# VECTORD OS bashrc\n", 20, 0, 1005},
    
    /* Regular files */
    {"/etc/config",      config_content, CONFIG_SIZE, 0, 1010},
    {"/etc/motd",        motd_content, MOTD_SIZE, 0, 1011},
    {"/bin/sh",          sh_content, SH_SIZE, 0, 1020},
    {"/home/user/readme.txt", readme_content, README_SIZE, 0, 1030},
    
    /* Sentinel */
    {NULL, NULL, 0, 0, 0}
};

/* ═══════════════════════════════════════════════════════════════════════
 * VFS STATE
 * ═══════════════════════════════════════════════════════════════════════ */

/* Filesystem drivers */
static fs_driver_t *fs_drivers[VFS_MAX_FS_DRIVERS];
static int fs_driver_count = 0;

/* Mount points */
static vfs_mount_t mount_points[VFS_MAX_MOUNTS];
static int mount_count = 0;

/* Open file handles */
static struct {
    file_handle_t handle;
    u8 in_use;
    u32 fake_file_index;  /* Index into fake_files array */
} open_files[VFS_MAX_OPEN_FILES];

static int open_file_count = 0;

/* Root node */
static vfs_node_t root_node;

/* Current working directory */
static char current_dir[VFS_MAX_PATH] = "/";

/* Next available handle ID */
static u32 next_handle_id = 1;

/* ═══════════════════════════════════════════════════════════════════════
 * HELPER FUNCTIONS (no memset, no complex loops)
 * ═══════════════════════════════════════════════════════════════════════ */

/* Simple string compare */
static int vfs_strcmp(const char *a, const char *b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/* Simple string copy with limit */
static void vfs_strcpy(char *dst, const char *src, u32 max_len) {
    u32 i = 0;
    if (!dst || !src || max_len == 0) return;
    while (src[i] && i < max_len - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

/* Find fake file by path */
static int find_fake_file(const char *path) {
    int i = 0;
    while (fake_files[i].path != NULL) {
        if (vfs_strcmp(fake_files[i].path, path) == 0) {
            return i;
        }
        i++;
    }
    return -1;
}

/* Note: init_node_from_fake removed - not needed for current implementation */

/* ═══════════════════════════════════════════════════════════════════════
 * VFS INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════ */

void vfs_init(void) {
    int i;
    
    serial_puts("VFS: Initializing Virtual Filesystem...\n");
    
    /* Initialize counters */
    fs_driver_count = 0;
    mount_count = 0;
    open_file_count = 0;
    next_handle_id = 1;

    
    /* Initialize open files table - mark all as free */
    for (i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        open_files[i].in_use = 0;
        open_files[i].fake_file_index = -1;
        open_files[i].handle.node = NULL;
        open_files[i].handle.offset = 0;
        open_files[i].handle.flags = 0;
        open_files[i].handle.refcount = 0;
    }

    
    /* Initialize root node */
    root_node.name[0] = '/';
    root_node.name[1] = '\0';
    root_node.type = 1;  /* Directory */
    root_node.size = 0;
    root_node.inode = 1;
    root_node.mode = 0755;
    root_node.uid = 0;
    root_node.gid = 0;
    root_node.atime = 0;
    root_node.mtime = 0;
    root_node.ctime = 0;
    root_node.mount = NULL;
    root_node.fs_data = NULL;
    root_node.parent = NULL;
    root_node.first_child = NULL;
    root_node.next_sibling = NULL;
    root_node.refcount = 1;

    
    /* Initialize mount points */
    for (i = 0; i < VFS_MAX_MOUNTS; i++) {
        mount_points[i].mounted = 0;
    }

    
    /* Set current directory */
    current_dir[0] = '/';
    current_dir[1] = '\0';
    
    /* Count fake files for reporting */
    i = 0;
    while (fake_files[i].path != NULL) {
        i++;
    }
    
    serial_puts("VFS: Initialized with ");
    serial_putdec(i);
    serial_puts(" fake entries\n");
    serial_puts("VFS: Ready\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS REGISTER FILESYSTEM DRIVER
 * ═══════════════════════════════════════════════════════════════════════ */

int vfs_register_fs(fs_driver_t *driver) {
    if (!driver) return -1;
    if (fs_driver_count >= VFS_MAX_FS_DRIVERS) return -1;
    
    fs_drivers[fs_driver_count] = driver;
    fs_driver_count++;
    
    serial_puts("VFS: Registered filesystem driver: ");
    serial_puts(driver->name);
    serial_puts("\n");
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS MOUNT
 * ═══════════════════════════════════════════════════════════════════════ */

int vfs_mount_fs(const char *path, u32 dev_id, u32 fs_type) {
    (void)dev_id;
    (void)fs_type;
    
    if (!path) return -1;
    if (mount_count >= VFS_MAX_MOUNTS) return -1;
    
    serial_puts("VFS: Mounting at ");
    serial_puts(path);
    serial_puts(" (stub - always succeeds)\n");
    
    /* Create a simple mount entry */
    vfs_strcpy(mount_points[mount_count].path, path, VFS_MAX_PATH);
    mount_points[mount_count].root = &root_node;
    mount_points[mount_count].driver = NULL;
    mount_points[mount_count].fs_private = NULL;
    mount_points[mount_count].dev_id = dev_id;
    mount_points[mount_count].mounted = 1;
    
    mount_count++;
    
    return 0;
}

/* Legacy wrapper */
int vfs_mount(const char *path, u32 dev_id, u32 fs_type) {
    return vfs_mount_fs(path, dev_id, fs_type);
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS OPEN
 * ═══════════════════════════════════════════════════════════════════════ */

file_handle_t *vfs_open(const char *path, u32 flags) {
    int fd_idx;
    int fake_idx;
    
    if (!path) return NULL;
    
    /* Find the file in fake filesystem */
    fake_idx = find_fake_file(path);
    if (fake_idx < 0) {
        serial_puts("VFS: File not found: ");
        serial_puts(path);
        serial_puts("\n");
        return NULL;
    }
    
    /* Check if it's a directory - can't open directories as files */
    if (fake_files[fake_idx].type == 1) {
        serial_puts("VFS: Cannot open directory as file: ");
        serial_puts(path);
        serial_puts("\n");
        return NULL;
    }
    
    /* Find a free slot in open files table */
    fd_idx = -1;
    for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        if (!open_files[i].in_use) {
            fd_idx = i;
            break;
        }
    }
    
    if (fd_idx < 0) {
        serial_puts("VFS: Too many open files\n");
        return NULL;
    }
    
    /* Initialize the file handle */
    open_files[fd_idx].in_use = 1;
    open_files[fd_idx].fake_file_index = fake_idx;
    open_files[fd_idx].handle.offset = 0;
    open_files[fd_idx].handle.flags = flags;
    open_files[fd_idx].handle.refcount = 1;
    
    /* Allocate and initialize a node for this file */
    vfs_node_t *node = &root_node;  /* Use root as placeholder */
    open_files[fd_idx].handle.node = node;
    
    serial_puts("VFS: Opened ");
    serial_puts(path);
    serial_puts(" (fd=");
    serial_putdec(fd_idx);
    serial_puts(")\n");
    
    return &open_files[fd_idx].handle;
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS READ
 * ═══════════════════════════════════════════════════════════════════════ */

int vfs_read(file_handle_t *handle, void *buf, u32 size) {
    int fd_idx;
    int fake_idx;
    u32 bytes_to_read;
    u32 offset;
    const char *src;
    char *dst;
    u32 i;
    
    if (!handle || !buf) return -1;
    
    /* Find the file handle in our table */
    fd_idx = -1;
    for (int j = 0; j < VFS_MAX_OPEN_FILES; j++) {
        if (&open_files[j].handle == handle && open_files[j].in_use) {
            fd_idx = j;
            break;
        }
    }
    
    if (fd_idx < 0) return -1;
    
    fake_idx = open_files[fd_idx].fake_file_index;
    if (fake_idx < 0) return -1;
    
    /* Get current offset and file data */
    offset = handle->offset;
    src = fake_files[fake_idx].content;
    
    if (!src) return 0;
    
    /* Calculate bytes to read */
    if (offset >= fake_files[fake_idx].size) {
        return 0;  /* EOF */
    }
    
    bytes_to_read = fake_files[fake_idx].size - offset;
    if (bytes_to_read > size) {
        bytes_to_read = size;
    }
    
    /* Copy data */
    dst = (char *)buf;
    for (i = 0; i < bytes_to_read; i++) {
        dst[i] = src[offset + i];
    }
    
    /* Update offset */
    handle->offset = offset + bytes_to_read;
    
    return (int)bytes_to_read;
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS WRITE
 * ═══════════════════════════════════════════════════════════════════════ */

int vfs_write(file_handle_t *handle, const void *buf, u32 size) {
    if (!handle || !buf) return -1;
    
    /* Stub - just return success for now */
    serial_puts("VFS: Write called (stub)\n");
    handle->offset += size;
    return (int)size;
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS CLOSE
 * ═══════════════════════════════════════════════════════════════════════ */

int vfs_close(file_handle_t *handle) {
    int fd_idx;
    
    if (!handle) return -1;
    
    /* Find the file handle in our table */
    fd_idx = -1;
    for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        if (&open_files[i].handle == handle && open_files[i].in_use) {
            fd_idx = i;
            break;
        }
    }
    
    if (fd_idx < 0) return -1;
    
    /* Mark as free */
    open_files[fd_idx].in_use = 0;
    open_files[fd_idx].fake_file_index = -1;
    open_files[fd_idx].handle.node = NULL;
    open_files[fd_idx].handle.offset = 0;
    open_files[fd_idx].handle.flags = 0;
    open_files[fd_idx].handle.refcount = 0;
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS FSTAT (stat by file handle)
 * ═══════════════════════════════════════════════════════════════════════ */

int vfs_fstat(file_handle_t *handle, stat_t *st) {
    int fd_idx;
    int fake_idx;
    
    if (!handle || !st) return -1;
    
    /* Find the file handle in our table */
    fd_idx = -1;
    for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        if (&open_files[i].handle == handle && open_files[i].in_use) {
            fd_idx = i;
            break;
        }
    }
    
    if (fd_idx < 0) return -1;
    
    fake_idx = open_files[fd_idx].fake_file_index;
    if (fake_idx < 0) return -1;
    
    /* Fill in stat structure */
    st->dev_id = 0;
    st->inode = fake_files[fake_idx].inode;
    st->mode = 0755 | (fake_files[fake_idx].type == 1 ? 0040000 : 0100000);
    st->uid = 0;
    st->gid = 0;
    st->size = fake_files[fake_idx].size;
    st->blocks = (fake_files[fake_idx].size + 511) / 512;
    st->atime = 0;
    st->mtime = 0;
    st->ctime = 0;
    st->type = fake_files[fake_idx].type;
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS STAT (stat by path)
 * ═══════════════════════════════════════════════════════════════════════ */

int vfs_stat(const char *path, stat_t *st) {
    int fake_idx;
    
    if (!path || !st) return -1;
    
    fake_idx = find_fake_file(path);
    if (fake_idx < 0) return -1;
    
    /* Fill in stat structure */
    st->dev_id = 0;
    st->inode = fake_files[fake_idx].inode;
    st->mode = 0755 | (fake_files[fake_idx].type == 1 ? 0040000 : 0100000);
    st->uid = 0;
    st->gid = 0;
    st->size = fake_files[fake_idx].size;
    st->blocks = (fake_files[fake_idx].size + 511) / 512;
    st->atime = 0;
    st->mtime = 0;
    st->ctime = 0;
    st->type = fake_files[fake_idx].type;
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS MKDIR
 * ═══════════════════════════════════════════════════════════════════════ */

int vfs_mkdir(const char *path) {
    if (!path) return -1;
    
    serial_puts("VFS: mkdir ");
    serial_puts(path);
    serial_puts(" (stub - always succeeds)\n");
    
    return 0;  /* Stub - always succeeds */
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS RMDIR
 * ═══════════════════════════════════════════════════════════════════════ */

int vfs_rmdir(const char *path) {
    if (!path) return -1;
    
    serial_puts("VFS: rmdir ");
    serial_puts(path);
    serial_puts(" (stub)\n");
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS READ DIR (returns directory entries)
 * ═══════════════════════════════════════════════════════════════════════ */

int vfs_read_dir(const char *path, dirent_t *entries, u32 max_entries) {
    u32 count = 0;
    
    if (!path || !entries) return -1;
    
    /* Check if path is root or a directory */
    if (vfs_strcmp(path, "/") != 0) {
        int dir_idx = find_fake_file(path);
        if (dir_idx < 0 || fake_files[dir_idx].type != 1) {
            return -1;  /* Not a directory */
        }
    }
    
    /* Return entries based on path */
    if (vfs_strcmp(path, "/") == 0) {
        /* Root directory - return top-level entries */
        /* Check for /etc */
        if (count < max_entries) {
            entries[count].inode = 1001;
            entries[count].type = 1;
            vfs_strcpy(entries[count].name, "etc", 256);
            count++;
        }
        /* Check for /bin */
        if (count < max_entries) {
            entries[count].inode = 1002;
            entries[count].type = 1;
            vfs_strcpy(entries[count].name, "bin", 256);
            count++;
        }
        /* Check for /home */
        if (count < max_entries) {
            entries[count].inode = 1003;
            entries[count].type = 1;
            vfs_strcpy(entries[count].name, "home", 256);
            count++;
        }
    } else if (vfs_strcmp(path, "/etc") == 0) {
        /* /etc directory */
        if (count < max_entries) {
            entries[count].inode = 1010;
            entries[count].type = 0;
            vfs_strcpy(entries[count].name, "config", 256);
            count++;
        }
        if (count < max_entries) {
            entries[count].inode = 1011;
            entries[count].type = 0;
            vfs_strcpy(entries[count].name, "motd", 256);
            count++;
        }
    } else if (vfs_strcmp(path, "/bin") == 0) {
        /* /bin directory */
        if (count < max_entries) {
            entries[count].inode = 1020;
            entries[count].type = 0;
            vfs_strcpy(entries[count].name, "sh", 256);
            count++;
        }
    } else if (vfs_strcmp(path, "/home") == 0) {
        /* /home directory */
        if (count < max_entries) {
            entries[count].inode = 1004;
            entries[count].type = 1;
            vfs_strcpy(entries[count].name, "user", 256);
            count++;
        }
    } else if (vfs_strcmp(path, "/home/user") == 0) {
        /* /home/user directory */
        if (count < max_entries) {
            entries[count].inode = 1030;
            entries[count].type = 0;
            vfs_strcpy(entries[count].name, "readme.txt", 256);
            count++;
        }
        if (count < max_entries) {
            entries[count].inode = 1005;
            entries[count].type = 0;
            vfs_strcpy(entries[count].name, ".bashrc", 256);
            count++;
        }
    }
    
    return (int)count;
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS LS - List directory (returns fake entries for testing)
 * ═══════════════════════════════════════════════════════════════════════ */

int vfs_ls(const char *path, dirent_t *entries, u32 max_entries) {
    /* ls is just an alias for read_dir */
    return vfs_read_dir(path, entries, max_entries);
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS SEEK
 * ═══════════════════════════════════════════════════════════════════════ */

int vfs_seek(file_handle_t *handle, u32 offset, u32 whence) {
    int fd_idx;
    int fake_idx;
    
    if (!handle) return -1;
    
    /* Find the file handle in our table */
    fd_idx = -1;
    for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        if (&open_files[i].handle == handle && open_files[i].in_use) {
            fd_idx = i;
            break;
        }
    }
    
    if (fd_idx < 0) return -1;
    
    fake_idx = open_files[fd_idx].fake_file_index;
    if (fake_idx < 0) return -1;
    
    switch (whence) {
        case SEEK_SET:
            handle->offset = offset;
            break;
        case SEEK_CUR:
            handle->offset += offset;
            break;
        case SEEK_END:
            handle->offset = fake_files[fake_idx].size + offset;
            break;
        default:
            return -1;
    }
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS PATH OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════ */

int vfs_chdir(const char *path) {
    if (!path) return -1;
    
    /* Check if path exists and is a directory */
    int fake_idx = find_fake_file(path);
    if (fake_idx >= 0 && fake_files[fake_idx].type == 1) {
        vfs_strcpy(current_dir, path, VFS_MAX_PATH);
        return 0;
    }
    
    /* Allow root */
    if (vfs_strcmp(path, "/") == 0) {
        current_dir[0] = '/';
        current_dir[1] = '\0';
        return 0;
    }
    
    return -1;
}

const char *vfs_getcwd(void) {
    return current_dir;
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS RESOLVE PATH
 * ═══════════════════════════════════════════════════════════════════════ */

vfs_node_t *vfs_resolve_path(const char *path) {
    /* Simple stub - just return root node */
    (void)path;
    return &root_node;
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS SYNC
 * ═══════════════════════════════════════════════════════════════════════ */

void vfs_sync(void) {
    serial_puts("VFS: Sync called (stub)\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS UNMOUNT
 * ═══════════════════════════════════════════════════════════════════════ */

int vfs_unmount(const char *path) {
    if (!path) return -1;
    
    serial_puts("VFS: Unmount ");
    serial_puts(path);
    serial_puts(" (stub)\n");
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * DELETE NODE (stub)
 * ═══════════════════════════════════════════════════════════════════════ */

int vfs_delete(const char *path) {
    if (!path) return -1;
    
    serial_puts("VFS: Delete ");
    serial_puts(path);
    serial_puts(" (stub)\n");
    
    return 0;
}
