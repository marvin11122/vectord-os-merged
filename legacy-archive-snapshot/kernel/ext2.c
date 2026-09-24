/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - ext2 Filesystem Driver (Read-Only)
 * Basic ext2 read support for compatibility with existing disks
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════
 * ext2 SUPERBLOCK STRUCTURE
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    u32 s_inodes_count;         /* Total inodes */
    u32 s_blocks_count;         /* Total blocks */
    u32 s_r_blocks_count;       /* Reserved blocks */
    u32 s_free_blocks_count;    /* Free blocks */
    u32 s_free_inodes_count;    /* Free inodes */
    u32 s_first_data_block;     /* First data block */
    u32 s_log_block_size;       /* Block size (1024 << s_log_block_size) */
    u32 s_log_frag_size;        /* Fragment size */
    u32 s_blocks_per_group;     /* Blocks per group */
    u32 s_frags_per_group;      /* Fragments per group */
    u32 s_inodes_per_group;     /* Inodes per group */
    u32 s_mtime;                /* Last mount time */
    u32 s_wtime;                /* Last write time */
    u16 s_mnt_count;            /* Mount count */
    u16 s_max_mnt_count;        /* Max mounts before check */
    u16 s_magic;                /* Magic number (0xEF53) */
    u16 s_state;                /* Filesystem state */
    u16 s_errors;               /* Error handling */
    u16 s_minor_rev_level;      /* Minor revision level */
    u32 s_lastcheck;            /* Last check time */
    u32 s_checkinterval;        /* Check interval */
    u32 s_creator_os;           /* Creator OS */
    u32 s_rev_level;            /* Revision level */
    u16 s_def_resuid;           /* Default reserved UID */
    u16 s_def_resgid;           /* Default reserved GID */
    u32 s_first_inode;          /* First non-reserved inode */
    u16 s_inode_size;           /* Inode size */
    u16 s_block_group_nr;       /* Block group number */
    u32 s_feature_compat;       /* Compatible features */
    u32 s_feature_incompat;     /* Incompatible features */
    u32 s_feature_ro_compat;    /* Read-only compatible features */
    u8  s_uuid[16];             /* Volume UUID */
    char s_volume_name[16];     /* Volume name */
    char s_last_mounted[64];    /* Last mount point */
    u32 s_algorithm_usage_bitmap; /* Compression algorithms */
} ext2_superblock_t;

/* ═══════════════════════════════════════════════════════════════════════
 * ext2 BLOCK GROUP DESCRIPTOR
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    u32 bg_block_bitmap;        /* Blocks bitmap block */
    u32 bg_inode_bitmap;        /* Inodes bitmap block */
    u32 bg_inode_table;         /* Inodes table block */
    u16 bg_free_blocks_count;   /* Free blocks count */
    u16 bg_free_inodes_count;   /* Free inodes count */
    u16 bg_used_dirs_count;     /* Used directories count */
    u16 bg_pad;                 /* Padding */
    u8  bg_reserved[12];        /* Reserved */
} ext2_group_desc_t;

/* ═══════════════════════════════════════════════════════════════════════
 * ext2 INODE STRUCTURE
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    u16 i_mode;         /* File mode */
    u16 i_uid;          /* Owner UID */
    u32 i_size;         /* Size in bytes */
    u32 i_atime;        /* Access time */
    u32 i_ctime;        /* Creation time */
    u32 i_mtime;        /* Modification time */
    u32 i_dtime;        /* Deletion time */
    u16 i_gid;          /* Group ID */
    u16 i_links_count;  /* Links count */
    u32 i_blocks;       /* Blocks count */
    u32 i_flags;        /* File flags */
    u32 i_osd1;         /* OS-specific 1 */
    u32 i_block[15];    /* Block pointers */
    u32 i_generation;   /* File version */
    u32 i_file_acl;     /* File ACL */
    u32 i_dir_acl;      /* Directory ACL */
    u32 i_faddr;        /* Fragment address */
    u8  osd2[12];       /* OS-specific 2 */
} ext2_inode_t;

