#include <filesystem/fat32.h>
#include <kernel/memory_manager.h>
#include <kernel/initcall.h>
#include <common/stypes.h>

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
    uint32_t fat_offset = cluster * JLOS_FAT32_FAT_ENTRY_SIZE;
    uint32_t fat_sector = sbi->fat_start + fat_offset / sbi->bpb.bytes_per_sector;
    uint32_t entry_offset = fat_offset % sbi->bpb.bytes_per_sector;
    
    uint8_t *buf = sbi->sec_buf;
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

static int fat32_fsinfo_read(jlos_vfs_super_block_t *sb, uint32_t *next_free)
{
    jlos_fat32_sb_info_t *sbi = fat32_sbi(sb);
    if (sbi->fsinfo_sector == 0) {
        return 1;
    }
    if (jlos_hal_block_read(sb->block_dev, sbi->fsinfo_sector, sbi->sec_buf, 1) != 0) {
        return -1;
    }
    uint32_t lead = *(uint32_t *)(sbi->sec_buf + JLOS_FAT32_FSINFO_LEAD_OFF);
    uint32_t stru_sig = *(uint32_t *)(sbi->sec_buf + JLOS_FAT32_FSINFO_STRUCT_OFF);
    if (lead != JLOS_FAT32_FSINFO_LEAD_SIG || stru_sig != JLOS_FAT32_FSINFO_STRUCT_SIG) {
        return 1;
    }
    if (next_free) {
        *next_free = *(uint32_t *)(sbi->sec_buf + JLOS_FAT32_FSINFO_FREE_OFF);
    }
    return 0;
}

static int fat32_fsinfo_update(jlos_vfs_super_block_t *sb, uint32_t next_free, int32_t free_delta)
{
    jlos_fat32_sb_info_t *sbi = fat32_sbi(sb);
    int ret = fat32_fsinfo_read(sb, NULL);
    if (ret != 0) {
        return (ret < 0) ? -1 : 0;
    }
    *(uint32_t *)(sbi->sec_buf + JLOS_FAT32_FSINFO_FREE_OFF) = next_free;
    *(uint32_t *)(sbi->sec_buf + JLOS_FAT32_FSINFO_COUNT_OFF) += free_delta;
    return jlos_hal_block_write(sb->block_dev, sbi->fsinfo_sector, sbi->sec_buf, 1);
}

static int fat32_set_next_cluster(jlos_vfs_super_block_t *sb, uint32_t cluster, uint32_t next)
{
    jlos_fat32_sb_info_t *sbi = fat32_sbi(sb);
    uint32_t fat_offset = cluster * JLOS_FAT32_FAT_ENTRY_SIZE;
    uint32_t fat_sector_index = fat_offset / sbi->bpb.bytes_per_sector;
    uint32_t entry_offset = fat_offset % sbi->bpb.bytes_per_sector;
    for (uint32_t copy = 0; copy < sbi->fat_copies; copy++) {
        uint32_t fat_sector = sbi->fat_start + fat_sector_index + copy * sbi->bpb.table_size;
        if (jlos_hal_block_read(sb->block_dev, fat_sector, sbi->sec_buf, 1) != 0) {
            return -1;
        }
        uint32_t *entry = (uint32_t *)(sbi->sec_buf + entry_offset);
        *entry = (*entry & ~JLOS_FAT32_CLUSTER_MASK) | (next & JLOS_FAT32_CLUSTER_MASK);
        if (jlos_hal_block_write(sb->block_dev, fat_sector, sbi->sec_buf, 1) != 0) {
            return -1;
        }
    }
    return 0;
}

static int fat32_claim_cluster(jlos_vfs_super_block_t *sb, uint32_t cluster, uint32_t *out_cluster)
{
    if (fat32_set_next_cluster(sb, cluster, JLOS_FAT32_CLUSTER_EOC) != 0) {
        return -1;
    }
    fat32_fsinfo_update(sb, cluster + 1, -1);
    *out_cluster = cluster;
    return 0;
}

