#include "framebuffer.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int main(void){
    uint32_t pixels[12*8],buffer[8*8];for(unsigned i=0;i<96;i++)pixels[i]=0x12345678;
    struct limine_framebuffer fb={.address=pixels,.width=8,.height=8,.pitch=48,.bpp=32,.memory_model=LIMINE_FRAMEBUFFER_RGB,.red_mask_size=8,.red_mask_shift=16,.green_mask_size=8,.green_mask_shift=8,.blue_mask_size=8};
    assert(fb_init(&fb));assert(!fb_begin_frame(buffer,63));assert(fb_begin_frame(buffer,64));assert(!fb_begin_frame(buffer,64));
    fb_clip(2,2,3,3);fb_rect(0,0,UINT64_MAX,UINT64_MAX,0xabcdef);
    assert(pixels[2*12+2]==0x12345678);fb_present();
    for(unsigned y=0;y<8;y++)for(unsigned x=0;x<12;x++)assert(pixels[y*12+x]==(x>=2&&x<5&&y>=2&&y<5?0xabcdef:0x12345678));
    assert(fb_begin_frame(buffer,64));fb_rect(0,0,1,1,1);fb_rect(7,7,1,1,2);fb_present();
    assert(pixels[0]==1&&pixels[7*12+7]==2&&pixels[3*12+3]==0xabcdef&&pixels[6*12+6]==0x12345678);
    uint32_t saved[96];memcpy(saved,pixels,sizeof(saved));fb_pointer_show(6,6);assert(fb_begin_frame(buffer,64));fb_present();assert(!memcmp(saved,pixels,sizeof(saved)));
    fb_clip(UINT64_MAX,UINT64_MAX,UINT64_MAX,UINT64_MAX);fb_clear(0);assert(!memcmp(saved,pixels,sizeof(saved)));fb_clip_reset();
    puts("PASS: buffered presentation, clipping, pitch padding, overflow bounds and pointer restoration");
}