/* ═══════════════════════════════════════════════════════════════════════
 * ext2 DIRECTORY ENTRY
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    u32 inode;          /* Inode number */
    u16 rec_len;        /* Directory entry length */
    u8  name_len;       /* Name length */
    u8  file_type;      /* File type */
    char name[255];     /* File name */
} ext2_dirent_t;

/* ═══════════════════════════════════════════════════════════════════════
 * ext2 PRIVATE DATA
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    ata_device_t *device;
    ext2_superblock_t superblock;
    ext2_group_desc_t *group_descs;
    u32 block_size;
    u32 groups_count;
    u32 inodes_per_group;
    u32 first_inode_block;
} ext2_private_t;

/* ═══════════════════════════════════════════════════════════════════════
 * INTERNAL HELPERS
 * ═══════════════════════════════════════════════════════════════════════ */

static void ext2_memset(void *s, int c, u32 n) {
    u8 *p = (u8 *)s;
    while (n--) *p++ = (u8)c;
}

static void ext2_memcpy(void *dest, const void *src, u32 n) {
    u8 *d = (u8 *)dest;
    const u8 *s = (const u8 *)src;
    while (n--) *d++ = *s++;
}

static int ext2_strlen(const char *s) {
    int len = 0;
    while (s && s[len]) len++;
    return len;
}

/* Read sector using ATA driver */
static int ext2_read_sector(ata_device_t *dev, u32 lba, void *buffer) {
    return ata_read_sectors(dev, lba, 1, (u16 *)buffer);
}

/* Read block from filesystem */
static int ext2_read_block(ext2_private_t *priv, u32 block_num, void *buffer) {
    u32 sectors_per_block = priv->block_size / 512;
    u32 start_sector = block_num * sectors_per_block;
    
    for (u32 i = 0; i < sectors_per_block; i++) {
        if (ext2_read_sector(priv->device, start_sector + i, 
                            (u8 *)buffer + i * 512) != 0) {
            return -1;
        }
    }
    
    return 0;
}

/* Get block group descriptor */
static int ext2_get_group_desc(ext2_private_t *priv, u32 group_num, ext2_group_desc_t *desc) {
    if (group_num >= priv->groups_count) return -1;
    
    /* Group descriptors start at block 2 (or first_data_block + 1) */
    u32 gdt_block = priv->superblock.s_first_data_block + 1;
    u32 gdt_offset = group_num * sizeof(ext2_group_desc_t);
    u32 gdt_index = gdt_offset / priv->block_size;
    u32 gdt_pos = gdt_offset % priv->block_size;
    
    u8 buffer[4096];  /* Max block size */
    if (ext2_read_block(priv, gdt_block + gdt_index, buffer) != 0) {
        return -1;
    }
    
    ext2_memcpy(desc, buffer + gdt_pos, sizeof(ext2_group_desc_t));
    return 0;
}

/* Read inode from disk */
static int ext2_read_inode(ext2_private_t *priv, u32 inode_num, ext2_inode_t *inode) {
    if (inode_num == 0) return -1;
    
    u32 group = (inode_num - 1) / priv->inodes_per_group;
    u32 index = (inode_num - 1) % priv->inodes_per_group;
    
    ext2_group_desc_t desc;
    if (ext2_get_group_desc(priv, group, &desc) != 0) {
        return -1;
    }
    
    /* Calculate inode location */
    u32 inode_table_block = desc.bg_inode_table;
    u32 offset_in_table = index * priv->superblock.s_inode_size;
    u32 block_num = inode_table_block + (offset_in_table / priv->block_size);
    u32 block_offset = offset_in_table % priv->block_size;
    
    u8 buffer[4096];
    if (ext2_read_block(priv, block_num, buffer) != 0) {
        return -1;
    }
    
    ext2_memcpy(inode, buffer + block_offset, sizeof(ext2_inode_t));
    return 0;
}

