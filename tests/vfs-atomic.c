#include "vfs.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int fail_after=-1;
static unsigned live;
void *kmalloc(size_t n){if(fail_after==0)return NULL;if(fail_after>0)fail_after--;void*p=malloc(n);if(p)live++;return p;}
void kfree(void*p){if(p){live--;free(p);}}
uint64_t hardware_uptime_ms(void){return 1000;}
static void contents(const char*path,const char*expected,size_t size){
    char data[10000];int fd=vfs_open(path,VFS_READ);assert(fd>=0);
    assert(vfs_read(fd,data,sizeof(data))==(long)size);assert(!memcmp(data,expected,size));assert(!vfs_close(fd));
}
int main(void){
    vfs_init();assert(!vfs_mkdir("/users/test",0700));assert(!vfs_chown("/users/test",1,1));
    vfs_set_credentials(1,1);const char*path="/users/test/file";
    assert(!vfs_replace_file(path,"old",3,false));assert(!vfs_chmod(path,0600));
    char data[9000];memset(data,'N',sizeof(data));
    struct vfs_stat before,after;assert(!vfs_stat_path(path,&before));
    unsigned baseline=live;
    for(int failure=0;failure<3;failure++){
        fail_after=failure;assert(vfs_replace_file(path,data,sizeof(data),true)==VFS_ENOSPC);fail_after=-1;
        contents(path,"old",3);assert(live==baseline);assert(!vfs_stat_path(path,&after));
        assert(before.size==after.size&&before.allocated==after.allocated&&before.modified==after.modified&&before.mode==after.mode);
        fail_after=failure;assert(vfs_replace_file("/users/test/new",data,sizeof(data),false)==VFS_ENOSPC);fail_after=-1;
        assert(vfs_stat_path("/users/test/new",&after)==VFS_ENOENT);assert(live==baseline);
    }
    assert(vfs_replace_file(path,"bad",3,false)==VFS_EEXIST);contents(path,"old",3);
    int fd=vfs_open(path,VFS_READ|VFS_WRITE);assert(fd>=0);
    assert(!vfs_link(path,"/users/test/alias",false));
    assert(!vfs_lock(path,true));assert(vfs_replace_file(path,data,1,true)==VFS_EBUSY);
    assert(vfs_write(fd,data,1)==VFS_EBUSY);assert(!vfs_lock(path,false));
    assert(!vfs_link(path,"/users/test/sym",true));assert(vfs_replace_file("/users/test/sym",data,1,true)==VFS_EINVAL);
    vfs_set_credentials(0,0);assert(!vfs_set_quota(1,4096));vfs_set_credentials(1,1);
    assert(vfs_replace_file(path,data,sizeof(data),true)==VFS_EDQUOT);contents(path,"old",3);
    assert(!vfs_replace_file(path,"new",3,true)); // Same final quota, no temporary double-charge.
    char held[4]={0};assert(vfs_read(fd,held,3)==3&&!strcmp(held,"new"));contents("/users/test/alias","new",3);
    assert(!vfs_stat_path(path,&after)&&after.mode==0600&&after.uid==1&&after.links==2);
    vfs_set_credentials(2,2);assert(vfs_replace_file(path,"bad",3,true)==VFS_EACCES);
    vfs_set_credentials(0,0);assert(!vfs_set_quota(1,1024*1024));vfs_set_credentials(1,1);
    // Ordinary writes must roll back pages allocated before a later allocation fails.
    assert(vfs_seek(fd,0)==0);baseline=live;
    fail_after=1;assert(vfs_write(fd,data,sizeof(data))==VFS_ENOSPC);fail_after=-1;
    assert(live==baseline);contents(path,"new",3);assert(vfs_user_usage(1)==4096);
    assert(vfs_seek(fd,UINT64_MAX)==-1);assert(vfs_write(fd,data,2)==VFS_EDQUOT);
    assert(vfs_write(fd,NULL,0)==0&&live==baseline);assert(vfs_seek(fd,0)==0);
    assert(vfs_write(fd,data,sizeof(data))==(long)sizeof(data));contents(path,data,sizeof(data));
    assert(!vfs_replace_file(path,NULL,0,true));contents(path,"",0);assert(vfs_user_usage(1)==0);
    assert(!vfs_close(fd));
    assert(vfs_replace_file("/users/test",data,1,true)==VFS_EISDIR);
    assert(vfs_replace_file("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuv",data,1,false)==VFS_ENAMETOOLONG);
    vfs_set_credentials(0,0);assert(!vfs_seal("/users/test"));assert(vfs_replace_file(path,data,1,true)==VFS_EROFS);
    assert(vfs_replace_file("/users/test/nope",data,1,false)==VFS_EROFS);assert(live==0);
    assert(!vfs_check(false));puts("PASS: atomic saves, allocation rollback, aliases, permissions, locks, quotas and empty/overflow writes");
}
