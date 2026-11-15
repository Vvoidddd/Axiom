#include "checkpoint.h"
#include "snapshot.h"
#include "block.h"
#include "vfs.h"
#include "power.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static unsigned char *disk_bytes;
static int fail_at=-1,operations;
static struct block_device device;
static bool writable=true;
void*kmalloc(size_t n){return malloc(n);}
void kfree(void*p){free(p);}
uint64_t hardware_uptime_ms(void){return 1000;}
void log_write(const char*a,const char*b){(void)a;(void)b;}
bool account_save(void){return true;}
bool service_stop_all(void){return true;}
bool log_flush(void){return true;}
uint32_t process_quiesce_for_power(void){return 0;}
int block_flush_all(void){return 0;}
uint32_t block_device_count(void){return 1;}
struct block_device*block_device_at(uint32_t i){return i?0:&device;}
static int read_disk(struct block_device*d,uint64_t l,uint32_t n,void*b){(void)d;assert(l+n<=131072);memcpy(b,disk_bytes+l*512,(size_t)n*512);return 0;}
static int write_disk(struct block_device*d,uint64_t l,uint32_t n,void*b){(void)d;if(++operations==fail_at)return -1;assert(l+n<=131072);memcpy(disk_bytes+l*512,b,(size_t)n*512);return 0;}
static int sync_disk(struct block_device*d){(void)d;return ++operations==fail_at?-1:0;}
bool ahci_enable_checkpoint_writes(struct block_device*d){if(!writable)return false;d->readonly=false;d->write=write_disk;d->sync=sync_disk;return true;}
static uint32_t crc(const void*p,size_t n){const unsigned char*b=p;uint32_t c=~0u;while(n--){c^=*b++;for(unsigned i=0;i<8;i++)c=(c>>1)^(0xedb88320u&-(c&1));}return ~c;}
static void label(void){uint64_t magic=0x3130415441445841ull;uint32_t fields[]={1,131072,2048,34816};memcpy(disk_bytes,&magic,8);memcpy(disk_bytes+8,fields,16);uint32_t c=crc(disk_bytes,24);memcpy(disk_bytes+24,&c,4);}
static void note(const char*s){int fd=vfs_open("/users/admin/note",VFS_WRITE|VFS_CREATE|VFS_TRUNCATE);assert(fd>=0);assert(vfs_write(fd,s,1)==1);vfs_close(fd);}
static char value(void){char c=0;int fd=vfs_open("/users/admin/note",VFS_READ);assert(fd>=0);assert(vfs_read(fd,&c,1)==1);vfs_close(fd);return c;}
int main(void){
    const size_t size=64*1024*1024;disk_bytes=calloc(1,size);unsigned char*baseline=malloc(size);assert(disk_bytes&&baseline);
    device=(struct block_device){.sectors=131072,.sector_size=512,.readonly=true,.present=true,.read=read_disk};vfs_init();
    assert(checkpoint_attach()==0&&device.readonly); // Ordinary blank disks are never opted in.
    label();writable=false;assert(checkpoint_attach()==-1&&device.readonly);writable=true;
    assert(checkpoint_attach()==1);note("A");assert(checkpoint_save());note("B");assert(checkpoint_save());memcpy(baseline,disk_bytes,size);
    for(int failure=1;failure<=6;failure++){
        memcpy(disk_bytes,baseline,size);fail_at=-1;assert(checkpoint_attach()==2);assert(value()=='B');note("C");operations=0;fail_at=failure;assert(!checkpoint_save());
        fail_at=-1;assert(checkpoint_attach()==2);assert(value()==(failure==6?'C':'B'));
    }
    memcpy(disk_bytes,baseline,size);disk_bytes[(34816+1)*512]^=1;assert(checkpoint_attach()==2&&value()=='A');
    disk_bytes[(2048+1)*512]^=1;assert(checkpoint_attach()==-1);assert(value()=='A');
    power_init();assert(!power_prepare(POWER_ACTION_REBOOT));assert(power_get_status()->state==POWER_FAILED);
    vfs_set_credentials(0,0);assert(!vfs_unlink("/users/admin/note",false));free(baseline);free(disk_bytes);
    puts("PASS: opt-in disk policy, six interrupted-save stages, corrupt-generation fallback and fail-closed restore");
}
