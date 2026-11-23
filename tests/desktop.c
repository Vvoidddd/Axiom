#include "desktop.h"
#include "console.h"
#include "framebuffer.h"
#include "hardware.h"
#include "mouse.h"
#include "vfs.h"
#include "account.h"
#include "process.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static struct key_event key;
static struct mouse_event mouse;
static bool has_key,has_mouse;
static int px=512,py=384;
static uint64_t clock_ms=1000;
static bool fail_alloc;
static const struct account_info user={.uid=1,.gid=1,.name="test",.home="/users/test"};
void *kmalloc(size_t n){return fail_alloc?NULL:malloc(n);}
void kfree(void*p){free(p);}
uint64_t hardware_uptime_ms(void){return clock_ms;}
uint64_t pmm_free_pages(void){return 16384;}
uint64_t heap_bytes_used(void){return 4096;}
uint32_t process_count(void){return 2;}
bool process_info_at(uint32_t i,struct process_info*p){if(i>=2)return false;*p=(struct process_info){.pid=i+1,.tid=i+1,.real_uid=i+1,.state=PROCESS_READY,.runtime_ticks=clock_ms};strcpy(p->name,i?"PRIVATE":"owner");return true;}
const char*process_state_name(enum process_state state){(void)state;return "ready";}
bool process_current_identity(uint32_t*r,uint32_t*e,uint32_t*g){if(r)*r=1;if(e)*e=1;if(g)*g=1;return true;}
bool process_current_has_capability(uint64_t cap,uint64_t now){(void)cap;(void)now;return false;}
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
static void save_name(const char*path){press(0,0x42);for(size_t i=0;i<strlen(user.home)+strlen("/document.txt");i++)press('\b',0);type(path);}
static long click(int x,int y){
    mouse=(struct mouse_event){.dx=x-px,.dy=y-py,.buttons=1};has_mouse=true;
    px=x;py=y;long result=desktop_poll();
    mouse=(struct mouse_event){0};has_mouse=true;desktop_poll();return result;
}
static void drag(int x,int y,int to_x,int to_y){
    mouse=(struct mouse_event){.dx=x-px,.dy=y-py,.buttons=1};has_mouse=true;desktop_poll();
    mouse=(struct mouse_event){.dx=to_x-x,.dy=to_y-y,.buttons=1};has_mouse=true;desktop_poll();
    px=to_x;py=to_y;mouse=(struct mouse_event){0};has_mouse=true;desktop_poll();
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
    click(100,570); // Desktop Task Manager icon.
    fb_pointer_hide();
    for(unsigned y=256;y<270;y++)for(unsigned x=52;x<896;x++)assert(pixels[y*1024+x]==0x101318); // Other user's row absent.
    uint32_t before[160*16];for(unsigned y=0;y<16;y++)memcpy(before+y*160,pixels+(230+y)*1024+462,160*4);
    clock_ms+=1000;desktop_poll();fb_pointer_hide();
    bool changed=false;for(unsigned y=0;y<16;y++)if(memcmp(before+y*160,pixels+(230+y)*1024+462,160*4))changed=true;
    assert(changed); // Runtime ticks refresh without further input.
    press(0,0x3b);key=(struct key_event){.pressed=true,.special=KEY_HOME};has_key=true;desktop_poll();
    press('\t',0);press('\n',0); // Keyboard launches Files.
    assert(pixels[728*1024+264]==0x315e97);
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
    save_name("/users/test/chosen.txt");press('\n',0);
    contents("/users/test/chosen.txt","hello desktop\nsecond line!?");
    type("new");save_name("/users/test/chosen.txt");press('\n',0);
    contents("/users/test/chosen.txt","hello desktop\nsecond line!?"); // Confirmation required.
    press(0,1); // Escape retains draft and old file.
    save_name("/users/test/chosen.txt");press('\n',0);press('\n',0);
    contents("/users/test/chosen.txt","hello desktop\nsecond line!?new");
    type("!");assert(!vfs_lock("/users/test/chosen.txt",true));
    save_name("/users/test/chosen.txt");press('\n',0);press('\n',0);
    contents("/users/test/chosen.txt","hello desktop\nsecond line!?new");
    assert(!vfs_lock("/users/test/chosen.txt",false));press('\n',0);press('\n',0);
    contents("/users/test/chosen.txt","hello desktop\nsecond line!?new!");
    assert(click(830,745)==0x100);
    press(0,0x3b);press(0,0x3c);press(0,0x3d); // Home, Terminal, Files: two visible windows.
    drag(200,80,300,140);fb_pointer_hide();
    assert(pixels[64*1024+32]==0x25303c&&pixels[124*1024+132]==0x293b50);
    console_write("background output must not paint over Files\n");desktop_poll();fb_pointer_hide();
    assert(pixels[124*1024+132]==0x293b50);
    click(60,80);fb_pointer_hide();assert(pixels[64*1024+32]==0x293b50);
    click(950,200);fb_pointer_hide();assert(pixels[124*1024+132]==0x293b50);
    press(0,0x57);fb_pointer_hide();assert(pixels[200*1024+950]==0x1b3954); // F11 minimize.
    press(0,0x3d);press(0,0x44);fb_pointer_hide();assert(pixels[42*1024+8]==0x293b50); // F10 maximize.
    press(0,0x44);fb_pointer_hide();assert(pixels[124*1024+132]==0x293b50);
    click(974,139);fb_pointer_hide();assert(pixels[64*1024+32]==0x293b50); // Close Files reveals Terminal.
    press(0,0x58);assert(pixels[728*1024+390]==0x315e97); // F12 skips closed Files and restores Editor.
    press(0,0x57);click(790,80);fb_pointer_hide();assert(pixels[64*1024+32]==0x152d46); // Minimize Terminal with its title-bar control.
    press(0,0x3c);drag(200,80,2000,2000);fb_pointer_hide();
    assert(pixels[136*1024+152]==0x293b50&&pixels[735*1024]==0x101c2a); // Clamp dragging above the taskbar.
    desktop_stop();assert(!desktop_active());
    fb.height=600;assert(fb_init(&fb));fail_alloc=true;desktop_start();fail_alloc=false;px=512;py=300;click(100,470); // 600p and direct-render fallback.
    assert(pixels[64*1024+32]==0x293b50);desktop_stop();
    vfs_set_credentials(0,0);assert(!vfs_unlink(user.home,true));free(pixels);
    puts("PASS: window dragging, focus, minimize/maximize, cycling, clipping, background output, Save As and allocation fallback");
}