static int fat32_alloc_cluster(jlos_vfs_super_block_t *sb, uint32_t *out_cluster)
{
    jlos_fat32_sb_info_t *sbi = fat32_sbi(sb);
    uint32_t fat_entries = sbi->total_clusters + JLOS_FAT32_FIRST_DATA_CLUSTER;
    uint32_t hint;

    if (fat32_fsinfo_read(sb, &hint) == 0 && hint >= JLOS_FAT32_FIRST_DATA_CLUSTER && hint < fat_entries) {
        uint32_t fat_offset = hint * JLOS_FAT32_FAT_ENTRY_SIZE;
        uint32_t fat_sector = sbi->fat_start + fat_offset / sbi->bpb.bytes_per_sector;
        uint32_t entry_offset = fat_offset % sbi->bpb.bytes_per_sector;
        if (jlos_hal_block_read(sb->block_dev, fat_sector, sbi->sec_buf, 1) == 0) {
            uint32_t val = *(uint32_t *)(sbi->sec_buf + entry_offset) & JLOS_FAT32_CLUSTER_MASK;
            if (val == JLOS_FAT32_CLUSTER_FREE) {
                return fat32_claim_cluster(sb, hint, out_cluster);
            }
        }
    }
    uint32_t entries_per_sector = JLOS_FAT32_BYTES_PER_SECTOR / JLOS_FAT32_FAT_ENTRY_SIZE;
    for (uint32_t sector_index = 0; sector_index * entries_per_sector < fat_entries; sector_index++) {
        if (jlos_hal_block_read(sb->block_dev, sbi->fat_start + sector_index, sbi->sec_buf, 1) != 0) {
            return -1;
        }
        uint32_t entry_count = entries_per_sector;
        if ((sector_index + 1) * entries_per_sector > fat_entries) {
            entry_count = fat_entries - sector_index * entries_per_sector;
        }
        for (uint32_t i = 0; i < entry_count; i++) {
            uint32_t val = *(uint32_t *)(sbi->sec_buf + i * JLOS_FAT32_FAT_ENTRY_SIZE) & JLOS_FAT32_CLUSTER_MASK;
            if (val != JLOS_FAT32_CLUSTER_FREE) {
                continue;
            }
            return fat32_claim_cluster(sb, sector_index * entries_per_sector + i, out_cluster);
        }
    }
    return -1;
}

static int fat32_free_cluster_chain(jlos_vfs_super_block_t *sb, uint32_t head)
{
    jlos_fat32_sb_info_t *sbi = fat32_sbi(sb);
    uint32_t cluster = head;
    uint32_t walked = 0;
    while (cluster >= JLOS_FAT32_FIRST_DATA_CLUSTER && cluster < JLOS_FAT32_CLUSTER_EOC) {
        if (walked++ >= sbi->total_clusters) {
            return -1;
        }
        uint32_t next = fat32_get_next_cluster(sb, cluster);
        if (fat32_set_next_cluster(sb, cluster, JLOS_FAT32_CLUSTER_FREE) != 0) {
            return -1;
        }
        cluster = next;
    }
    fat32_fsinfo_update(sb, head, 1);
    return 0;
}

static int fat32_write_cluster(jlos_vfs_super_block_t *sb, uint32_t cluster, const uint8_t *buf)
{
    jlos_fat32_sb_info_t *sbi = fat32_sbi(sb);
    uint32_t sector = fat32_cluster_to_sector(sbi, cluster);
    return jlos_hal_block_write(sb->block_dev, sector, buf, sbi->sectors_per_cluster);
}