/* Get block number from inode */
static int ext2_get_data_block(ext2_private_t *priv, ext2_inode_t *inode, u32 block_index) {
    /* Direct blocks */
    if (block_index < 12) {
        return inode->i_block[block_index];
    }
    
    /* Single indirect */
    block_index -= 12;
    if (block_index < priv->block_size / 4) {
        u32 indirect_block = inode->i_block[12];
        if (indirect_block == 0) return 0;
        
        u32 buffer[1024];
        if (ext2_read_block(priv, indirect_block, buffer) != 0) {
            return -1;
        }
        return buffer[block_index];
    }
    
    /* Double indirect - simplified */
    block_index -= priv->block_size / 4;
    if (block_index < (priv->block_size / 4) * (priv->block_size / 4)) {
        u32 dindirect_block = inode->i_block[13];
        if (dindirect_block == 0) return 0;
        
        u32 entries_per_block = priv->block_size / 4;
        u32 first_level = block_index / entries_per_block;
        u32 second_level = block_index % entries_per_block;
        
        u32 buffer[1024];
        if (ext2_read_block(priv, dindirect_block, buffer) != 0) {
            return -1;
        }
        
        u32 indirect_block = buffer[first_level];
        if (indirect_block == 0) return 0;
        
        if (ext2_read_block(priv, indirect_block, buffer) != 0) {
            return -1;
        }
        
        return buffer[second_level];
    }
    
    return -1;  /* Out of range */
}

/* Read data from inode */
static int ext2_read_inode_data(ext2_private_t *priv, ext2_inode_t *inode, 
                                 u32 offset, u32 size, void *buffer) {
    u8 *buf = (u8 *)buffer;
    u32 bytes_read = 0;
    u32 file_size = inode->i_size;
    
    if (offset >= file_size) return 0;
    if (offset + size > file_size) {
        size = file_size - offset;
    }
    
    u32 block_num = offset / priv->block_size;
    u32 block_offset = offset % priv->block_size;
    
    u8 block_buffer[4096];
    
    while (bytes_read < size) {
        int data_block = ext2_get_data_block(priv, inode, block_num);
        if (data_block <= 0) break;
        
        if (ext2_read_block(priv, data_block, block_buffer) != 0) {
            break;
        }
        
        u32 chunk = priv->block_size - block_offset;
        if (chunk > size - bytes_read) {
            chunk = size - bytes_read;
        }
        
        ext2_memcpy(buf + bytes_read, block_buffer + block_offset, chunk);
        bytes_read += chunk;
        
        block_num++;
        block_offset = 0;
    }
    
    return bytes_read;
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS DRIVER INTERFACE - Lookup
 * ═══════════════════════════════════════════════════════════════════════ */

static int ext2_lookup(vfs_mount_t *mount, vfs_node_t *parent, 
                       const char *name, vfs_node_t **result) {
    ext2_private_t *priv = (ext2_private_t *)mount->fs_private;
    if (!priv) return -1;
    
    /* Get parent inode */
    ext2_inode_t parent_inode;
    if (ext2_read_inode(priv, parent->inode, &parent_inode) != 0) {
        return -1;
    }
    
    /* Check if parent is a directory */
    if (!(parent_inode.i_mode & EXT2_S_IFDIR)) {
        return -1;
    }
    
    /* Read directory entries */
    u32 dir_size = parent_inode.i_size;
    u32 offset = 0;
    u8 dir_buffer[4096];
    
    while (offset < dir_size) {
        u32 block_num = offset / priv->block_size;
        u32 block_offset = offset % priv->block_size;
        
        int data_block = ext2_get_data_block(priv, &parent_inode, block_num);
        if (data_block <= 0) break;
        
        if (ext2_read_block(priv, data_block, dir_buffer) != 0) {
            break;
        }
        
        /* Parse directory entries in this block */
        while (block_offset < priv->block_size && offset < dir_size) {
            ext2_dirent_t *dirent = (ext2_dirent_t *)(dir_buffer + block_offset);
            
            if (dirent->inode == 0) {
                block_offset += dirent->rec_len;
                offset += dirent->rec_len;
                continue;
            }
            
            /* Compare name */
            int name_len = ext2_strlen(name);
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
                    
                    ext2_memset(node, 0, sizeof(vfs_node_t));
                    
                    /* Copy name */
                    int copy_len = dirent->name_len;
                    if (copy_len > MAX_NAME) copy_len = MAX_NAME;
                    ext2_memcpy(node->name, dirent->name, copy_len);
                    node->name[copy_len] = '\0';
                    
                    node->inode = dirent->inode;
                    node->mount = mount;
                    node->refcount = 1;
                    
                    /* Read inode to get more info */
                    ext2_inode_t inode;
                    if (ext2_read_inode(priv, dirent->inode, &inode) == 0) {
                        node->size = inode.i_size;
                        node->uid = inode.i_uid;
                        node->gid = inode.i_gid;
                        node->atime = inode.i_atime;
                        node->mtime = inode.i_mtime;
                        node->ctime = inode.i_ctime;
                        node->mode = inode.i_mode;
                        
                        /* Determine file type */
                        if (inode.i_mode & EXT2_S_IFDIR) {
                            node->type = FILE_TYPE_DIRECTORY;
                        } else if (inode.i_mode & EXT2_S_IFLNK) {
                            node->type = FILE_TYPE_SYMLINK;
                        } else {
                            node->type = FILE_TYPE_REGULAR;
                        }
                    }
                    
                    *result = node;
                    return 0;
                }
            }
            
            block_offset += dirent->rec_len;
            offset += dirent->rec_len;
        }
    }
    
    return -1;  /* Not found */
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS DRIVER INTERFACE - Read
 * ═══════════════════════════════════════════════════════════════════════ */

