#include <filesystem/fat32.h>
#include <kernel/memory_manager.h>
#include <kernel/initcall.h>

#define JLOS_KERNEL_LOG_SUBSYS "fat32"
#include <kernel/printk.h>

static jlos_fat32_sb_info_t *fat32_sbi(jlos_vfs_super_block_t *sb)
{
    return (jlos_fat32_sb_info_t *)sb->fs_private;
}

static jlos_fat32_inode_info_t *fat32_ii(jlos_vfs_inode_t *inode)
{
    return (jlos_fat32_inode_info_t *)inode->fs_private;
}

static uint32_t fat32_cluster_to_sector(jlos_fat32_sb_info_t *sbi, uint32_t cluster)
{
    return sbi->data_start + sbi->sectors_per_cluster * (cluster - JLOS_FAT32_FIRST_DATA_CLUSTER);
}

static uint32_t fat32_get_next_cluster(jlos_vfs_super_block_t *sb, uint32_t cluster)
{
    jlos_fat32_sb_info_t *sbi = fat32_sbi(sb);
    uint32_t fat_offset = cluster * sizeof(uint32_t);
    uint32_t fat_sector = sbi->fat_start + fat_offset / sbi->bpb.bytes_per_sector;
    uint32_t entry_offset = fat_offset % sbi->bpb.bytes_per_sector;
    
    uint8_t buf[JLOS_FAT32_BYTES_PER_SECTOR];
    if (jlos_hal_block_read(sb->block_dev, fat_sector, buf, 1) != 0) {
        return JLOS_FAT32_CLUSTER_EOC;
    }
    uint32_t next = *(uint32_t *)(buf + entry_offset);
    return next & JLOS_FAT32_CLUSTER_MASK;
}

static int fat32_read_cluster(jlos_vfs_super_block_t *sb, uint32_t cluster, uint8_t *buf)
{
    jlos_fat32_sb_info_t *sbi = fat32_sbi(sb);
    uint32_t sector = fat32_cluster_to_sector(sbi, cluster);
    return jlos_hal_block_read(sb->block_dev, sector, buf, sbi->sectors_per_cluster);
}

static void fat32_name_to_str(const jlos_fat32_dirent_t *de, char *buf, size_t bufsize)
{
    size_t pos = 0;
    bool has_ext = false;
    bool name_lower = (de->reserved & 0x08) != 0;
    bool ext_lower = (de->reserved & 0x10) != 0;
    for (int i = 0; i < JLOS_FAT32_SHORT_NAME_LEN && pos < bufsize - 1; i++) {
        if (de->name[i] != ' ') {
            char c = (char)de->name[i];
            if (name_lower && c >= 'A' && c <= 'Z') {
                c += 'a' - 'A';
            }
            buf[pos++] = c;
        }
    }
    for (int i = 0; i < JLOS_FAT32_SHORT_EXT_LEN; i++) {
        if (de->ext[i] != ' ') {
            has_ext = true;
            break;
        }
    }
    if (has_ext) {
        if (pos < bufsize - 1) {
            buf[pos++] = '.';
        }
        for (int i = 0; i < JLOS_FAT32_SHORT_EXT_LEN && pos < bufsize - 1; i++) {
            if (de->ext[i] != ' ') {
                char c = (char)de->ext[i];
                if (ext_lower && c >= 'A' && c <= 'Z') {
                    c += 'a' - 'A';
                }
                buf[pos++] = c;
            }
        }
    }
    buf[pos] = 0;
}

static jlos_vfs_inode_t *fat32_alloc_inode(jlos_vfs_super_block_t *sb)
{
    (void)sb;
    jlos_vfs_inode_t *inode = jlos_kalloc(sizeof(jlos_vfs_inode_t));
    if (!inode) {
        return NULL;
    }
    jlos_memset(inode, 0, sizeof(*inode));
    jlos_fat32_inode_info_t *fi = jlos_kalloc(sizeof(jlos_fat32_inode_info_t));
    if (!fi) {
        jlos_kfree(inode);
        return NULL;
    }
    jlos_memset(fi, 0, sizeof(*fi));
    inode->fs_private = fi;
    return inode;
}

