#ifndef _JLOS_FILESYSTEM_VFS_H
#define _JLOS_FILESYSTEM_VFS_H

#include <hal/block.h>
#include <hal/atomic.h>
#include <hal/spinlock.h>
#include <dsa/list.h>
#include <dsa/hash_chain.h>
#include <common/types.h>

#define JLOS_VFS_NAME_MAX       255
#define JLOS_VFS_PATH_MAX       1024
#define JLOS_VFS_HASH_BUCKETS   256

#define JLOS_VFS_S_IFMT     0xF000
#define JLOS_VFS_S_IFREG    0x8000
#define JLOS_VFS_S_IFDIR    0x4000
#define JLOS_VFS_S_IFLNK    0xA000

#define JLOS_VFS_IS_REG(mode) (((mode) & JLOS_VFS_S_IFMT) == JLOS_VFS_S_IFREG)
#define JLOS_VFS_IS_DIR(mode) (((mode) & JLOS_VFS_S_IFMT) == JLOS_VFS_S_IFDIR)

#define JLOS_VFS_O_RDONLY   0x00
#define JLOS_VFS_O_WRONLY   0x01
#define JLOS_VFS_O_RDWR     0x02
#define JLOS_VFS_O_ACCMODE  0x03
#define JLOS_VFS_O_CREAT    0x04
#define JLOS_VFS_O_TRUNC    0x08
#define JLOS_VFS_O_APPEND   0x10

#define JLOS_VFS_SEEK_SET   0
#define JLOS_VFS_SEEK_CUR   1
#define JLOS_VFS_SEEK_END   2

#define JLOS_VFS_DCACHE_MAX    256

typedef struct jlos_vfs_inode           jlos_vfs_inode_t;
typedef struct jlos_vfs_super_block     jlos_vfs_super_block_t;
typedef struct jlos_vfs_dentry          jlos_vfs_dentry_t;
typedef struct jlos_vfs_file            jlos_vfs_file_t;
typedef struct jlos_vfs_fs_type         jlos_vfs_fs_type_t;

typedef struct jlos_vfs_dentry {
    char                name[JLOS_VFS_NAME_MAX + 1];
    uint32_t            name_len;
    jlos_vfs_inode_t    *inode;
    jlos_vfs_dentry_t   *parent;
    jlos_list_head_t    child_list;
    jlos_list_head_t    sibling;
    jlos_atomic_t       ref_count;
    jlos_hash_node_t    hash_node;
    bool                unhashed;
    jlos_list_head_t    lru;
} jlos_vfs_dentry_t;

typedef struct jlos_vfs_inode_ops {
    jlos_vfs_dentry_t   *(*lookup)(jlos_vfs_inode_t *dir, const char *name);
    int                 (*create)(jlos_vfs_inode_t *dir, const char *name, uint32_t mode);
    int                 (*mkdir)(jlos_vfs_inode_t *dir, const char *name);
    int                 (*unlink)(jlos_vfs_inode_t *dir, const char *name);
} jlos_vfs_inode_ops_t;

typedef struct jlos_vfs_file_ops {
    int (*open)(jlos_vfs_file_t *file);
    int (*close)(jlos_vfs_file_t *file);
    int (*read)(jlos_vfs_file_t *file, uint8_t *buf, uint32_t count);
    int (*write)(jlos_vfs_file_t *file, const uint8_t *buf, uint32_t count);
    int (*seek)(jlos_vfs_file_t *file, int32_t offset, int whence);
    int (*readdir)(jlos_vfs_file_t *file, jlos_vfs_dentry_t *dirent);
} jlos_vfs_file_ops_t;

typedef struct jlos_vfs_inode {
    uint32_t                    ino;
    uint32_t                    mode;
    uint32_t                    size;
    jlos_atomic_t               ref_count;
    jlos_vfs_super_block_t      *sb;
    const jlos_vfs_inode_ops_t  *i_ops;
    const jlos_vfs_file_ops_t   *f_ops;
    void                        *fs_private;
    jlos_hash_node_t            hash_node;
} jlos_vfs_inode_t;

typedef struct jlos_vfs_super_ops {
    jlos_vfs_inode_t    *(*alloc_inode)(jlos_vfs_super_block_t *sb);
    void                (*destroy_inode)(jlos_vfs_inode_t *inode);
    int                 (*read_inode)(jlos_vfs_inode_t *inode);
    int                 (*write_inode)(jlos_vfs_inode_t *inode);
} jlos_vfs_super_ops_t;

typedef struct jlos_vfs_super_block {
    jlos_vfs_fs_type_t          *fs_type;
    jlos_hal_block_dev_t        *block_dev;
    jlos_vfs_dentry_t            *root_dentry;
    const jlos_vfs_super_ops_t  *ops;
    void                        *fs_private;
    jlos_spinlock_t             lock;
} jlos_vfs_super_block_t;

typedef struct jlos_vfs_file {
    jlos_vfs_dentry_t           *dentry;
    jlos_vfs_inode_t            *inode;
    uint32_t                    pos;
    uint32_t                    flags;
    jlos_atomic_t               ref_count;
    const jlos_vfs_file_ops_t   *f_ops;
} jlos_vfs_file_t;

typedef struct jlos_vfs_fs_type {
    const char              *name;
    jlos_vfs_super_block_t  *(*mount)(jlos_hal_block_dev_t *dev, void *data);
    int                     (*unmount)(jlos_vfs_super_block_t *sb);
    jlos_list_head_t        list;
} jlos_vfs_fs_type_t;

typedef struct jlos_vfs_mount {
    jlos_vfs_super_block_t  *sb;
    jlos_vfs_dentry_t       *mountpoint;
    jlos_vfs_dentry_t       *root;
    jlos_list_head_t        list;
} jlos_vfs_mount_t;

int jlos_vfs_register_fs_type(jlos_vfs_fs_type_t *fs_type);
int jlos_vfs_unregister_fs_type(jlos_vfs_fs_type_t *fs_type);

jlos_vfs_mount_t *jlos_vfs_mount(const char *fs_type_name, jlos_hal_block_dev_t *dev, const char *mount_path);
int jlos_vfs_unmount(jlos_vfs_mount_t *mnt);

jlos_vfs_dentry_t *jlos_vfs_lookup(const char *path);
int jlos_vfs_unlink(const char *path);

jlos_vfs_file_t *jlos_vfs_open(const char *path, uint32_t flags, uint32_t mode);
int jlos_vfs_close(jlos_vfs_file_t *file);
int jlos_vfs_read(jlos_vfs_file_t *file, uint8_t *buf, uint32_t count);
int jlos_vfs_write(jlos_vfs_file_t *file, const uint8_t *buf, uint32_t count);
int jlos_vfs_lseek(jlos_vfs_file_t *file, int32_t offset, int whence);
int jlos_vfs_readdir(jlos_vfs_file_t *file, jlos_vfs_dentry_t *dirent);

void jlos_vfs_inode_get(jlos_vfs_inode_t *inode);
void jlos_vfs_inode_put(jlos_vfs_inode_t *inode);
void jlos_vfs_dentry_get(jlos_vfs_dentry_t *dentry);
void jlos_vfs_dentry_put(jlos_vfs_dentry_t *dentry);

jlos_vfs_inode_t *jlos_vfs_inode_alloc(jlos_vfs_super_block_t *sb, uint32_t ino);
jlos_vfs_dentry_t *jlos_vfs_dentry_alloc(const char *name, jlos_vfs_inode_t *inode);

#endif