static int ext2_read(vfs_mount_t *mount, vfs_node_t *node, 
                     u32 offset, u32 size, void *buffer) {
    ext2_private_t *priv = (ext2_private_t *)mount->fs_private;
    if (!priv) return -1;
    
    ext2_inode_t inode;
    if (ext2_read_inode(priv, node->inode, &inode) != 0) {
        return -1;
    }
    
    return ext2_read_inode_data(priv, &inode, offset, size, buffer);
}

/* ═══════════════════════════════════════════════════════════════════════
 * VFS DRIVER INTERFACE - Read Directory
 * ═══════════════════════════════════════════════════════════════════════ */

static int ext2_read_dir(vfs_mount_t *mount, vfs_node_t *node, 
                         u32 index, dirent_t *entry) {
    ext2_private_t *priv = (ext2_private_t *)mount->fs_private;
    if (!priv) return -1;
    
    ext2_inode_t inode;
    if (ext2_read_inode(priv, node->inode, &inode) != 0) {
        return -1;
    }
    
    /* Read directory entries */
    u32 dir_size = inode.i_size;
    u32 offset = 0;
    u32 current_index = 0;
    u8 dir_buffer[4096];
    
    while (offset < dir_size) {
        u32 block_num = offset / priv->block_size;
        u32 block_offset = offset % priv->block_size;
        
        int data_block = ext2_get_data_block(priv, &inode, block_num);
        if (data_block <= 0) break;
        
        if (ext2_read_block(priv, data_block, dir_buffer) != 0) {
            break;
        }
        
        while (block_offset < priv->block_size && offset < dir_size) {
            ext2_dirent_t *dirent = (ext2_dirent_t *)(dir_buffer + block_offset);
            
            if (dirent->inode == 0) {
                block_offset += dirent->rec_len;
                offset += dirent->rec_len;
                continue;
            }
            
            if (current_index == index) {
                /* Found requested entry */
                entry->inode = dirent->inode;
                
                int copy_len = dirent->name_len;
                if (copy_len > MAX_NAME) copy_len = MAX_NAME;
                ext2_memcpy(entry->name, dirent->name, copy_len);
                entry->name[copy_len] = '\0';
                
                /* Read inode to get type */
                ext2_inode_t ent_inode;
                if (ext2_read_inode(priv, dirent->inode, &ent_inode) == 0) {
                    if (ent_inode.i_mode & EXT2_S_IFDIR) {
                        entry->type = FILE_TYPE_DIRECTORY;
                    } else if (ent_inode.i_mode & EXT2_S_IFLNK) {
                        entry->type = FILE_TYPE_SYMLINK;
                    } else {
                        entry->type = FILE_TYPE_REGULAR;
                    }
                }
                
                return 0;
            }
            
            current_index++;
            block_offset += dirent->rec_len;
            offset += dirent->rec_len;
        }
    }
    
    return -1;  /* Index out of range */
}