static void fat32_destroy_inode(jlos_vfs_inode_t *inode)
{
    if (inode->fs_private) {
        jlos_kfree(inode->fs_private);
    }
    jlos_kfree(inode);
}

static const jlos_vfs_inode_ops_t s_fat32_inode_ops;
static const jlos_vfs_file_ops_t  s_fat32_file_ops;

static bool fat32_dirent_should_skip(const jlos_fat32_dirent_t *de)
{
    if (de->name[0] == JLOS_FAT32_DIRENT_DELETED) {
        return true;
    }
    if ((de->attributes & JLOS_FAT32_ATTR_LFN) == JLOS_FAT32_ATTR_LFN) {
        return true;
    }
    return false;
}

static jlos_vfs_inode_t *fat32_dirent_to_inode(jlos_vfs_super_block_t *sb, const jlos_fat32_dirent_t *de)
{
    uint32_t first_cluster = ((uint32_t)de->first_cluster_hi << 16) | de->first_cluster_low;
    jlos_vfs_inode_t *inode = jlos_vfs_inode_alloc(sb, first_cluster);
    if (!inode) {
        return NULL;
    }
    inode->mode = (de->attributes & JLOS_FAT32_ATTR_DIRECTORY) ? JLOS_VFS_S_IFDIR : JLOS_VFS_S_IFREG;
    inode->size = de->size;
    inode->i_ops = &s_fat32_inode_ops;
    inode->f_ops = &s_fat32_file_ops;
    fat32_ii(inode)->first_cluster = first_cluster;
    return inode;
}

static jlos_vfs_dentry_t *fat32_lookup(jlos_vfs_inode_t *dir, const char *name)
{
    jlos_vfs_super_block_t *sb = dir->sb;
    jlos_fat32_sb_info_t *sbi = fat32_sbi(sb);
    jlos_fat32_inode_info_t *fi = fat32_ii(dir);
    uint32_t cluster = fi->first_cluster;
    char dirent_name[JLOS_VFS_NAME_MAX + 1];
    uint8_t *cluster_buf = jlos_kalloc(sbi->bytes_per_cluster);
    if (!cluster_buf) {
        return NULL;
    }
    while (cluster < JLOS_FAT32_CLUSTER_EOC) {
        if (fat32_read_cluster(sb, cluster, cluster_buf) != 0) {
            jlos_kfree(cluster_buf);
            return NULL;
        }
        uint32_t entries_per_cluster = sbi->bytes_per_cluster / sizeof(jlos_fat32_dirent_t);
        jlos_fat32_dirent_t *entries = (jlos_fat32_dirent_t *)cluster_buf;
        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            if (entries[i].name[0] == JLOS_FAT32_DIRENT_END) {
                jlos_kfree(cluster_buf);
                return NULL;
            }
            if (fat32_dirent_should_skip(&entries[i])) {
                continue;
            }
            fat32_name_to_str(&entries[i], dirent_name, sizeof(dirent_name));
            if (jlos_strcmp(dirent_name, name) != 0) {
                continue;
            }
            jlos_vfs_inode_t *inode = fat32_dirent_to_inode(sb, &entries[i]);
            if (!inode) {
                jlos_kfree(cluster_buf);
                return NULL;
            }
            jlos_vfs_dentry_t *child = jlos_vfs_dentry_alloc(name, inode);
            jlos_kfree(cluster_buf);
            return child;
        }
        cluster = fat32_get_next_cluster(sb, cluster);
    }
    jlos_kfree(cluster_buf);
    return NULL;
}

