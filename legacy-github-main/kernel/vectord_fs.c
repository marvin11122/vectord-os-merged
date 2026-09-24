/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - VECTORD FS Filesystem Driver
 * Simple custom filesystem with vector-based addressing
 * Magic number: 0x56454354 ("VECT")
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD FS STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════ */

/* Superblock - 256 bytes */
typedef struct __attribute__((packed)) {
    u32 magic;              /* Magic number: 0x56454354 ("VECT") */
    u32 version;            /* Filesystem version */
    u32 block_size;         /* Block size in bytes */
    u32 total_blocks;       /* Total number of blocks */
    u32 free_blocks;        /* Number of free blocks */
    u32 total_inodes;       /* Total number of inodes */
    u32 free_inodes;        /* Number of free inodes */
    u32 first_inode_block;  /* First block of inode table */
    u32 first_data_block;   /* First data block */
    u32 inode_size;         /* Size of each inode */
    u32 root_inode;         /* Root directory inode */
    u32 flags;              /* Filesystem flags */
    u32 checksum;           /* Superblock checksum */
    u8  uuid[16];           /* Filesystem UUID */
    char label[32];         /* Filesystem label */
    u32 reserved[24];       /* Reserved for future use */
} vectord_superblock_t;

/* Inode structure - 64 bytes with vector-based addressing */
typedef struct __attribute__((packed)) {
    u16 mode;               /* File mode and permissions */
    u16 uid;                /* Owner user ID */
    u32 size;               /* File size in bytes */
    u32 atime;              /* Access time */
    u32 mtime;              /* Modification time */
    u32 ctime;              /* Creation time */
    u32 dtime;              /* Deletion time */
    u16 gid;                /* Group ID */
    u16 links_count;        /* Hard links count */
    u32 blocks;             /* Number of allocated blocks */
    u32 flags;              /* Inode flags */
    
    /* Vector-based block addressing */
    union {
        struct {
            u32 direct[VECTORD_DIRECT_BLOCKS];      /* Direct blocks */
            u32 single_indirect;                      /* Single indirect */
            u32 double_indirect;                      /* Double indirect */
            u32 triple_indirect;                      /* Triple indirect */
        } addr;
        vec4_t vec_addr;                              /* Vector address */
    } blocks_ptr;
    
    u32 generation;         /* File version/generation */
    u32 acl;                /* ACL pointer */
    u32 reserved[2];        /* Reserved */
} vectord_inode_t;

/* Directory entry - variable length */
typedef struct __attribute__((packed)) {
    u32 inode;              /* Inode number */
    u16 rec_len;            /* Record length */
    u8  name_len;           /* Name length */
    u8  type;               /* File type */
    char name[];            /* File name (variable length) */
} vectord_dirent_t;

/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD FS CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════ */

#define VECTORD_SUPERBLOCK_BLOCK 0
#define VECTORD_SUPERBLOCK_MAGIC_V 0x56454354  /* "VECT" */
#define VECTORD_SUPERBLOCK_SIZE  256
#define VECTORD_FS_VERSION       1

/* Inode flags */
#define VECTORD_INODE_USED_V     0x0001
#define VECTORD_INODE_DIR_V      0x0002
#define VECTORD_INODE_SYMLINK_V  0x0004
#define VECTORD_INODE_DEVICE_V   0x0008

/* File mode bits */
#define VECTORD_S_IFMT   0xF000
#define VECTORD_S_IFREG  0x8000
#define VECTORD_S_IFDIR  0x4000
#define VECTORD_S_IFLNK  0xA000

/* Directory entry type */
#define VECTORD_DET_UNKNOWN   0
#define VECTORD_DET_FILE      1
#define VECTORD_DET_DIR       2
#define VECTORD_DET_SYMLINK   3

/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD FS PRIVATE DATA
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    ata_device_t *device;
    vectord_superblock_t superblock;
    u32 block_size;
    u32 inodes_per_block;
    u32 blocks_per_group;
} vectord_private_t;

/* ═══════════════════════════════════════════════════════════════════════
 * INTERNAL HELPERS
 * ═══════════════════════════════════════════════════════════════════════ */

static void vfs_memset(void *s, int c, u32 n) {
    u8 *p = (u8 *)s;
    while (n--) *p++ = (u8)c;
}

