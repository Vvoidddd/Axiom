#include "snapshot.h"
#include "vfs.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static int fail_after=-1;
void*kmalloc(size_t n){if(fail_after==0)return 0;if(fail_after>0)fail_after--;return malloc(n);}
void kfree(void*p){free(p);}
uint64_t hardware_uptime_ms(void){return 1000;}
static void write_file(const char*p,const char*s){int fd=vfs_open(p,VFS_WRITE|VFS_CREATE|VFS_TRUNCATE);assert(fd>=0);assert(vfs_write(fd,s,strlen(s))==(long)strlen(s));assert(!vfs_close(fd));}
static void expect(const char*p,const char*s){char b[100];int fd=vfs_open(p,VFS_READ);assert(fd>=0);long n=vfs_read(fd,b,sizeof(b));assert(n==(long)strlen(s)&&!memcmp(b,s,n));assert(!vfs_close(fd));}
int main(void){
    vfs_init();write_file("/system/kernel","original system");assert(!vfs_seal("/system"));
    assert(!vfs_mkdir("/users/one",0700));write_file("/users/one/note","saved note");
    assert(!vfs_chown("/users/one",1000,1000));assert(!vfs_chown("/users/one/note",1000,1000));
    assert(!vfs_link("/users/one/note","/users/one/hard",false));
    assert(!vfs_link("/users/one/note","/users/one/sym",true));
    int fd=vfs_open("/users/one/sparse",VFS_WRITE|VFS_CREATE);assert(fd>=0);assert(vfs_seek(fd,8192)==8192);assert(vfs_write(fd,"z",1)==1);vfs_close(fd);
    void*b=malloc(SNAPSHOT_MAX),*bad=malloc(SNAPSHOT_MAX);assert(b&&bad);
    long n=vfs_snapshot_export(b,SNAPSHOT_MAX);assert(n>0);
    write_file("/users/one/note","changed");
    fail_after=2;assert(vfs_snapshot_import(b,n)==VFS_ENOSPC);fail_after=-1;expect("/users/one/note","changed");
    assert(!vfs_snapshot_import(b,n));expect("/users/one/note","saved note");expect("/users/one/hard","saved note");expect("/users/one/sym","saved note");expect("/system/kernel","original system");
    struct vfs_stat st;assert(!vfs_stat_path("/users/one/sparse",&st)&&st.size==8193&&st.allocated==4096);
    assert(!vfs_stat_path("/users/one/note",&st)&&st.uid==1000&&st.links==2);
    for(long size=0;size<n;size+=97)assert(vfs_snapshot_import(b,size)<0);
    memcpy(bad,b,n);((uint32_t*)bad)[2]=99;assert(vfs_snapshot_import(bad,n)<0);
    // Untrusted parent IDs, including a later record, must never index outside the table.
    memcpy(bad,b,n);*(uint32_t*)((char*)bad+144+392+304)=0xffffffffu;assert(vfs_snapshot_import(bad,n)<0);
    expect("/users/one/note","saved note");
    fd=vfs_open("/users/one/note",VFS_READ);assert(vfs_snapshot_import(b,n)==VFS_EBUSY);vfs_close(fd);
    vfs_set_credentials(1001,1001);assert(vfs_snapshot_export(b,SNAPSHOT_MAX)==VFS_EACCES);assert(vfs_snapshot_import(b,n)==VFS_EACCES);assert(vfs_open("/users/one/note",VFS_READ)==VFS_EACCES);
    vfs_set_credentials(0,0);write_file("/users/one/bad\nname","invalid filename");
    assert(vfs_snapshot_export(bad,SNAPSHOT_MAX)==VFS_EINVAL);assert(!vfs_unlink("/users/one/bad\nname",false));
    assert(!vfs_seal("/users/one"));assert(vfs_snapshot_import(b,n)==VFS_EROFS);
    free(b);free(bad);puts("PASS: snapshot round trips, aliases, sparse files, permissions, allocation failure and malformed input");
}
