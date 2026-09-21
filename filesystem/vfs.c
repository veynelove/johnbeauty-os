#include <filesystem/vfs.h>
#include <kernel/memory_manager.h>
#include <kernel/initcall.h>

#define JLOS_KERNEL_LOG_SUBSYS "vfs"
#include <kernel/printk.h>

typedef struct {
    jlos_vfs_super_block_t  *sb;
    uint32_t                ino;
} vfs_inode_key_t;

typedef struct {
    jlos_vfs_dentry_t   *parent;
    const char          *name;
} vfs_dentry_key_t;

static jlos_list_head_t     s_fs_types;
static jlos_list_head_t     s_mounts;
static jlos_vfs_dentry_t    *s_root_dentry;
static jlos_hash_chain_t    s_inode_hash;
static jlos_hash_chain_t    s_dentry_hash;

static uint32_t inode_hash_fn(const void *key)
{
    const vfs_inode_key_t *k = (const vfs_inode_key_t *)key;
    return jlos_hash_ptr(k->sb) ^ jlos_hash_uint32(&k->ino);
}

static int inode_cmp_fn(const void *key, const void *node)
{
    const vfs_inode_key_t *k = (const vfs_inode_key_t *)key;
    const jlos_vfs_inode_t *inode = container_of((const jlos_hash_node_t *)node, jlos_vfs_inode_t, hash_node);
    if (inode->sb != k->sb) {
        return 1;
    }
    return (inode->ino != k->ino);
}

static uint32_t dentry_hash_fn(const void *key)
{
    const vfs_dentry_key_t *k = (const vfs_dentry_key_t *)key;
    return jlos_hash_ptr(k->parent) ^ jlos_hash_str(k->name);
}

static int dentry_cmp_fn(const void *key, const void *node)
{
    const vfs_dentry_key_t *k = (const vfs_dentry_key_t *)key;
    const jlos_vfs_dentry_t *d = container_of((const jlos_hash_node_t *)node, jlos_vfs_dentry_t, hash_node);
    if (d->parent != k->parent) {
        return 1;
    }
    return (jlos_strcmp(d->name, k->name) != 0);
}

static void jlos_vfs_init(void)
{
    jlos_list_init(&s_fs_types);
    jlos_list_init(&s_mounts);
    s_root_dentry = NULL;
    jlos_hash_chain_init(&s_inode_hash, JLOS_VFS_HASH_BUCKETS, inode_hash_fn, inode_cmp_fn);
    jlos_hash_chain_init(&s_dentry_hash, JLOS_VFS_HASH_BUCKETS, dentry_hash_fn, dentry_cmp_fn);
}

int jlos_vfs_register_fs_type(jlos_vfs_fs_type_t *fs_type)
{
    if (!fs_type || !fs_type->name || !fs_type->mount) {
        return -1;
    }
    jlos_list_add(&fs_type->list, &s_fs_types);
    return 0;
}

int jlos_vfs_unregister_fs_type(jlos_vfs_fs_type_t *fs_type)
{
    if (!fs_type) {
        return -1;
    }
    jlos_list_del(&fs_type->list);
    return 0;
}

static jlos_vfs_fs_type_t *find_fs_type(const char *name)
{
    jlos_vfs_fs_type_t *ft;
    jlos_list_for_each_entry(ft, &s_fs_types, list) {
        if (jlos_strcmp(ft->name, name) == 0) {
            return ft;
        }
    }
    return NULL;
}

static jlos_vfs_dentry_t *dentry_lookup_child(jlos_vfs_dentry_t *parent, const char *name)
{
    vfs_dentry_key_t key = {.parent = parent, .name = name};
    jlos_hash_node_t *node = jlos_hash_chain_see(&s_dentry_hash, &key);
    if (!node) {
        return NULL;
    }
    return container_of(node, jlos_vfs_dentry_t, hash_node);
}

static int path_next_component(const char *path, char *buf, size_t bufsize)
{
    size_t i = 0;
    while (path[i] && path[i] != '/' && i < bufsize - 1) {
        buf[i] = path[i];
        i++;
    }
    buf[i] = 0;
    return (int)i;
}

