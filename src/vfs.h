#ifndef AXIOM_VFS_H
#define AXIOM_VFS_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
enum vfs_error{VFS_OK=0,VFS_ENOENT=-2,VFS_EIO=-5,VFS_EBADF=-9,VFS_EACCES=-13,VFS_EBUSY=-16,VFS_EEXIST=-17,VFS_ENOTDIR=-20,VFS_EISDIR=-21,VFS_EINVAL=-22,VFS_ENOSPC=-28,VFS_EROFS=-30,VFS_ENAMETOOLONG=-36,VFS_ELOOP=-40,VFS_EDQUOT=-122};
#define VFS_READ 1u
#define VFS_WRITE 2u
#define VFS_CREATE 4u
#define VFS_TRUNCATE 8u
#define VFS_DIRECTORY 16u
#define VFS_EXECUTE 32u
struct vfs_stat{uint64_t size,allocated,created,modified;uint32_t uid,gid,mode,links;bool directory,symlink,sparse;};
struct vfs_security_context{uint32_t uid,gid,groups[8],group_count,creation_mask;uint64_t capabilities;};
typedef void(*vfs_list_fn)(const char*,const struct vfs_stat*,void*);
void vfs_init(void);
int vfs_open(const char *path,uint32_t flags);
int vfs_close(int fd);
long vfs_read(int fd,void *buffer,size_t size);
long vfs_write(int fd,const void *buffer,size_t size);
long vfs_seek(int fd,uint64_t offset);
int vfs_mkdir(const char *path,uint32_t mode);
int vfs_unlink(const char *path,bool recursive);
int vfs_rename(const char *old_path,const char *new_path);
int vfs_link(const char *target,const char *link_path,bool symbolic);
int vfs_stat_path(const char *path,struct vfs_stat *stat);
int vfs_list(const char *path,vfs_list_fn callback,void *context);
int vfs_chmod(const char *path,uint32_t mode);
int vfs_chown(const char *path,uint32_t uid,uint32_t gid);
void vfs_set_credentials(uint32_t uid,uint32_t gid);
void vfs_set_security_context(uint32_t uid,uint32_t gid,const uint32_t *groups,
                              uint32_t group_count,uint32_t creation_mask,
                              uint64_t capabilities);
void vfs_get_security_context(uint32_t *uid,uint32_t *gid,uint32_t *creation_mask,
                              uint64_t *capabilities);
void vfs_capture_security_context(struct vfs_security_context *context);
void vfs_restore_security_context(const struct vfs_security_context *context);
bool vfs_can_access(const char *path,uint32_t access);
int vfs_lock(const char *path,bool locked);
int vfs_set_quota(uint32_t uid,uint64_t bytes);
uint64_t vfs_user_usage(uint32_t uid);
int vfs_chdir(const char *path);
const char *vfs_cwd(void);
int vfs_mount(const char *source,const char *path,const char *type,bool readonly);
int vfs_unmount(const char *path);
uint64_t vfs_used_bytes(void);
uint64_t vfs_capacity_bytes(void);
int vfs_check(bool repair);
const char *vfs_error_string(int error);
bool vfs_self_test(void);
uint32_t vfs_self_test_failure(void);
#endif