/* ═══════════════════════════════════════════════════════════════════════
 * MOUNT OPERATION
 * ═══════════════════════════════════════════════════════════════════════ */

int ext2_mount(vfs_mount_t *mount, u32 dev_id) {
    /* Find ATA device */
    ata_device_t *device = NULL;
    for (int i = 0; i < ATA_MAX_DEVICES; i++) {
        if (ata_devices[i].present) {
            device = &ata_devices[i];
            break;
        }
    }
    
    if (!device) {
        serial_puts("ext2: No ATA device found\n");
        return -1;
    }
    
    /* Allocate private data */
    ext2_private_t *priv = (ext2_private_t *)kmalloc(sizeof(ext2_private_t));
    if (!priv) {
        return -1;
    }
    
    ext2_memset(priv, 0, sizeof(ext2_private_t));
    priv->device = device;
    
    /* Read superblock (at sector 2, offset 1024) */
    u8 buffer[512];
    if (ext2_read_sector(device, 2, buffer) != 0 ||
        ext2_read_sector(device, 3, buffer + 512) != 0) {
        serial_puts("ext2: Failed to read superblock\n");
        kfree(priv);
        return -1;
    }
    
    ext2_memcpy(&priv->superblock, buffer + (1024 - 1024), sizeof(ext2_superblock_t));
    
    /* Validate superblock */
    if (priv->superblock.s_magic != EXT2_SUPERBLOCK_MAGIC) {
        serial_puts("ext2: Invalid magic number\n");
        kfree(priv);
        return -1;
    }
    
    /* Calculate parameters */
    priv->block_size = 1024 << priv->superblock.s_log_block_size;
    priv->inodes_per_group = priv->superblock.s_inodes_per_group;
    priv->groups_count = (priv->superblock.s_blocks_count + 
                          priv->superblock.s_blocks_per_group - 1) /
                         priv->superblock.s_blocks_per_group;
    
    serial_puts("ext2: Mounted successfully\n");
    serial_puts("  Block size: ");
    serial_putdec(priv->block_size);
    serial_puts(" bytes\n");
    serial_puts("  Inodes: ");
    serial_putdec(priv->superblock.s_inodes_count);
    serial_puts("\n  Blocks: ");
    serial_putdec(priv->superblock.s_blocks_count);
    serial_puts("\n  Groups: ");
    serial_putdec(priv->groups_count);
    serial_puts("\n");
    
    /* Store private data */
    mount->fs_private = priv;
    
    /* Create root node */
    vfs_node_t *root = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
    if (!root) {
        kfree(priv);
        return -1;
    }
    
    ext2_memset(root, 0, sizeof(vfs_node_t));
    root->name[0] = '/';
    root->name[1] = '\0';
    root->inode = 2;  /* ext2 root inode is always 2 */
    root->type = FILE_TYPE_DIRECTORY;
    root->mount = mount;
    root->refcount = 1;
    
    /* Read root inode */
    ext2_inode_t root_inode;
    if (ext2_read_inode(priv, 2, &root_inode) == 0) {
        root->size = root_inode.i_size;
        root->mode = root_inode.i_mode;
    }
    
    mount->root = root;
    
    return 0;
}

int ext2_unmount(vfs_mount_t *mount) {
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
 * ext2 DRIVER STRUCTURE
 * ═══════════════════════════════════════════════════════════════════════ */

static fs_driver_t ext2_driver = {
    .name = "ext2",
    .type = FS_TYPE_EXT2,
    .mount = ext2_mount,
    .unmount = ext2_unmount,
    .read_node = NULL,
    .write_node = NULL,
    .create_node = NULL,
    .delete_node = NULL,
    .read = ext2_read,
    .write = NULL,
    .truncate = NULL,
    .read_dir = ext2_read_dir,
    .lookup = ext2_lookup
};

/* ═══════════════════════════════════════════════════════════════════════
 * INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════ */

void ext2_init(void) {
    vfs_register_fs(&ext2_driver);
    serial_puts("ext2: Driver initialized (read-only)\n");
}
