#include "desktop.h"
#include "console.h"
#include "framebuffer.h"
#include "hardware.h"
#include "mouse.h"
#include "vfs.h"
#include "account.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static struct key_event key;
static struct mouse_event mouse;
static bool has_key,has_mouse;
static int px=512,py=384;
static const struct account_info user={.uid=1,.gid=1,.name="test",.home="/users/test"};
void *kmalloc(size_t n){return malloc(n);}
void kfree(void*p){free(p);}
uint64_t hardware_uptime_ms(void){return 1000;}
const struct account_info *account_current(void){return &user;}
void log_write(const char*a,const char*b){(void)a;(void)b;}
const char *keyboard_layout_name(void){return "us";}
bool config_user_set(const char*a,const char*b){(void)a;(void)b;return true;}
bool checkpoint_active(void){return false;}
bool checkpoint_save(void){return true;}
const char*checkpoint_status(void){return "Live session";}
bool account_save(void){return true;}
bool keyboard_poll_event(struct key_event*out){if(!has_key)return false;*out=key;has_key=false;return true;}
bool mouse_poll_event(struct mouse_event*out){if(!has_mouse)return false;*out=mouse;has_mouse=false;return true;}
static long press(char c,unsigned scan){key=(struct key_event){.pressed=true,.character=c,.scancode=scan};has_key=true;return desktop_poll();}
static void type(const char*s){while(*s)press(*s++,0);}
static long click(int x,int y){
    mouse=(struct mouse_event){.dx=x-px,.dy=y-py,.buttons=1};has_mouse=true;
    px=x;py=y;long result=desktop_poll();
    mouse=(struct mouse_event){0};has_mouse=true;desktop_poll();return result;
}
static void contents(const char*path,const char*expected){
    char b[4096];int fd=vfs_open(path,VFS_READ);assert(fd>=0);
    long n=vfs_read(fd,b,sizeof(b));assert(n==(long)strlen(expected)&&!memcmp(b,expected,n));assert(!vfs_close(fd));
}
int main(void){
    uint32_t*pixels=calloc(1024*768,4);assert(pixels);
    struct limine_framebuffer fb={.address=pixels,.width=1024,.height=768,.pitch=4096,.bpp=32,
        .memory_model=LIMINE_FRAMEBUFFER_RGB,.red_mask_size=8,.red_mask_shift=16,
        .green_mask_size=8,.green_mask_shift=8,.blue_mask_size=8};
    assert(fb_init(&fb));vfs_init();assert(!vfs_mkdir(user.home,0700));assert(!vfs_chown(user.home,1,1));
    vfs_set_credentials(1,1);console_init();desktop_start();assert(desktop_active());
    press(0,0x3c); // F2
    for(unsigned i=0;i<200;i++)console_write("scrolling inside a desktop terminal\n");
    assert(pixels[0]==0x101c2a&&pixels[735*1024]==0x101c2a);
    press(0,0x3e);type("hello desktop\nsecond line");
    assert(!click(830,745)); // Unsaved draft requires a second session action.
    press(0,0x3e); // Cancel pending exit by returning to editor.
    click(85,125);contents("/users/test/note-1.txt","hello desktop\nsecond line");
    press(0,0x3d);click(160,190); // F3 then first file row: read the saved file.
    type("!");click(85,125);
    contents("/users/test/note-1.txt","hello desktop\nsecond line");
    contents("/users/test/note-2.txt","hello desktop\nsecond line!");
    // Allocation failure must preserve the draft and previously saved files.
    vfs_set_credentials(0,0);assert(!vfs_set_quota(1,8192));vfs_set_credentials(1,1);
    type("?");click(85,125);
    struct vfs_stat st;assert(vfs_stat_path("/users/test/note-3.txt",&st)==VFS_ENOENT);
    vfs_set_credentials(0,0);assert(!vfs_set_quota(1,16384));vfs_set_credentials(1,1);
    click(85,125);
    contents("/users/test/note-3.txt","hello desktop\nsecond line!?");
    assert(click(830,745)==0x100);
    desktop_stop();assert(!desktop_active());
    vfs_set_credentials(0,0);assert(!vfs_unlink(user.home,true));free(pixels);
    puts("PASS: desktop file round trips, non-destructive saves, quota failure, draft guard and terminal clipping");
}