jlos_vfs_mount_t *jlos_vfs_mount(const char *fs_type_name, jlos_hal_block_dev_t *dev, const char *mount_path)
{
    jlos_vfs_fs_type_t *ft = find_fs_type(fs_type_name);
    if (!ft) {
        printk_err("unknown fs type: %s\n", fs_type_name);
        return NULL;
    }
    jlos_vfs_super_block_t *sb = ft->mount(dev, NULL);
    if (!sb) {
        printk_err("mount failed\n");
        return NULL;
    }
    sb->fs_type = ft;
    jlos_vfs_mount_t *mnt = jlos_kalloc(sizeof(jlos_vfs_mount_t));
    if (!mnt) {
        return NULL;
    }
    jlos_memset(mnt, 0, sizeof(*mnt));
    mnt->sb = sb;
    if (!s_root_dentry) {
        s_root_dentry = sb->root_dentry;
        jlos_vfs_dentry_get(s_root_dentry);
        mnt->root = s_root_dentry;
        mnt->mountpoint = NULL;
    } else {
        jlos_vfs_dentry_t *mp = jlos_vfs_lookup(mount_path);
        if (!mp) {
            printk_err("mountpoint not fount: %s\n", mount_path);
            jlos_kfree(mnt);
            return NULL;
        }
        mp->inode = sb->root_dentry->inode;
        jlos_vfs_inode_get(mp->inode);
        mnt->root = sb->root_dentry;
        mnt->mountpoint = mp;
    }
    jlos_list_add(&mnt->list, &s_mounts);

    return mnt;
}

int jlos_vfs_unmount(jlos_vfs_mount_t *mnt)
{
    if (!mnt) {
        return -1;
    }
    if (mnt->sb && mnt->sb->fs_type && mnt->sb->fs_type->unmount) {
        mnt->sb->fs_type->unmount(mnt->sb);
    }
    jlos_list_del(&mnt->list);
    if (mnt->mountpoint) {
        jlos_vfs_dentry_put(mnt->mountpoint);
    }
    jlos_kfree(mnt);
    return 0;
}

jlos_vfs_dentry_t *jlos_vfs_lookup(const char *path)
{
    const char *p = path;
    if (!p || p[0] != '/' || !s_root_dentry) {
        return NULL;
    }
    char component[JLOS_VFS_NAME_MAX + 1];
    jlos_vfs_dentry_t *cur = s_root_dentry;
    jlos_vfs_dentry_get(cur);
    p++;
    while (*p) {
        while (*p == '/') {
            p++;
        }
        if (!*p) {
            break;
        }
        int len = path_next_component(p, component, sizeof(component));
        if (len == 0) {
            break;
        }
        p += len;
        jlos_vfs_dentry_t *child = dentry_lookup_child(cur, component);
        if (child) {
            jlos_vfs_dentry_get(child);
            jlos_vfs_dentry_put(cur);
            cur = child;
            continue;
        }
        if (!cur->inode || !cur->inode->i_ops || !cur->inode->i_ops->lookup) {
            jlos_vfs_dentry_put(cur);
            return NULL;
        }
        child = cur->inode->i_ops->lookup(cur->inode, component);
        if (!child) {
            jlos_vfs_dentry_put(cur);
            return NULL;
        }
        child->parent = cur;
        jlos_list_add(&child->sibling, &cur->child_list);
        vfs_dentry_key_t key = {.parent = cur, .name = child->name};
        jlos_hash_chain_insert(&s_dentry_hash, &key, &child->hash_node);
        jlos_vfs_dentry_get(child);
        jlos_vfs_dentry_put(cur);
        cur = child;
    }
    return cur;
}

jlos_vfs_file_t *jlos_vfs_open(const char *path, uint32_t flags)
{
    jlos_vfs_dentry_t *d = jlos_vfs_lookup(path);
    if (!d || !d->inode) {
        if (d) {
            jlos_vfs_dentry_put(d);
        }
        return NULL;
    }
    jlos_vfs_file_t *file = jlos_kalloc(sizeof(jlos_vfs_file_t));
    if (!file) {
        jlos_vfs_dentry_put(d);
        return NULL;
    }
    jlos_memset(file, 0, sizeof(*file));
    file->dentry = d;
    file->inode = d->inode;
    file->pos = 0;
    file->flags = flags;
    file->f_ops = d->inode->f_ops;
    jlos_atomic_set(&file->ref_count, 1);
    if (file->f_ops && file->f_ops->open) {
        if (file->f_ops->open(file) != 0) {
            jlos_vfs_dentry_put(d);
            jlos_kfree(file);
            return NULL;
        }
    }
    return file;
}

int jlos_vfs_close(jlos_vfs_file_t *file)
{
    if (!file) {
        return -1;
    }
    int ret = 0;
    if (file->f_ops && file->f_ops->close) {
        ret = file->f_ops->close(file);
    }
    jlos_vfs_dentry_put(file->dentry);
    jlos_kfree(file);
    return ret;
}