static void vfs_memcpy(void *dest, const void *src, u32 n) {
    u8 *d = (u8 *)dest;
    const u8 *s = (const u8 *)src;
    while (n--) *d++ = *s++;
}

static int vfs_strlen(const char *s) {
    int len = 0;
    while (s && s[len]) len++;
    return len;
}

static int vfs_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *(unsigned char*)a - *(unsigned char*)b;
}

static char *vfs_strcpy_local(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

/* Read sector */
static int vectord_read_sector(ata_device_t *dev, u32 lba, void *buffer) {
    return ata_read_sectors(dev, lba, 1, (u16 *)buffer);
}

/* Read block */
static int vectord_read_block(vectord_private_t *priv, u32 block_num, void *buffer) {
    u32 sectors_per_block = priv->block_size / 512;
    u32 start_sector = block_num * sectors_per_block;
    
    for (u32 i = 0; i < sectors_per_block; i++) {
        if (vectord_read_sector(priv->device, start_sector + i,
                               (u8 *)buffer + i * 512) != 0) {
            return -1;
        }
    }
    return 0;
}

/* Write block */
static int vectord_write_block(vectord_private_t *priv, u32 block_num, const void *buffer) {
    u32 sectors_per_block = priv->block_size / 512;
    u32 start_sector = block_num * sectors_per_block;
    
    for (u32 i = 0; i < sectors_per_block; i++) {
        if (ata_write_sectors(priv->device, start_sector + i,
                             1, (const u16 *)((const u8 *)buffer + i * 512)) != 0) {
            return -1;
        }
    }
    return 0;
}

/* Calculate checksum */
static u32 vectord_calc_checksum(const void *data, u32 size) {
    const u8 *p = (const u8 *)data;
    u32 sum = 0;
    for (u32 i = 0; i < size; i++) {
        sum = (sum << 7) | (sum >> 25);
        sum += p[i];
    }
    return sum;
}

/* Read inode */
static int vectord_read_inode(vectord_private_t *priv, u32 inode_num, vectord_inode_t *inode) {
    if (inode_num == 0 || inode_num >= priv->superblock.total_inodes) {
        return -1;
    }
    
    /* Calculate inode location */
    u32 inode_index = inode_num - 1;
    u32 inodes_per_block = priv->block_size / sizeof(vectord_inode_t);
    u32 block_num = priv->superblock.first_inode_block + (inode_index / inodes_per_block);
    u32 index_in_block = inode_index % inodes_per_block;
    
    u8 buffer[4096];
    if (vectord_read_block(priv, block_num, buffer) != 0) {
        return -1;
    }
    
    vfs_memcpy(inode, buffer + index_in_block * sizeof(vectord_inode_t), sizeof(vectord_inode_t));
    return 0;
}

/* Write inode */
static int vectord_write_inode(vectord_private_t *priv, u32 inode_num, const vectord_inode_t *inode) {
    if (inode_num == 0 || inode_num >= priv->superblock.total_inodes) {
        return -1;
    }
    
    u32 inode_index = inode_num - 1;
    u32 inodes_per_block = priv->block_size / sizeof(vectord_inode_t);
    u32 block_num = priv->superblock.first_inode_block + (inode_index / inodes_per_block);
    u32 index_in_block = inode_index % inodes_per_block;
    
    u8 buffer[4096];
    if (vectord_read_block(priv, block_num, buffer) != 0) {
        return -1;
    }
    
    vfs_memcpy(buffer + index_in_block * sizeof(vectord_inode_t), inode, sizeof(vectord_inode_t));
    
    return vectord_write_block(priv, block_num, buffer);
}

/* Get data block number from inode */
static int vectord_get_data_block(vectord_private_t *priv, const vectord_inode_t *inode, u32 block_index) {
    /* Direct blocks */
    if (block_index < VECTORD_DIRECT_BLOCKS) {
        return inode->blocks_ptr.addr.direct[block_index];
    }
    
    /* Single indirect */
    block_index -= VECTORD_DIRECT_BLOCKS;
    if (block_index < priv->block_size / 4) {
        u32 indirect = inode->blocks_ptr.addr.single_indirect;
        if (indirect == 0) return 0;
        
        u32 buffer[1024];
        if (vectord_read_block(priv, indirect, buffer) != 0) return -1;
        return buffer[block_index];
    }
    
    /* Double indirect */
    block_index -= priv->block_size / 4;
    u32 entries_per_block = priv->block_size / 4;
    if (block_index < entries_per_block * entries_per_block) {
        u32 dindirect = inode->blocks_ptr.addr.double_indirect;
        if (dindirect == 0) return 0;
        
        u32 buffer[1024];
        if (vectord_read_block(priv, dindirect, buffer) != 0) return -1;
        
        u32 first = block_index / entries_per_block;
        u32 second = block_index % entries_per_block;
        
        u32 indirect = buffer[first];
        if (indirect == 0) return 0;
        
        if (vectord_read_block(priv, indirect, buffer) != 0) return -1;
        return buffer[second];
    }
    
    return -1;
}

/* Read data from inode */
static int vectord_read_inode_data(vectord_private_t *priv, const vectord_inode_t *inode,
                                   u32 offset, u32 size, void *buffer) {
    u8 *buf = (u8 *)buffer;
    u32 bytes_read = 0;
    u32 file_size = inode->size;
    
    if (offset >= file_size) return 0;
    if (offset + size > file_size) {
        size = file_size - offset;
    }
    
    u32 block_num = offset / priv->block_size;
    u32 block_offset = offset % priv->block_size;
    
    u8 block_buffer[4096];
    
    while (bytes_read < size) {
        int data_block = vectord_get_data_block(priv, inode, block_num);
        if (data_block <= 0) break;
        
        if (vectord_read_block(priv, data_block, block_buffer) != 0) break;
        
        u32 chunk = priv->block_size - block_offset;
        if (chunk > size - bytes_read) {
            chunk = size - bytes_read;
        }
        
        vfs_memcpy(buf + bytes_read, block_buffer + block_offset, chunk);
        bytes_read += chunk;
        
        block_num++;
        block_offset = 0;
    }
    
    return bytes_read;
}

/* Write data to inode */
static int vectord_write_inode_data(vectord_private_t *priv, vectord_inode_t *inode,
                                    u32 offset, u32 size, const void *buffer) {
    /* Simplified - direct blocks only for now */
    const u8 *buf = (const u8 *)buffer;
    u32 bytes_written = 0;
    
    u32 block_num = offset / priv->block_size;
    u32 block_offset = offset % priv->block_size;
    
    while (bytes_written < size && block_num < VECTORD_DIRECT_BLOCKS) {
        /* Allocate block if needed */
        if (inode->blocks_ptr.addr.direct[block_num] == 0) {
            /* Find free block (simplified - just use next available) */
            static u32 next_free_block = 100;  /* Start after superblock and inode tables */
            inode->blocks_ptr.addr.direct[block_num] = next_free_block++;
            inode->blocks++;
        }
        
        u32 data_block = inode->blocks_ptr.addr.direct[block_num];
        u8 block_buffer[4096];
        
        /* Read existing block if not at offset 0 */
        if (block_offset > 0) {
            vectord_read_block(priv, data_block, block_buffer);
        } else {
            vfs_memset(block_buffer, 0, priv->block_size);
        }
        
        u32 chunk = priv->block_size - block_offset;
        if (chunk > size - bytes_written) {
            chunk = size - bytes_written;
        }
        
        vfs_memcpy(block_buffer + block_offset, buf + bytes_written, chunk);
        vectord_write_block(priv, data_block, block_buffer);
        
        bytes_written += chunk;
        block_num++;
        block_offset = 0;
    }
    
    return bytes_written;
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS DRIVER INTERFACE - Lookup
 * ═══════════════════════════════════════════════════════════════════════ */

static int vectord_lookup(vfs_mount_t *mount, vfs_node_t *parent,
                          const char *name, vfs_node_t **result) {
    vectord_private_t *priv = (vectord_private_t *)mount->fs_private;
    if (!priv) return -1;
    
    /* Read parent inode */
    vectord_inode_t parent_inode;
    if (vectord_read_inode(priv, parent->inode, &parent_inode) != 0) {
        return -1;
    }
    
    /* Must be a directory */
    if ((parent_inode.mode & VECTORD_S_IFMT) != VECTORD_S_IFDIR) {
        return -1;
    }
    
    /* Scan directory entries */
    u32 dir_size = parent_inode.size;
    u32 offset = 0;
    u8 dir_buffer[4096];
    u32 buffer_valid = 0;
    u32 buffer_block = 0xFFFFFFFF;
    
    while (offset < dir_size) {
        u32 block_num = offset / priv->block_size;
        
        /* Read new block if needed */
        if (block_num != buffer_block) {
            int data_block = vectord_get_data_block(priv, &parent_inode, block_num);
            if (data_block <= 0) break;
            if (vectord_read_block(priv, data_block, dir_buffer) != 0) break;
            buffer_block = block_num;
            buffer_valid = 1;
        }
        
        if (!buffer_valid) break;
        
        u32 block_offset = offset % priv->block_size;
        vectord_dirent_t *dirent = (vectord_dirent_t *)(dir_buffer + block_offset);
        
        if (dirent->inode == 0) {
            offset += dirent->rec_len;
            continue;
        }
        
        /* Compare name */
        int name_len = vfs_strlen(name);
        if (dirent->name_len == name_len) {
            int match = 1;
            for (int i = 0; i < name_len; i++) {
                if (dirent->name[i] != name[i]) {
                    match = 0;
                    break;
                }
            }
            
            if (match) {
                /* Found! Create VFS node */
                vfs_node_t *node = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
                if (!node) return -1;
                
                vfs_memset(node, 0, sizeof(vfs_node_t));
                
                /* Copy name */
                int copy_len = dirent->name_len;
                if (copy_len > MAX_NAME) copy_len = MAX_NAME;
                vfs_memcpy(node->name, dirent->name, copy_len);
                node->name[copy_len] = '\0';
                
                node->inode = dirent->inode;
                node->mount = mount;
                node->refcount = 1;
                
                /* Read inode for more info */
                vectord_inode_t inode;
                if (vectord_read_inode(priv, dirent->inode, &inode) == 0) {
                    node->size = inode.size;
                    node->uid = inode.uid;
                    node->gid = inode.gid;
                    node->atime = inode.atime;
                    node->mtime = inode.mtime;
                    node->ctime = inode.ctime;
                    node->mode = inode.mode;
                    
                    if ((inode.mode & VECTORD_S_IFMT) == VECTORD_S_IFDIR) {
                        node->type = FILE_TYPE_DIRECTORY;
                    } else if ((inode.mode & VECTORD_S_IFMT) == VECTORD_S_IFLNK) {
                        node->type = FILE_TYPE_SYMLINK;
                    } else {
                        node->type = FILE_TYPE_REGULAR;
                    }
                }
                
                *result = node;
                return 0;
            }
        }
        
        offset += dirent->rec_len;
    }
    
    return -1;
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS DRIVER INTERFACE - Read
 * ═══════════════════════════════════════════════════════════════════════ */

static int vectord_read(vfs_mount_t *mount, vfs_node_t *node,
                        u32 offset, u32 size, void *buffer) {
    vectord_private_t *priv = (vectord_private_t *)mount->fs_private;
    if (!priv) return -1;
    
    vectord_inode_t inode;
    if (vectord_read_inode(priv, node->inode, &inode) != 0) {
        return -1;
    }
    
    return vectord_read_inode_data(priv, &inode, offset, size, buffer);
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS DRIVER INTERFACE - Write
 * ═══════════════════════════════════════════════════════════════════════ */

static int vectord_write(vfs_mount_t *mount, vfs_node_t *node,
                         u32 offset, u32 size, const void *buffer) {
    vectord_private_t *priv = (vectord_private_t *)mount->fs_private;
    if (!priv) return -1;
    
    vectord_inode_t inode;
    if (vectord_read_inode(priv, node->inode, &inode) != 0) {
        return -1;
    }
    
    int written = vectord_write_inode_data(priv, &inode, offset, size, buffer);
    
    if (written > 0) {
        if (offset + written > inode.size) {
            inode.size = offset + written;
        }
        inode.mtime = 0;  /* TODO: Get current time */
        vectord_write_inode(priv, node->inode, &inode);
    }
    
    return written;
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS DRIVER INTERFACE - Read Directory
 * ═══════════════════════════════════════════════════════════════════════ */

static int vectord_read_dir(vfs_mount_t *mount, vfs_node_t *node,
                            u32 index, dirent_t *entry) {
    vectord_private_t *priv = (vectord_private_t *)mount->fs_private;
    if (!priv) return -1;
    
    vectord_inode_t inode;
    if (vectord_read_inode(priv, node->inode, &inode) != 0) {
        return -1;
    }
    
    u32 dir_size = inode.size;
    u32 offset = 0;
    u32 current_index = 0;
    u8 dir_buffer[4096];
    u32 buffer_block = 0xFFFFFFFF;
    
    while (offset < dir_size) {
        u32 block_num = offset / priv->block_size;
        
        if (block_num != buffer_block) {
            int data_block = vectord_get_data_block(priv, &inode, block_num);
            if (data_block <= 0) break;
            if (vectord_read_block(priv, data_block, dir_buffer) != 0) break;
            buffer_block = block_num;
        }
        
        u32 block_offset = offset % priv->block_size;
        vectord_dirent_t *dirent = (vectord_dirent_t *)(dir_buffer + block_offset);
        
        if (dirent->inode == 0) {
            offset += dirent->rec_len;
            continue;
        }
        
        if (current_index == index) {
            entry->inode = dirent->inode;
            
            int copy_len = dirent->name_len;
            if (copy_len > MAX_NAME) copy_len = MAX_NAME;
            vfs_memcpy(entry->name, dirent->name, copy_len);
            entry->name[copy_len] = '\0';
            
            entry->type = dirent->type;
            return 0;
        }
        
        current_index++;
        offset += dirent->rec_len;
    }
    
    return -1;
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS DRIVER INTERFACE - Create Node
 * ═══════════════════════════════════════════════════════════════════════ */

static int vectord_create_node(vfs_mount_t *mount, vfs_node_t *parent,
                               const char *name, u8 type, vfs_node_t **result) {
    vectord_private_t *priv = (vectord_private_t *)mount->fs_private;
    if (!priv) return -1;
    
    /* Find free inode */
    u32 free_inode = 0;
    for (u32 i = 1; i < priv->superblock.total_inodes; i++) {
        vectord_inode_t inode;
        if (vectord_read_inode(priv, i, &inode) == 0) {
            if ((inode.flags & VECTORD_INODE_USED_V) == 0) {
                free_inode = i;
                break;
            }
        }
    }
    
    if (free_inode == 0) return -1;
    
    /* Initialize new inode */
    vectord_inode_t new_inode;
    vfs_memset(&new_inode, 0, sizeof(vectord_inode_t));
    new_inode.mode = (type == FILE_TYPE_DIRECTORY) ? VECTORD_S_IFDIR : VECTORD_S_IFREG;
    new_inode.flags = VECTORD_INODE_USED_V;
    if (type == FILE_TYPE_DIRECTORY) {
        new_inode.flags |= VECTORD_INODE_DIR_V;
        new_inode.links_count = 2;  /* . and .. */
    } else {
        new_inode.links_count = 1;
    }
    new_inode.uid = 0;
    new_inode.gid = 0;
    
    if (vectord_write_inode(priv, free_inode, &new_inode) != 0) {
        return -1;
    }
    
    /* Add entry to parent directory */
    vectord_inode_t parent_inode;
    if (vectord_read_inode(priv, parent->inode, &parent_inode) != 0) {
        return -1;
    }
    
    /* Calculate directory entry size */
    int name_len = vfs_strlen(name);
    u16 rec_len = sizeof(vectord_dirent_t) + name_len + 1;
    rec_len = (rec_len + 3) & ~3;  /* Align to 4 bytes */
    
    /* Find space in parent directory */
    u8 dir_buffer[4096];
    u32 dir_offset = 0;
    u32 dir_size = parent_inode.size;
    
    /* Extend directory if needed */
    u32 new_block = 0;
    
    /* Find last block and add entry */
    u32 block_num = dir_size / priv->block_size;
    int data_block = vectord_get_data_block(priv, &parent_inode, block_num);
    
    if (data_block <= 0) {
        /* Allocate new block */
        static u32 next_block = 100;
        data_block = next_block++;
        parent_inode.blocks_ptr.addr.direct[block_num] = data_block;
        parent_inode.blocks++;
        vfs_memset(dir_buffer, 0, priv->block_size);
    } else {
        vectord_read_block(priv, data_block, dir_buffer);
    }
    
    /* Add directory entry */
    u32 block_offset = dir_size % priv->block_size;
    vectord_dirent_t *dirent = (vectord_dirent_t *)(dir_buffer + block_offset);
    dirent->inode = free_inode;
    dirent->rec_len = rec_len;
    dirent->name_len = name_len;
    dirent->type = (type == FILE_TYPE_DIRECTORY) ? VECTORD_DET_DIR : VECTORD_DET_FILE;
    vfs_memcpy(dirent->name, name, name_len);
    dirent->name[name_len] = '\0';
    
    vectord_write_block(priv, data_block, dir_buffer);
    
    /* Update parent inode */
    parent_inode.size += rec_len;
    vectord_write_inode(priv, parent->inode, &parent_inode);
    
    /* Create VFS node */
    vfs_node_t *node = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
    if (!node) return -1;
    
    vfs_memset(node, 0, sizeof(vfs_node_t));
    vfs_strcpy_local(node->name, name);
    node->inode = free_inode;
    node->type = type;
    node->mount = mount;
    node->refcount = 1;
    
    *result = node;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * MOUNT OPERATION
 * ═══════════════════════════════════════════════════════════════════════ */

int vectord_fs_mount(vfs_mount_t *mount, u32 dev_id) {
    /* Find ATA device */
    ata_device_t *device = NULL;
    for (int i = 0; i < ATA_MAX_DEVICES; i++) {
        if (ata_devices[i].present) {
            device = &ata_devices[i];
            break;
        }
    }
    
    if (!device) {
        serial_puts("VECTORD FS: No ATA device found\n");
        return -1;
    }
    
    /* Allocate private data */
    vectord_private_t *priv = (vectord_private_t *)kmalloc(sizeof(vectord_private_t));
    if (!priv) return -1;
    
    vfs_memset(priv, 0, sizeof(vectord_private_t));
    priv->device = device;
    
    /* Read superblock */
    u8 buffer[512];
    if (vectord_read_sector(device, 0, buffer) != 0 ||
        vectord_read_sector(device, 1, buffer + 512) != 0) {
        serial_puts("VECTORD FS: Failed to read superblock\n");
        kfree(priv);
        return -1;
    }
    
    vfs_memcpy(&priv->superblock, buffer, sizeof(vectord_superblock_t));
    
    /* Validate superblock */
    if (priv->superblock.magic != VECTORD_SUPERBLOCK_MAGIC_V) {
        serial_puts("VECTORD FS: Invalid magic number (expected 0x56454354)\n");
        serial_puts("VECTORD FS: Found 0x");
        serial_puthex(priv->superblock.magic);
        serial_puts("\n");
        kfree(priv);
        return -1;
    }
    
    priv->block_size = priv->superblock.block_size;
    if (priv->block_size == 0) priv->block_size = VECTORD_BLOCK_SIZE;
    priv->inodes_per_block = priv->block_size / sizeof(vectord_inode_t);
    
    serial_puts("VECTORD FS: Mounted successfully\n");
    serial_puts("  Magic: 0x");
    serial_puthex(priv->superblock.magic);
    serial_puts(" ('VECT')\n");
    serial_puts("  Block size: ");
    serial_putdec(priv->block_size);
    serial_puts(" bytes\n");
    serial_puts("  Total inodes: ");
    serial_putdec(priv->superblock.total_inodes);
    serial_puts("\n  Total blocks: ");
    serial_putdec(priv->superblock.total_blocks);
    serial_puts("\n  Free inodes: ");
    serial_putdec(priv->superblock.free_inodes);
    serial_puts("\n  Free blocks: ");
    serial_putdec(priv->superblock.free_blocks);
    serial_puts("\n  Root inode: ");
    serial_putdec(priv->superblock.root_inode);
    serial_puts("\n");
    
    mount->fs_private = priv;
    
    /* Create root node */
    vfs_node_t *root = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
    if (!root) {
        kfree(priv);
        return -1;
    }
    
    vfs_memset(root, 0, sizeof(vfs_node_t));
    root->name[0] = '/';
    root->name[1] = '\0';
    root->inode = priv->superblock.root_inode;
    root->type = FILE_TYPE_DIRECTORY;
    root->mount = mount;
    root->refcount = 1;
    
    /* Read root inode */
    vectord_inode_t root_inode;
    if (vectord_read_inode(priv, root->inode, &root_inode) == 0) {
        root->size = root_inode.size;
        root->mode = root_inode.mode;
    }
    
    mount->root = root;
    
    return 0;
}

int vectord_fs_unmount(vfs_mount_t *mount) {
    if (mount->fs_private) {
        kfree(mount->fs_private);
        mount->fs_private = NULL;
    }
    
    if (mount->root) {
        kfree(mount->root);
        mount->root = NULL;
    }
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * FORMATTING
 * ═══════════════════════════════════════════════════════════════════════ */

int vectord_fs_format(ata_device_t *device, const char *label) {
    if (!device) return -1;
    
    vectord_superblock_t sb;
    vfs_memset(&sb, 0, sizeof(vectord_superblock_t));
    
    sb.magic = VECTORD_SUPERBLOCK_MAGIC_V;
    sb.version = VECTORD_FS_VERSION;
    sb.block_size = VECTORD_BLOCK_SIZE;
    sb.total_blocks = device->sectors / (VECTORD_BLOCK_SIZE / 512);
    sb.free_blocks = sb.total_blocks - 10;  /* Reserve first 10 blocks */
    sb.total_inodes = sb.total_blocks * 2;  /* 2 inodes per block */
    sb.free_inodes = sb.total_inodes - 1;   /* Root inode used */
    sb.first_inode_block = 1;
    sb.first_data_block = 10;
    sb.inode_size = sizeof(vectord_inode_t);
    sb.root_inode = 1;
    sb.flags = 0;
    sb.checksum = vectord_calc_checksum(&sb, sizeof(sb) - 4);
    
    if (label) {
        int i;
        for (i = 0; i < 31 && label[i]; i++) {
            sb.label[i] = label[i];
        }
        sb.label[i] = '\0';
    } else {
        vfs_strcpy_local(sb.label, "VECTORD");
    }
    
    /* Write superblock */
    u8 buffer[512];
    vfs_memset(buffer, 0, 512);
    vfs_memcpy(buffer, &sb, sizeof(sb));
    
    if (ata_write_sectors(device, 0, 1, (u16 *)buffer) != 0) {
        return -1;
    }
    
    if (ata_write_sectors(device, 1, 1, (u16 *)(buffer + 512)) != 0) {
        /* Continue anyway */
    }
    
    /* Initialize inode table */
    vfs_memset(buffer, 0, 512);
    vectord_inode_t root_inode;
    vfs_memset(&root_inode, 0, sizeof(vectord_inode_t));
    root_inode.mode = VECTORD_S_IFDIR;
    root_inode.flags = VECTORD_INODE_USED_V | VECTORD_INODE_DIR_V;
    root_inode.links_count = 2;
    
    /* Write root inode */
    vfs_memcpy(buffer, &root_inode, sizeof(vectord_inode_t));
    
    for (int i = 1; i <= 10; i++) {
        ata_write_sectors(device, i, 1, (u16 *)buffer);
    }
    
    serial_puts("VECTORD FS: Formatted successfully\n");
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD FS DRIVER STRUCTURE
 * ═══════════════════════════════════════════════════════════════════════ */

static fs_driver_t vectord_fs_driver = {
    .name = "vectord",
    .type = FS_TYPE_VECTORD,
    .mount = vectord_fs_mount,
    .unmount = vectord_fs_unmount,
    .read_node = NULL,
    .write_node = NULL,
    .create_node = vectord_create_node,
    .delete_node = NULL,
    .read = vectord_read,
    .write = vectord_write,
    .truncate = NULL,
    .read_dir = vectord_read_dir,
    .lookup = vectord_lookup
};

/* ═══════════════════════════════════════════════════════════════════════
 * INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════ */

void vectord_fs_init(void) {
    vfs_register_fs(&vectord_fs_driver);
    serial_puts("VECTORD FS: Driver initialized\n");
    serial_puts("  Magic: 0x56454354 ('VECT')\n");
    serial_puts("  Inode size: ");
    serial_putdec(sizeof(vectord_inode_t));
    serial_puts(" bytes\n");
}

