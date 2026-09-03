#include "initramfs.h"
#include "vfs.h"
#include <stddef.h>
struct embedded_file{const char*path;const char*data;};
static const struct embedded_file image[]={
 {"/system/welcome.txt","Welcome to Axiom. Your system. Your rules.\n"},
 {"/boot/axiom.conf","filesystem=axiomfs\nwrites=validated\n"},
 {"/system/version","Axiom 0.4.0 x86_64\n"},
 {"/apps/README","Applications will live here after user mode is implemented.\n"}
};
bool initramfs_load(void){for(unsigned i=0;i<sizeof(image)/sizeof(image[0]);i++){int fd=vfs_open(image[i].path,VFS_WRITE|VFS_CREATE|VFS_TRUNCATE);if(fd<0)return false;size_t n=0;while(image[i].data[n])n++;if(vfs_write(fd,image[i].data,n)!=(long)n){vfs_close(fd);return false;}vfs_close(fd);}return true;}