static int fat32_read(jlos_vfs_file_t *file, uint8_t *buf, uint32_t count)
{
    if (file->pos >= file->inode->size) {
        return 0;
    }
    jlos_vfs_inode_t *inode = file->inode;
    jlos_vfs_super_block_t *sb = inode->sb;
    jlos_fat32_sb_info_t *sbi = fat32_sbi(sb);
    jlos_fat32_inode_info_t *fi = fat32_ii(inode);
    
    if (file->pos + count > inode->size) {
        count = inode->size - file->pos;
    }
    uint32_t cluster = fi->first_cluster;
    uint32_t cluster_index = file->pos / sbi->bytes_per_cluster;
    uint32_t cluster_offset = file->pos % sbi->bytes_per_cluster;

    for (uint32_t i = 0; i < cluster_index; i++) {
        cluster = fat32_get_next_cluster(sb, cluster);
        if (cluster >= JLOS_FAT32_CLUSTER_EOC) {
            return 0;
        }
    }
    uint8_t *cluster_buf = jlos_kalloc(sbi->bytes_per_cluster);
    if (!cluster_buf) {
        return -1;
    }
    uint32_t bytes_read = 0;
    while (bytes_read < count) {
        if (cluster >= JLOS_FAT32_CLUSTER_EOC) {
            break;
        }
        if (fat32_read_cluster(sb, cluster, cluster_buf) != 0) {
            jlos_kfree(cluster_buf);
            return -1;
        }
        uint32_t to_copy = sbi->bytes_per_cluster - cluster_offset;
        if (to_copy > count - bytes_read) {
            to_copy = count - bytes_read;
        }
        jlos_memcpy(buf + bytes_read, cluster_buf + cluster_offset, to_copy);
        bytes_read += to_copy;
        cluster_offset = 0;
        cluster = fat32_get_next_cluster(sb, cluster);
    }
    file->pos += bytes_read;
    jlos_kfree(cluster_buf);
    return (int)bytes_read;
}

static int fat32_readdir(jlos_vfs_file_t *file, jlos_vfs_dentry_t *dirent)
{
    jlos_vfs_inode_t *inode = file->inode;
    jlos_vfs_super_block_t *sb = inode->sb;
    jlos_fat32_sb_info_t *sbi = fat32_sbi(sb);
    jlos_fat32_inode_info_t *fi = fat32_ii(inode);
    uint32_t entry_index = 0;
    uint32_t cluster = fi->first_cluster;
    uint8_t *cluster_buf = jlos_kalloc(sbi->bytes_per_cluster);
    if (!cluster_buf) {
        return -1;
    }
    while (cluster < JLOS_FAT32_CLUSTER_EOC) {
        if (fat32_read_cluster(sb, cluster, cluster_buf) != 0) {
            jlos_kfree(cluster_buf);
            return -1;
        }
        uint32_t entries_per_cluster = sbi->bytes_per_cluster / sizeof(jlos_fat32_dirent_t);
        jlos_fat32_dirent_t *entries = (jlos_fat32_dirent_t *)cluster_buf;
        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            if (entries[i].name[0] == JLOS_FAT32_DIRENT_END) {
                jlos_kfree(cluster_buf);
                return -1;
            }
            if (fat32_dirent_should_skip(&entries[i])) {
                continue;
            }
            if (entry_index != file->pos) {
                entry_index++;
                continue;
            }
            fat32_name_to_str(&entries[i], dirent->name, JLOS_VFS_NAME_MAX + 1);
            dirent->name_len = (uint32_t)jlos_strlen(dirent->name);
            jlos_vfs_inode_t *child_inode = fat32_dirent_to_inode(sb, &entries[i]);
            if (!child_inode) {
                jlos_kfree(cluster_buf);
                return -1;
            }
            dirent->inode = child_inode;
            file->pos++;
            jlos_kfree(cluster_buf);
            return 0;
        }
        cluster = fat32_get_next_cluster(sb, cluster);
    }
    jlos_kfree(cluster_buf);
    return -1;
}

static const jlos_vfs_super_ops_t s_fat32_super_ops = {
    .alloc_inode = fat32_alloc_inode,
    .destroy_inode = fat32_destroy_inode,
};

