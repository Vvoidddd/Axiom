#include "block.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char disk[64][512];
static int reads,writes,fail_read,fail_write,fail_sync;
static int read_disk(struct block_device*d,uint64_t l,uint32_t n,void*b){
    (void)d;reads++;assert(l<64&&n<=64-l);
    if(fail_read){memset(b,0xee,17);return-1;}
    memcpy(b,disk[l],n*512);return 0;
}
static int write_disk(struct block_device*d,uint64_t l,uint32_t n,void*b){
    (void)d;writes++;assert(l<64&&n<=64-l);
    if(fail_write)return-1;
    memcpy(disk[l],b,n*512);return 0;
}
static int sync_disk(struct block_device*d){(void)d;return fail_sync?-1:0;}
int main(void){
    struct block_device d={.sectors=64,.sector_size=512,.present=true,.read=read_disk,.write=write_disk,.sync=sync_disk};
    unsigned char buffer[1024],saved[512];
    for(unsigned i=0;i<64;i++)memset(disk[i],i,512);
    block_init();
    assert(block_read(0,0,1,buffer)<0&&block_write(0,0,1,buffer)<0&&block_flush(0)<0);
    assert(block_read(&d,UINT64_MAX,2,buffer)<0&&block_write(&d,UINT64_MAX,2,buffer)<0);
    assert(block_read(&d,63,2,buffer)<0&&block_write(&d,63,2,buffer)<0);
    assert(block_read(&d,0,1,0)<0&&block_write(&d,0,1,0)<0);
    assert(!reads&&!writes);
    assert(!block_read(&d,64,0,0)&&!block_write(&d,64,0,0));
    for(unsigned i=0;i<32;i++)assert(!block_read(&d,i,1,buffer));
    fail_read=1;assert(block_read(&d,32,1,buffer)<0);fail_read=0;
    assert(!block_read(&d,0,1,buffer));assert(!memcmp(buffer,disk[0],512)); // Failed refill must not corrupt the old tag.
    d.present=false;assert(block_read(&d,0,1,buffer)<0&&block_write(&d,0,1,buffer)<0&&block_flush(&d)<0);d.present=true;
    memset(saved,0x5a,512);assert(!block_write(&d,1,1,saved));
    fail_write=1;assert(block_flush(&d)<0);assert(!block_read(&d,1,1,buffer)&&!memcmp(buffer,saved,512));
    fail_write=0;assert(!block_flush(&d)&&!memcmp(disk[1],saved,512));
    fail_sync=1;assert(block_flush(&d)<0);fail_sync=0;assert(!block_flush(&d));
    // A failed dirty eviction must preserve data for a later retry.
    block_init();for(unsigned i=0;i<32;i++)assert(!block_write(&d,i,1,saved));
    fail_write=1;assert(block_read(&d,32,1,buffer)<0);assert(!block_read(&d,0,1,buffer)&&!memcmp(buffer,saved,512));
    fail_write=0;assert(!block_flush(&d));
    d.readonly=true;assert(block_write(&d,0,1,saved)<0);d.readonly=false;
    d.write=0;assert(block_write(&d,0,1,saved)<0);d.read=0;assert(block_read(&d,0,1,buffer)<0);
    puts("PASS: block bounds, overflow, removed devices, failed read refill, dirty eviction and flush retry");
}
