#include "picture.h"
#include "vfs.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fail_after=-1;
static unsigned live;
void*kmalloc(size_t n){if(!fail_after)return NULL;if(fail_after>0)fail_after--;void*p=malloc(n);if(p)live++;return p;}
void kfree(void*p){if(p){live--;free(p);}}
uint64_t hardware_uptime_ms(void){return 1000;}
static unsigned char bmp[70];
static void put32(unsigned at,uint32_t n){for(unsigned i=0;i<4;i++)bmp[at+i]=(unsigned char)(n>>(i*8));}
static void fixture(void){memset(bmp,0,sizeof(bmp));bmp[0]='B';bmp[1]='M';put32(2,70);put32(10,54);put32(14,40);put32(18,2);put32(22,2);bmp[26]=1;bmp[28]=24;bmp[54]=255;bmp[57]=bmp[58]=bmp[59]=255;bmp[64]=255;bmp[66]=255;}
static void save(void){assert(!vfs_replace_file("/tmp/picture",bmp,sizeof(bmp),true));}
int main(void){
    vfs_init();fixture();save();struct picture p={0};assert(!picture_load("/tmp/picture",&p));
    assert(p.width==2&&p.height==2&&p.pixels[0]==0xff0000&&p.pixels[1]==0x00ff00&&p.pixels[2]==0x0000ff&&p.pixels[3]==0xffffff);
    uint32_t*previous=p.pixels;unsigned before=live;
    for(int i=0;i<2;i++){fail_after=i;assert(picture_load("/tmp/picture",&p)==VFS_ENOSPC);fail_after=-1;assert(p.pixels==previous&&live==before);}
    put32(22,0xfffffffe);save();assert(!picture_load("/tmp/picture",&p));assert(p.pixels[0]==0x0000ff);previous=p.pixels;
    const unsigned offsets[]={2,10,14,18,22,26,28,30};
    for(unsigned i=0;i<sizeof(offsets)/sizeof(offsets[0]);i++){fixture();put32(offsets[i],offsets[i]==22?0x80000000u:UINT32_MAX);save();assert(picture_load("/tmp/picture",&p)==VFS_EINVAL&&p.pixels==previous);}
    fixture();put32(18,2048);put32(22,2048);save();assert(picture_load("/tmp/picture",&p)==VFS_EINVAL);
    fixture();save();assert(!vfs_chmod("/tmp/picture",0600));vfs_set_credentials(1,1);assert(picture_load("/tmp/picture",&p)==VFS_EACCES);vfs_set_credentials(0,0);
    uint32_t random=1234;
    for(unsigned i=0;i<1000;i++){fixture();random=random*1664525u+1013904223u;bmp[random%54]^=(uint8_t)(random>>24);save();picture_load("/tmp/picture",&p);}
    picture_close(&p);assert(!vfs_unlink("/tmp/picture",false));assert(!live);
    puts("PASS: BMP orientation, pixels, padding, permissions, failed-load preservation and 1000 malformed headers");
}
