#include "form.h"
#include "mouse.h"
#include "framebuffer.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static void edit(struct form_field*f,char c,uint8_t special){
    struct key_event key={.pressed=true,.character=c,.special=special};
    form_edit(f,&key);
}
int main(void){
    char text[5]="";struct form_field field={.text=text,.capacity=sizeof(text)};
    edit(&field,'a',0);edit(&field,'c',0);edit(&field,0,KEY_LEFT);edit(&field,'b',0);
    assert(!strcmp(text,"abc")&&field.cursor==2);
    edit(&field,0,KEY_DELETE);assert(!strcmp(text,"ab"));
    edit(&field,'\b',0);assert(!strcmp(text,"a")&&field.cursor==1);
    edit(&field,0,KEY_HOME);edit(&field,'!',0);edit(&field,0,KEY_END);
    edit(&field,'*',0);edit(&field,'z',0);edit(&field,'x',0);
    assert(!strcmp(text,"!a*z")&&field.length==4&&field.cursor==4);
    edit(&field,0,KEY_HOME);edit(&field,'\b',0);assert(!strcmp(text,"!a*z"));
    struct key_event released={.character='a'};assert(!form_edit(&field,&released));
    char password[73]="";struct form_field secret={.text=password,.capacity=sizeof(password),.secret=true};
    for(unsigned i=0;i<100;i++)edit(&secret,'x',0);
    assert(secret.length==72&&secret.cursor==72&&password[72]==0);
    edit(&secret,0,KEY_HOME);edit(&secret,0,KEY_DELETE);edit(&secret,'!',0);
    assert(secret.length==72&&password[0]=='!'&&password[72]==0);

    struct mouse_decoder decoder={0};struct mouse_event event;
    assert(!mouse_decode(&decoder,0,&event));
    assert(!mouse_decode(&decoder,0x09,&event));assert(!mouse_decode(&decoder,200,&event));
    assert(mouse_decode(&decoder,10,&event)&&event.dx==200&&event.dy==-10&&event.buttons==1);
    assert(!mouse_decode(&decoder,0x38,&event));assert(!mouse_decode(&decoder,56,&event));
    assert(mouse_decode(&decoder,255,&event)&&event.dx==-200&&event.dy==1&&!event.buttons);
    assert(!mouse_decode(&decoder,0xc8,&event));assert(!mouse_decode(&decoder,0,&event));
    assert(!mouse_decode(&decoder,0,&event)&&decoder.count==0);

    uint32_t pixels[64*64],original[64*64];
    for(unsigned i=0;i<64*64;i++)pixels[i]=i;
    memcpy(original,pixels,sizeof(pixels));
    struct limine_framebuffer fb={.address=pixels,.width=64,.height=64,.pitch=64*4,
        .bpp=32,.memory_model=LIMINE_FRAMEBUFFER_RGB,.red_mask_size=8,.red_mask_shift=16,
        .green_mask_size=8,.green_mask_shift=8,.blue_mask_size=8};
    assert(fb_init(&fb));fb_pointer_show(10,10);assert(memcmp(pixels,original,sizeof(pixels)));
    fb_pointer_show(63,63);fb_pointer_hide();assert(!memcmp(pixels,original,sizeof(pixels)));
    fb_clear(0);fb_text(0,0,"*",0xffffff,2);assert(pixels[2*64]!=0);
    puts("PASS: bounded text editing, mouse packets, cursor restoration and password glyph");
}
