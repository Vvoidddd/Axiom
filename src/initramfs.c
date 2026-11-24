#include "initramfs.h"
#include "vfs.h"
#include "log.h"
#include "memory.h"
#include <stddef.h>
extern const unsigned char user_abiprobe_start[],user_abiprobe_end[];
struct embedded_file{const char*path;const char*data;};
static const struct embedded_file image[]={
 {"/system/welcome.txt","Welcome to Axiom. Your system. Your rules.\n"},
 {"/boot/axiom.conf","filesystem=axiomfs\nwrites=validated\n"},
 {"/system/version","Axiom 0.5.0 x86_64\n"},
 {"/apps/README","Shipped applications are read-only. Store application data in your home directory.\n"}
 ,{"/system/packages/init.pkg","name=init\nversion=0.1.0\nabi=1\ntype=system\nexecutable=/system/bin/init\n"}
 ,{"/system/packages/shell.pkg","name=shell\nversion=0.1.0\nabi=1\ntype=system\nexecutable=/system/bin/shell\n"}
 ,{"/system/packages/hello.pkg","name=hello\nversion=0.1.0\nabi=1\ntype=app\nexecutable=/apps/hello\n"}
 ,{"/system/services/system-logger.svc","schema=1\nname=system-logger\naccount=system-logger\nexecutable=/system/bin/loggerd\nrestart=always\nrestart_limit=3\nheartbeat_ms=2000\n"}
 ,{"/system/services/device-manager.svc","schema=1\nname=device-manager\naccount=device-manager\nexecutable=/system/bin/deviced\nrestart=always\nrestart_limit=3\nheartbeat_ms=2000\n"}
};
extern const unsigned char user_init_start[],user_init_end[],user_shell_start[],user_shell_end[],user_hello_start[],user_hello_end[],user_loggerd_start[],user_loggerd_end[],user_deviced_start[],user_deviced_end[];
static bool install_display_sample(void){
    enum {WIDTH=160,HEIGHT=96,SIZE=54+WIDTH*HEIGHT*3};
    unsigned char*bmp=kmalloc(SIZE);if(!bmp)return false;
    for(unsigned i=0;i<54;i++)bmp[i]=0;
    bmp[0]='B';bmp[1]='M';bmp[2]=SIZE&255;bmp[3]=(SIZE>>8)&255;bmp[10]=54;bmp[14]=40;bmp[18]=WIDTH;bmp[22]=HEIGHT;bmp[26]=1;bmp[28]=24;
    for(unsigned y=0;y<HEIGHT;y++)for(unsigned x=0;x<WIDTH;x++){
        unsigned i=54+((HEIGHT-1-y)*WIDTH+x)*3;
        bmp[i]=((x/20+y/12)&1)?210:60;bmp[i+1]=y*255/(HEIGHT-1);bmp[i+2]=x*255/(WIDTH-1);
    }
    bool ok=vfs_replace_file("/system/display.bmp",bmp,SIZE,false)==0;kfree(bmp);return ok;
}
static bool seal_image(void){
    if(!install_display_sample())return false;
    if(vfs_seal("/boot")||vfs_seal("/system")||vfs_seal("/apps"))return false;
    bool ok=vfs_open("/system/bin/shell",VFS_WRITE|VFS_TRUNCATE)==VFS_EROFS
        &&vfs_chmod("/system/bin/shell",0777)==VFS_EROFS
        &&vfs_unlink("/system",true)==VFS_EROFS
        &&vfs_rename("/apps","/tmp/apps")==VFS_EROFS
        &&vfs_open("/boot/new",VFS_WRITE|VFS_CREATE)==VFS_EROFS
        &&vfs_unmount("/system")==VFS_EROFS
        &&vfs_can_access("/system/bin/shell",VFS_READ|VFS_EXECUTE);
    log_write(ok?"PASS":"FAIL","IMMUTABLE SYSTEM IMAGE TEST");
    return ok;
}
static bool install_binary(const char*path,const unsigned char*start,const unsigned char*end){int fd=vfs_open(path,VFS_WRITE|VFS_CREATE|VFS_TRUNCATE);if(fd<0)return false;size_t size=(size_t)(end-start);bool ok=vfs_write(fd,start,size)==(long)size;ok=vfs_close(fd)==0&&ok;return ok&&vfs_chmod(path,0755)==0;}
bool initramfs_load(void){int e=vfs_mkdir("/system/packages",0755);if(e&&e!=VFS_EEXIST)return false;e=vfs_mkdir("/system/services",0755);if(e&&e!=VFS_EEXIST)return false;for(unsigned i=0;i<sizeof(image)/sizeof(image[0]);i++){int fd=vfs_open(image[i].path,VFS_WRITE|VFS_CREATE|VFS_TRUNCATE);if(fd<0)return false;size_t n=0;while(image[i].data[n])n++;if(vfs_write(fd,image[i].data,n)!=(long)n){vfs_close(fd);return false;}vfs_close(fd);}e=vfs_mkdir("/system/bin",0755);if(e&&e!=VFS_EEXIST)return false;return install_binary("/system/bin/abiprobe",user_abiprobe_start,user_abiprobe_end)&&install_binary("/system/bin/init",user_init_start,user_init_end)&&install_binary("/system/bin/shell",user_shell_start,user_shell_end)&&install_binary("/apps/hello",user_hello_start,user_hello_end)&&install_binary("/system/bin/loggerd",user_loggerd_start,user_loggerd_end)&&install_binary("/system/bin/deviced",user_deviced_start,user_deviced_end)&&seal_image();}
