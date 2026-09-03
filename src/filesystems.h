#ifndef AXIOM_FILESYSTEMS_H
#define AXIOM_FILESYSTEMS_H
#include "block.h"
#include <stdbool.h>
#include <stddef.h>
enum filesystem_kind{FS_UNKNOWN,FS_FAT32,FS_AXIOMFS};
enum filesystem_kind filesystem_probe(struct block_device *device,uint64_t start_lba);
int fat32_validate(struct block_device *device,uint64_t start_lba);
struct fat32_volume{struct block_device*device;uint64_t start_lba,total_sectors,fat_lba,data_lba;uint32_t sectors_per_fat,root_cluster,cluster_count;uint16_t bytes_per_sector;uint8_t sectors_per_cluster,fat_count;};
struct fat32_entry{char name[256];uint32_t cluster,size;uint16_t date,time;bool directory,readonly,hidden;};
typedef bool(*fat32_list_fn)(const struct fat32_entry *entry,void *context);
int fat32_open(struct fat32_volume *volume,struct block_device *device,uint64_t start_lba);
int fat32_lookup(struct fat32_volume *volume,const char *path,struct fat32_entry *entry);
int fat32_list(struct fat32_volume *volume,const char *path,fat32_list_fn callback,void *context);
long fat32_read(struct fat32_volume *volume,const char *path,uint64_t offset,void *buffer,size_t size);
bool fat32_self_test(struct block_device *device);
int axiomfs_format(struct block_device *device,uint64_t start_lba,uint64_t sectors);
int axiomfs_check(struct block_device *device,uint64_t start_lba,bool repair);
int axiomfs_simulate_interrupted_write(struct block_device *device,uint64_t start_lba);
struct axiomfs_volume{struct block_device*device;uint64_t start_lba,sectors,generation;uint32_t inode_count,data_start;};
struct axiomfs_stat{uint64_t size,created,modified;uint32_t inode,uid,gid,mode,links;bool directory,symlink,sparse;};
int axiomfs_open(struct axiomfs_volume *volume,struct block_device *device,uint64_t start_lba,bool repair);
int axiomfs_create(struct axiomfs_volume *volume,const char *path,bool directory,uint32_t uid,uint32_t gid,uint32_t mode);
long axiomfs_write(struct axiomfs_volume *volume,const char *path,uint64_t offset,const void *buffer,size_t size,uint32_t uid,uint32_t gid);
long axiomfs_read(struct axiomfs_volume *volume,const char *path,uint64_t offset,void *buffer,size_t size,uint32_t uid,uint32_t gid);
int axiomfs_stat_path(struct axiomfs_volume *volume,const char *path,struct axiomfs_stat *stat);
int axiomfs_rename(struct axiomfs_volume *volume,const char *old_path,const char *new_path);
int axiomfs_link(struct axiomfs_volume *volume,const char *target,const char *link_path,bool symbolic);
int axiomfs_set_xattr(struct axiomfs_volume *volume,const char *path,const char *name,const char *value);
int axiomfs_get_xattr(struct axiomfs_volume *volume,const char *path,const char *name,char *value,size_t capacity);
bool axiomfs_self_test(struct block_device *device);
#endif