static int fat32_cluster_advance(jlos_vfs_super_block_t *sb, uint32_t *cluster)
{
    uint32_t next = fat32_get_next_cluster(sb, *cluster);
    if (next < JLOS_FAT32_CLUSTER_EOC) {
        *cluster = next;
        return 0;
    }
    if (fat32_alloc_cluster(sb, &next) != 0) {
        return -1;
    }
    if (fat32_set_next_cluster(sb, *cluster, next) != 0) {
        return -1;
    }
    *cluster = next;
    return 0;
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
            if (name_lower) {
                c = (char)jlos_tolower(c);
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

static void fat32_dirent_locate(jlos_fat32_sb_info_t *sbi, uint32_t cluster, uint32_t entry_index, uint32_t *out_sector, uint32_t *out_index)
{
    uint32_t offset = entry_index * JLOS_FAT32_DIRENT_SIZE;
    *out_sector = fat32_cluster_to_sector(sbi, cluster) + (offset / JLOS_FAT32_BYTES_PER_SECTOR);
    *out_index = (offset % JLOS_FAT32_BYTES_PER_SECTOR) / JLOS_FAT32_DIRENT_SIZE;
}

static bool fat32_name_char_legal(char c)
{
    if (jlos_isalnum(c)) {
        return true;
    }
    return c == '_' || c == '-';
}

static int fat32_name_to_83(const char *name, jlos_fat32_dirent_t *de)
{
    const char *dot = NULL;
    uint32_t name_len = 0;
    for (uint32_t i = 0; name[i]; i++) {
        if (name[i] == '.') {
            if (dot || i == 0) {
                return -1;
            }
            dot = name + i;
        } else if (!fat32_name_char_legal(name[i])) {
            return -1;
        }
        name_len++;
    }
    if (name_len == 0 || name_len > JLOS_FAT32_SHORT_NAME_LEN + JLOS_FAT32_SHORT_EXT_LEN + 1) {
        return -1;
    }
    uint32_t base_len = dot ? (uint32_t)(dot - name) : name_len;
    if (base_len > JLOS_FAT32_SHORT_EXT_LEN) {
        return -1;
    }
    uint32_t ext_len = dot ? name_len - base_len - 1 : 0;
    if (ext_len > JLOS_FAT32_SHORT_EXT_LEN) {
        return -1;
    }
    for (uint32_t i = 0; i < JLOS_FAT32_SHORT_NAME_LEN; i++) {
        char c = (i < base_len) ? name[i] : JLOS_FAT32_NAME_FILL;
        de->name[i] = (uint8_t)jlos_toupper(c);
    }
    for (uint32_t i = 0; i < JLOS_FAT32_SHORT_EXT_LEN; i++) {
        uint32_t idx = base_len + 1 + i;
        char c = (dot && i < ext_len) ? name[idx] : JLOS_FAT32_NAME_FILL;
        de->ext[i] = (uint8_t)jlos_toupper(c);
    }
    return 0;
}

static int fat32_dirent_read(jlos_vfs_super_block_t *sb, uint32_t dir_sector, uint32_t dir_index, jlos_fat32_dirent_t *out)
{
    jlos_fat32_sb_info_t *sbi = fat32_sbi(sb);
    if (jlos_hal_block_read(sb->block_dev, dir_sector, sbi->sec_buf, 1) != 0) {
        return -1;
    }
    jlos_memcpy(out, sbi->sec_buf + dir_index * JLOS_FAT32_DIRENT_SIZE, sizeof(jlos_fat32_dirent_t));
    return 0;
}

static int fat32_dirent_write(jlos_vfs_super_block_t *sb, uint32_t dir_sector, uint32_t dir_index, const jlos_fat32_dirent_t *de)
{
    jlos_fat32_sb_info_t *sbi = fat32_sbi(sb);
    if (jlos_hal_block_read(sb->block_dev, dir_sector, sbi->sec_buf, 1) != 0) {
        return -1;
    }
    jlos_memcpy(sbi->sec_buf + dir_index * JLOS_FAT32_DIRENT_SIZE, de, sizeof(jlos_fat32_dirent_t));
    return jlos_hal_block_write(sb->block_dev, dir_sector, sbi->sec_buf, 1);
}

static fat32_scan_verdict_t fat32_match_name(const jlos_fat32_dirent_t *de, uint32_t entry_pos, void *ctx)
{
    (void)entry_pos;
    const char *name = (const char *)ctx;
    if (de->name[0] == JLOS_FAT32_DIRENT_END) {
        return FAT32_SCAN_STOP;
    }
    if (de->name[0] == JLOS_FAT32_DIRENT_DELETED || (de->attributes & JLOS_FAT32_ATTR_LFN) == JLOS_FAT32_ATTR_LFN) {
        return FAT32_SCAN_MISS;
    }
    char dirent_name[JLOS_VFS_NAME_MAX + 1];
    fat32_name_to_str(de, dirent_name, sizeof(dirent_name));
    return (jlos_strcmp(dirent_name, name) == 0) ? FAT32_SCAN_HIT : FAT32_SCAN_MISS;
}

static fat32_scan_verdict_t fat32_match_free_slot(const jlos_fat32_dirent_t *de, uint32_t entry_pos, void *ctx)
{
    (void)entry_pos;
    (void)ctx;
    if (de->name[0] == JLOS_FAT32_DIRENT_END || de->name[0] == JLOS_FAT32_DIRENT_DELETED) {
        return FAT32_SCAN_HIT;
    }
    return FAT32_SCAN_MISS;
}

static fat32_scan_verdict_t fat32_match_entry_at(const jlos_fat32_dirent_t *de, uint32_t entry_pos, void *ctx)
{
    uint32_t target = *(const uint32_t *)ctx;
    if (de->name[0] == JLOS_FAT32_DIRENT_END) {
        return FAT32_SCAN_STOP;
    }
    if (de->name[0] == JLOS_FAT32_DIRENT_DELETED || (de->attributes & JLOS_FAT32_ATTR_LFN) == JLOS_FAT32_ATTR_LFN) {
        return FAT32_SCAN_MISS;
    }
    return (entry_pos == target) ? FAT32_SCAN_HIT : FAT32_SCAN_MISS;
}

static fat32_scan_verdict_t fat32_dir_scan(jlos_vfs_inode_t *dir, fat32_dir_match_fn match, void *ctx, fat32_dirent_hit_t *hit)
{
    jlos_vfs_super_block_t *sb = dir->sb;
    jlos_fat32_sb_info_t *sbi = fat32_sbi(sb);
    uint32_t cluster = fat32_ii(dir)->first_cluster;
    uint32_t entry_pos = 0;
    hit->cluster = cluster;
    uint8_t *cluster_buf = jlos_kalloc(sbi->bytes_per_cluster);
    if (!cluster_buf) {
        return FAT32_SCAN_MISS;
    }
    while (cluster < JLOS_FAT32_CLUSTER_EOC) {
        if (fat32_read_cluster(sb, cluster, cluster_buf) != 0) {
            jlos_kfree(cluster_buf);
            return FAT32_SCAN_MISS;
        }
        uint32_t entries_per_sector = sbi->bytes_per_cluster / JLOS_FAT32_DIRENT_SIZE;
        jlos_fat32_dirent_t *entries = (jlos_fat32_dirent_t *)cluster_buf;
        for (uint32_t i = 0; i < entries_per_sector; i++, entry_pos++) {
            fat32_scan_verdict_t v = match(&entries[i], entry_pos, ctx);
            if (v == FAT32_SCAN_MISS) {
                continue;
            }
            hit->cluster = cluster;
            hit->entry_index = i;
            jlos_memcpy(&hit->de, &entries[i], sizeof(jlos_fat32_dirent_t));
            jlos_kfree(cluster_buf);
            return v;
        }
        hit->cluster = cluster;
        cluster = fat32_get_next_cluster(sb, cluster);
    }
    jlos_kfree(cluster_buf);
    return FAT32_SCAN_MISS;
}

static int fat32_sync_inode(jlos_vfs_inode_t *inode)
{
    jlos_fat32_inode_info_t *fi = fat32_ii(inode);
    jlos_fat32_dirent_t de;
    if (fi->dir_sector == 0) {
        return -1;
    }
    if (fat32_dirent_read(inode->sb, fi->dir_sector, fi->dir_index, &de) != 0) {
        return -1;
    }
    de.size = inode->size;
    de.first_cluster_low = fi->first_cluster & JLOS_FAT32_CLUSTER_LOW_MASK;
    de.first_cluster_hi = fi->first_cluster >> 16;
    return fat32_dirent_write(inode->sb, fi->dir_sector, fi->dir_index, &de);
}

static int fat32_find_free_slot(jlos_vfs_inode_t *dir, uint32_t *out_sector, uint32_t *out_index)
{
    jlos_vfs_super_block_t *sb = dir->sb;
    jlos_fat32_sb_info_t *sbi = fat32_sbi(sb);
    fat32_dirent_hit_t hit;
    if (fat32_dir_scan(dir, fat32_match_free_slot, NULL, &hit) == FAT32_SCAN_HIT) {
        fat32_dirent_locate(sbi, hit.cluster, hit.entry_index, out_sector, out_index);
        return 0;
    }
    uint32_t tail = hit.cluster;
    if (fat32_cluster_advance(sb, &tail) != 0) {
        return -1;
    }
    uint8_t *cluster_buf = jlos_kalloc(sbi->bytes_per_cluster);
    if (!cluster_buf) {
        return -1;
    }
    jlos_memset(cluster_buf, 0, sbi->bytes_per_cluster);
    if (fat32_write_cluster(sb, tail, cluster_buf) != 0) {
        jlos_kfree(cluster_buf);
        return -1;
    }
    jlos_kfree(cluster_buf);
    fat32_dirent_locate(sbi, tail, 0, out_sector, out_index);
    return 0;
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

static jlos_vfs_dentry_t *fat32_lookup_inner(jlos_vfs_inode_t *dir, const char *name)
{
    jlos_vfs_super_block_t *sb = dir->sb;
    fat32_dirent_hit_t hit;
    if (fat32_dir_scan(dir, fat32_match_name, (void *)name, &hit) != FAT32_SCAN_HIT) {
        return NULL;
    }
    jlos_vfs_inode_t *inode = fat32_dirent_to_inode(sb, &hit.de);
    if (!inode) {
        return NULL;
    }
    jlos_fat32_inode_info_t *ii = fat32_ii(inode);
    fat32_dirent_locate(fat32_sbi(sb), hit.cluster, hit.entry_index, &ii->dir_sector, &ii->dir_index);
    return jlos_vfs_dentry_alloc(name, inode);
}

static jlos_vfs_dentry_t *fat32_lookup(jlos_vfs_inode_t *dir, const char *name)
{
    uint32_t fl = jlos_spin_lock_irqsave(&dir->sb->lock);
    jlos_vfs_dentry_t *ret = fat32_lookup_inner(dir, name);
    jlos_spin_unlock_irqrestore(&dir->sb->lock, fl);
    return ret;
}

static int fat32_read_inner(jlos_vfs_file_t *file, uint8_t *buf, uint32_t count)
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

static int fat32_read(jlos_vfs_file_t *file, uint8_t *buf, uint32_t count)
{
    uint32_t fl = jlos_spin_lock_irqsave(&file->inode->sb->lock);
    int ret = fat32_read_inner(file, buf, count);
    jlos_spin_unlock_irqrestore(&file->inode->sb->lock, fl);
    return ret;
}

static int fat32_readdir_inner(jlos_vfs_file_t *file, jlos_vfs_dentry_t *dirent)
{
    jlos_vfs_inode_t *inode = file->inode;
    uint32_t target = file->pos;
    fat32_dirent_hit_t hit;
    if (fat32_dir_scan(inode, fat32_match_entry_at, &target, &hit) != FAT32_SCAN_HIT) {
        return -1;
    }
    fat32_name_to_str(&hit.de, dirent->name, JLOS_VFS_NAME_MAX + 1);
    dirent->name_len = (uint32_t)jlos_strlen(dirent->name);
    jlos_vfs_inode_t *child = fat32_dirent_to_inode(inode->sb, &hit.de);
    if (!child) {
        return -1;
    }
    jlos_fat32_inode_info_t *ii = fat32_ii(child);
    fat32_dirent_locate(fat32_sbi(inode->sb), hit.cluster, hit.entry_index, &ii->dir_sector, &ii->dir_index);
    dirent->inode = child;
    file->pos++;
    return 0;
}

static int fat32_readdir(jlos_vfs_file_t *file, jlos_vfs_dentry_t *dirent)
{
    uint32_t fl = jlos_spin_lock_irqsave(&file->inode->sb->lock);
    int ret = fat32_readdir_inner(file, dirent);
    jlos_spin_unlock_irqrestore(&file->inode->sb->lock, fl);
    return ret;
}

static int fat32_write_inner(jlos_vfs_file_t *file, const uint8_t *buf, uint32_t count)
{
    jlos_vfs_inode_t *inode = file->inode;
    jlos_vfs_super_block_t *sb = inode->sb;
    jlos_fat32_sb_info_t *sbi = fat32_sbi(sb);
    jlos_fat32_inode_info_t *fi = fat32_ii(inode);

    uint32_t end = file->pos + count;
    if (end < file->pos) {
        return -1;
    }
    uint32_t cluster;
    if (fi->first_cluster == 0) {
        if (fat32_alloc_cluster(sb, &cluster) != 0) {
            return -1;
        }
        fi->first_cluster = cluster;
    } else {
        cluster = fi->first_cluster;
    }
    uint32_t cluster_index = file->pos / sbi->bytes_per_cluster;
    for (uint32_t i = 0; i < cluster_index; i++) {
        if (fat32_cluster_advance(sb, &cluster) != 0) {
            return -1;
        }
    }
    uint8_t *cluster_buf = jlos_kalloc(sbi->bytes_per_cluster);
    if (!cluster_buf) {
        return -1;
    }
    uint32_t written = 0;
    uint32_t cluster_offset = file->pos % sbi->bytes_per_cluster;
    while (written < count) {
        uint32_t to_write = sbi->bytes_per_cluster - cluster_offset;
        if (to_write > count - written) {
            to_write = count - written;
        }
        if (fat32_read_cluster(sb, cluster, cluster_buf) != 0) {
            jlos_kfree(cluster_buf);
            return -1;
        }
        jlos_memcpy(cluster_buf + cluster_offset, buf + written, to_write);
        if (fat32_write_cluster(sb, cluster, cluster_buf) != 0) {
            jlos_kfree(cluster_buf);
            return -1;
        }
        written += to_write;
        cluster_offset = 0;
        if (written < count && fat32_cluster_advance(sb, &cluster) != 0) {
            jlos_kfree(cluster_buf);
            return -1;
        }
    }
    jlos_kfree(cluster_buf);
    file->pos += written;
    if (file->pos > inode->size) {
        inode->size = file->pos;
    }
    if (fat32_sync_inode(inode) != 0) {
        return -1;
    }
    return (int)written;
}

static int fat32_write(jlos_vfs_file_t *file, const uint8_t *buf, uint32_t count)
{
    uint32_t fl = jlos_spin_lock_irqsave(&file->inode->sb->lock);
    int ret = fat32_write_inner(file, buf, count);
    jlos_spin_unlock_irqrestore(&file->inode->sb->lock, fl);
    return ret;
}

static int fat32_open_inner(jlos_vfs_file_t *file)
{
    if ((file->flags & JLOS_VFS_O_TRUNC) && file->inode->size > 0) {
        jlos_fat32_inode_info_t *fi = fat32_ii(file->inode);
        if (fi->first_cluster != 0) {
            if (fat32_free_cluster_chain(file->inode->sb, fi->first_cluster) != 0) {
                return -1;
            }
            fi->first_cluster = 0;
        }
        file->inode->size = 0;
        if (fat32_sync_inode(file->inode) != 0) {
            return -1;
        }
    }
    if (file->flags & JLOS_VFS_O_APPEND) {
        file->pos = file->inode->size;
    }
    return 0;
}

static int fat32_open(jlos_vfs_file_t * file)
{
    uint32_t fl = jlos_spin_lock_irqsave(&file->inode->sb->lock);
    int ret = fat32_open_inner(file);
    jlos_spin_unlock_irqrestore(&file->inode->sb->lock, fl);
    return ret;
}

static int fat32_create_inner(jlos_vfs_inode_t *dir, const char *name, uint32_t mode)
{
    (void)mode;
    if (!JLOS_VFS_IS_DIR(dir->mode)) {
        return -1;
    }
    jlos_fat32_dirent_t de;
    jlos_memset(&de, 0, sizeof(de));
    if (fat32_name_to_83(name, &de) != 0) {
        return -1;
    }
    fat32_dirent_hit_t hit;
    if (fat32_dir_scan(dir, fat32_match_name, (void *)name, &hit) == FAT32_SCAN_HIT) {
        return -1;
    }
    uint32_t dir_sector, dir_index;
    if (fat32_find_free_slot(dir, &dir_sector, &dir_index) != 0) {
        return -1;
    }
    de.attributes = JLOS_FAT32_ATTR_ARCHIVE;
    de.c_time = JLOS_FAT32_TIME_DEFAULT;
    de.c_date = JLOS_FAT32_DATE_DEFAULT;
    de.w_time = JLOS_FAT32_TIME_DEFAULT;
    de.w_date = JLOS_FAT32_DATE_DEFAULT;
    return fat32_dirent_write(dir->sb, dir_sector, dir_index, &de);
}

static int fat32_create(jlos_vfs_inode_t *dir, const char *name, uint32_t mode)
{
    uint32_t fl = jlos_spin_lock_irqsave(&dir->sb->lock);
    int ret = fat32_create_inner(dir, name, mode);
    jlos_spin_unlock_irqrestore(&dir->sb->lock, fl);
    return ret;
}

static int fat32_unlink_inner(jlos_vfs_inode_t *dir, const char *name)
{
    fat32_dirent_hit_t hit;
    if (fat32_dir_scan(dir, fat32_match_name, (void *)name ,&hit) != FAT32_SCAN_HIT) {
        return -1;
    }
    if (hit.de.attributes & JLOS_FAT32_ATTR_DIRECTORY) {
        return -1;
    }
    uint32_t first_cluster = (((uint32_t)hit.de.first_cluster_hi << 16) | hit.de.first_cluster_low);
    if (first_cluster != 0 && fat32_free_cluster_chain(dir->sb, first_cluster) != 0) {
        return -1;
    }
    hit.de.name[0] = JLOS_FAT32_DIRENT_DELETED;
    uint32_t dir_sector, dir_index;
    fat32_dirent_locate(fat32_sbi(dir->sb), hit.cluster, hit.entry_index, &dir_sector, &dir_index);
    return fat32_dirent_write(dir->sb, dir_sector, dir_index, &hit.de);
}

static int fat32_unlink(jlos_vfs_inode_t *dir, const char *name)
{
    uint32_t fl = jlos_spin_lock_irqsave(&dir->sb->lock);
    int ret = fat32_unlink_inner(dir, name);
    jlos_spin_unlock_irqrestore(&dir->sb->lock, fl);
    return ret;
}

static const jlos_vfs_super_ops_t s_fat32_super_ops = {
    .alloc_inode = fat32_alloc_inode,
    .destroy_inode = fat32_destroy_inode,
};

static const jlos_vfs_inode_ops_t s_fat32_inode_ops = {
    .lookup = fat32_lookup,
    .create = fat32_create,
    .unlink = fat32_unlink,
};

static const jlos_vfs_file_ops_t s_fat32_file_ops = {
    .open = fat32_open,
    .read = fat32_read,
    .write = fat32_write,
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

    uint32_t total_sectors = bpb.total_sector_count ? bpb.total_sector_count : bpb.total_sectors;
    sbi->total_clusters = (total_sectors - sbi->data_start) / bpb.sector_per_cluster;
    sbi->fsinfo_sector = (bpb.fat_info < total_sectors) ? bpb.fat_info : 0;
    sbi->sec_buf = (uint8_t *)jlos_kalloc(bpb.bytes_per_sector);
    if (!sbi->sec_buf) {
        jlos_kfree(sbi);
        return NULL;
    }

    jlos_vfs_super_block_t *sb = jlos_kalloc(sizeof(jlos_vfs_super_block_t));
    if (!sb) {
        jlos_kfree(sbi->sec_buf);
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
        jlos_kfree(sbi->sec_buf);
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
        jlos_kfree(sbi->sec_buf);
        jlos_kfree(sbi);
        jlos_kfree(sb);
        return NULL;
    }
    sb->root_dentry = root_dentry;

    printk_debug("cluster = %u, root = %u\n", sbi->total_clusters, sbi->root_cluster);
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
        jlos_fat32_sb_info_t *sbi = (jlos_fat32_sb_info_t *)sb->fs_private;
        if (sbi->sec_buf) {
            jlos_kfree(sbi->sec_buf);
        }
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
