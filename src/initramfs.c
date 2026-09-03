#include "initramfs.h"
#include "vfs.h"
#include <stddef.h>
struct embedded_file{const char*path;const char*data;};
static const struct embedded_file image[]={
 {"/system/welcome.txt","Welcome to Axiom. Your system. Your rules.\n"},
 {"/boot/axiom.conf","filesystem=axiomfs\nwrites=validated\n"},
 {"/system/version","Axiom 0.5.0 x86_64\n"},
 {"/apps/README","Applications will live here after user mode is implemented.\n"}
 ,{"/system/packages/init.pkg","name=init\nversion=0.1.0\nabi=1\ntype=system\nexecutable=/system/bin/init\n"}
 ,{"/system/packages/shell.pkg","name=shell\nversion=0.1.0\nabi=1\ntype=system\nexecutable=/system/bin/shell\n"}
 ,{"/system/packages/hello.pkg","name=hello\nversion=0.1.0\nabi=1\ntype=app\nexecutable=/apps/hello\n"}
};
extern const unsigned char user_init_start[],user_init_end[],user_shell_start[],user_shell_end[],user_hello_start[],user_hello_end[];
static bool install_binary(const char*path,const unsigned char*start,const unsigned char*end){int fd=vfs_open(path,VFS_WRITE|VFS_CREATE|VFS_TRUNCATE);if(fd<0)return false;size_t size=(size_t)(end-start);bool ok=vfs_write(fd,start,size)==(long)size;return vfs_close(fd)==0&&ok;}
bool initramfs_load(void){int e=vfs_mkdir("/system/packages",0755);if(e&&e!=VFS_EEXIST)return false;for(unsigned i=0;i<sizeof(image)/sizeof(image[0]);i++){int fd=vfs_open(image[i].path,VFS_WRITE|VFS_CREATE|VFS_TRUNCATE);if(fd<0)return false;size_t n=0;while(image[i].data[n])n++;if(vfs_write(fd,image[i].data,n)!=(long)n){vfs_close(fd);return false;}vfs_close(fd);}e=vfs_mkdir("/system/bin",0755);if(e&&e!=VFS_EEXIST)return false;return install_binary("/system/bin/init",user_init_start,user_init_end)&&install_binary("/system/bin/shell",user_shell_start,user_shell_end)&&install_binary("/apps/hello",user_hello_start,user_hello_end);}
