#include "vfs.h"
#include "security.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

void *kmalloc(size_t size){return malloc(size);}
void kfree(void *p){free(p);}
uint64_t hardware_uptime_ms(void){return 1000;}

static int file(const char *path){
    int fd=vfs_open(path,VFS_CREATE|VFS_WRITE);
    assert(fd>=0);assert(vfs_write(fd,"ok",2)==2);return fd;
}
static void denied(const char *path){
    assert(vfs_open(path,VFS_WRITE|VFS_TRUNCATE)==VFS_EROFS);
    assert(vfs_chmod(path,0777)==VFS_EROFS);
    assert(vfs_chown(path,1,1)==VFS_EROFS);
    assert(vfs_lock(path,true)==VFS_EROFS);
    assert(vfs_unlink(path,false)==VFS_EROFS);
    assert(vfs_rename(path,"/tmp/escaped")==VFS_EROFS);
    assert(vfs_link(path,"/tmp/new-alias",false)==VFS_EROFS);
    assert(!vfs_can_access(path,VFS_WRITE));
    int fd=vfs_open(path,VFS_READ);char bytes[2];assert(fd>=0);
    assert(vfs_read(fd,bytes,2)==2&&bytes[0]=='o'&&bytes[1]=='k');vfs_close(fd);
}
int main(void){
    vfs_init();assert(vfs_self_test());
    assert(vfs_mkdir("/system/bin",0755)==0);
    int open_before=file("/system/bin/app");
    assert(vfs_link("/system/bin/app","/tmp/alias",false)==0);
    assert(vfs_link("/system/bin/app","/tmp/symlink",true)==0);
    vfs_close(file("/tmp/external"));
    assert(vfs_link("/tmp/external","/system/bin/imported",false)==0);
    assert(vfs_seal("/boot")==0&&vfs_seal("/system")==0&&vfs_seal("/apps")==0);
    denied("/system/bin/app");denied("/tmp/alias");denied("/tmp/external");
    assert(vfs_open("/tmp/symlink",VFS_WRITE)==VFS_EROFS);
    assert(vfs_write(open_before,"bad",3)==VFS_EROFS);vfs_close(open_before);
    assert(vfs_unlink("/system",true)==VFS_EROFS);
    assert(vfs_rename("/system","/tmp/moved")==VFS_EROFS);
    assert(vfs_mkdir("/system/bin/new",0755)==VFS_EROFS);
    assert(vfs_open("/apps/new",VFS_WRITE|VFS_CREATE)==VFS_EROFS);
    assert(vfs_open("/boot/new",VFS_WRITE|VFS_CREATE)==VFS_EROFS);
    assert(vfs_mount("ram0","/system","ramfs",false)==VFS_EROFS);
    assert(vfs_unmount("/system")==VFS_EROFS);
    vfs_close(file("/tmp/mutable"));
    assert(vfs_rename("/tmp/mutable","/system/bin/new")==VFS_EROFS);
    assert(vfs_mkdir("/var/config",0700)==0);vfs_close(file("/var/config/settings"));
    assert(vfs_mkdir("/users/test",0700)==0);vfs_close(file("/users/test/data"));
    vfs_set_security_context(1,1,0,0,0022,CAP_ALL);
    denied("/system/bin/app");assert(vfs_seal("/tmp")==VFS_EACCES);
    vfs_set_credentials(0,0);

    /* Read-only mounts protect descendants and already-open aliases too. */
    assert(vfs_mkdir("/tmp/mount-parent",0755)==0);
    assert(vfs_mkdir("/tmp/mount-parent/volume",0755)==0);
    assert(vfs_mkdir("/tmp/mount-parent/volume/sub",0755)==0);
    int fd=file("/tmp/mount-parent/volume/sub/file");
    assert(vfs_link("/tmp/mount-parent/volume/sub/file","/tmp/mount-alias",false)==0);
    assert(vfs_mount("ram0","/tmp/mount-parent/volume","ramfs",true)==0);
    denied("/tmp/mount-parent/volume/sub/file");denied("/tmp/mount-alias");
    assert(vfs_write(fd,"bad",3)==VFS_EROFS);
    assert(vfs_unlink("/tmp/mount-parent",true)==VFS_EROFS);
    assert(vfs_rename("/tmp/mount-parent","/tmp/moved")==VFS_EROFS);
    assert(vfs_unmount("/tmp/mount-parent/volume")==0);
    assert(vfs_write(fd,"ok",2)==2);vfs_close(fd);
    assert(vfs_rename("/tmp/mount-parent","/tmp/mount-parent/volume/cycle")==VFS_EINVAL);
    assert(vfs_rename("/","/tmp/root")==VFS_EINVAL);
    assert(vfs_check(false)==0);
    puts("PASS: immutable system trees, aliases, descriptors, mounts and mutable data");
}