int jlos_vfs_read(jlos_vfs_file_t *file, uint8_t *buf, uint32_t count)
{
    if (!file || !file->f_ops || !file->f_ops->read) {
        return -1;
    }
    return file->f_ops->read(file, buf, count);
}

int jlos_vfs_write(jlos_vfs_file_t *file, const uint8_t *buf, uint32_t count)
{
    if (!file || !file->f_ops || !file->f_ops->write) {
        return -1;
    }
    return file->f_ops->write(file, buf, count);
}

int jlos_vfs_lseek(jlos_vfs_file_t *file, int32_t offset, int whence)
{
    if (!file) {
        return -1;
    }
    if (file->f_ops && file->f_ops->seek) {
        return file->f_ops->seek(file, offset, whence);
    }
    switch (whence) {
        case JLOS_VFS_SEEK_SET :
            file->pos = (uint32_t)offset;
            break;
        case JLOS_VFS_SEEK_CUR :
            file->pos = (uint32_t)((int32_t)file->pos + offset);
            break;
        case JLOS_VFS_SEEK_END:
            file->pos = file->inode->size + (uint32_t)offset;
            break;
        default:
            return -1;
    }
    return (int)file->pos;
}

int jlos_vfs_readdir(jlos_vfs_file_t *file, jlos_vfs_dentry_t *dirent)
{
    if (!file || !file->f_ops || !file->f_ops->readdir) {
        return -1;
    }
    return file->f_ops->readdir(file, dirent);
}

void jlos_vfs_inode_get(jlos_vfs_inode_t *inode)
{
    if (inode) {
        jlos_atomic_inc(&inode->ref_count);
    }
}

void jlos_vfs_inode_put(jlos_vfs_inode_t *inode)
{
    if (!inode) {
        return;
    }
    if (jlos_atomic_dec_return(&inode->ref_count) == 0) {
        jlos_hash_chain_remove(&s_inode_hash, &inode->hash_node);
        if (inode->sb && inode->sb->ops && inode->sb->ops->destroy_inode) {
            inode->sb->ops->destroy_inode(inode);
        }
    }
}

void jlos_vfs_dentry_get(jlos_vfs_dentry_t *dentry)
{
    if (dentry) {
        jlos_atomic_inc(&dentry->ref_count);
    }
}

void jlos_vfs_dentry_put(jlos_vfs_dentry_t *dentry)
{
    if (!dentry) {
        return;
    }
    if (jlos_atomic_dec_return(&dentry->ref_count) == 0) {
        if (dentry->parent) {
            jlos_list_del(&dentry->sibling);
        }
        jlos_hash_chain_remove(&s_dentry_hash, &dentry->hash_node);
        jlos_vfs_inode_put(dentry->inode);
        jlos_kfree(dentry);
    }
}

jlos_vfs_inode_t *jlos_vfs_inode_alloc(jlos_vfs_super_block_t *sb, uint32_t ino)
{
    if (!sb || !sb->ops || !sb->ops->alloc_inode) {
        return NULL;
    }
    jlos_vfs_inode_t *inode = sb->ops->alloc_inode(sb);
    if (!inode) {
        return NULL;
    }
    inode->ino = ino;
    inode->sb = sb;
    jlos_atomic_set(&inode->ref_count, 1);
    vfs_inode_key_t key = {.sb = sb, .ino = ino};
    jlos_hash_chain_insert(&s_inode_hash, &key, &inode->hash_node);
    return inode;
}

jlos_vfs_dentry_t *jlos_vfs_dentry_alloc(const char *name, jlos_vfs_inode_t *inode)
{
    jlos_vfs_dentry_t *d = jlos_kalloc(sizeof(jlos_vfs_dentry_t));
    if (!d) {
        return NULL;
    }
    jlos_memset(d, 0, sizeof(*d));
    jlos_strlcpy(d->name, name, JLOS_VFS_NAME_MAX + 1);
    d->name_len = (uint32_t)jlos_strlen(d->name);
    d->inode = inode;
    d->parent = NULL;
    jlos_list_init(&d->child_list);
    jlos_list_init(&d->sibling);
    jlos_atomic_set(&d->ref_count, 1);
    if (inode) {
        jlos_vfs_inode_get(inode);
    }
    return d;
}

JLOS_INITCALL(JLOS_INITCALL_SUBSYS, jlos_vfs_init);