static const jlos_vfs_inode_ops_t s_fat32_inode_ops = {
    .lookup = fat32_lookup,
};

static const jlos_vfs_file_ops_t s_fat32_file_ops = {
    .read = fat32_read,
    .readdir = fat32_readdir,
};

static jlos_vfs_super_block_t *fat32_mount(jlos_hal_block_dev_t *dev, void *data)
{
    (void)data;
    uint8_t sector[JLOS_FAT32_BYTES_PER_SECTOR];
    if (jlos_hal_block_read(dev, 0, sector, 1) != 0) {
        printk_err("failed to read BPB\n");
        return NULL;
    }
    jlos_fat32_bpb_t bpb;
    jlos_memcpy(&bpb, sector, sizeof(bpb));
    if (bpb.bytes_per_sector != JLOS_FAT32_BYTES_PER_SECTOR) {
        printk_err("unsupported sector size: %u\n", bpb.bytes_per_sector);
        return NULL;
    }
    jlos_fat32_sb_info_t *sbi = jlos_kalloc(sizeof(jlos_fat32_sb_info_t));
    if (!sbi) {
        return NULL;
    }
    jlos_memcpy(&sbi->bpb, &bpb, sizeof(bpb));
    sbi->fat_start = bpb.reserved_sectors;
    sbi->data_start = sbi->fat_start + bpb.table_size * bpb.fat_copies;
    sbi->root_cluster = bpb.root_cluster;
    sbi->bytes_per_cluster = (uint32_t)bpb.sector_per_cluster * bpb.bytes_per_sector;
    sbi->sectors_per_cluster = bpb.sector_per_cluster;
    sbi->fat_copies = bpb.fat_copies;
    sbi->total_clusters = (bpb.total_sector_count - sbi->data_start) / bpb.sector_per_cluster;

    jlos_vfs_super_block_t *sb = jlos_kalloc(sizeof(jlos_vfs_super_block_t));
    if (!sb) {
        jlos_kfree(sbi);
        return NULL;
    }
    jlos_memset(sb, 0, sizeof(*sb));
    sb->block_dev = dev;
    sb->ops = &s_fat32_super_ops;
    sb->fs_private = sbi;
    jlos_spinlock_init(&sb->lock);

    jlos_vfs_inode_t *root_inode = jlos_vfs_inode_alloc(sb, sbi->root_cluster);
    if (!root_inode) {
        jlos_kfree(sbi);
        jlos_kfree(sb);
        return NULL;
    }
    root_inode->mode = JLOS_VFS_S_IFDIR;
    root_inode->size = 0;
    root_inode->i_ops = &s_fat32_inode_ops;
    root_inode->f_ops = &s_fat32_file_ops;
    fat32_ii(root_inode)->first_cluster = sbi->root_cluster;

    jlos_vfs_dentry_t *root_dentry = jlos_vfs_dentry_alloc("/", root_inode);
    if (!root_dentry) {
        jlos_vfs_inode_put(root_inode);
        jlos_kfree(sbi);
        jlos_kfree(sb);
        return NULL;
    }
    sb->root_dentry = root_dentry;

    printk_debug("fat32 mounted: cluster = %u, root = %u\n", sbi->total_clusters, sbi->root_cluster);
    return sb;
}

static int fat32_unmount(jlos_vfs_super_block_t *sb)
{
    if (!sb) {
        return -1;
    }
    if (sb->root_dentry) {
        jlos_vfs_dentry_put(sb->root_dentry);
    }
    if (sb->fs_private) {
        jlos_kfree(sb->fs_private);
    }
    jlos_kfree(sb);
    return 0;
}

jlos_vfs_fs_type_t g_fat32_fs_type = {
    .name = "fat32",
    .mount = fat32_mount,
    .unmount = fat32_unmount,
};

static void jlos_fat32_init(void)
{
    jlos_vfs_register_fs_type(&g_fat32_fs_type);
}

JLOS_INITCALL(JLOS_INITCALL_DEVICE, jlos_fat32_init);
