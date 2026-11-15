#include "desktop.h"
#include "framebuffer.h"
#include "console.h"
#include "hardware.h"
#include "mouse.h"
#include "vfs.h"
#include "account.h"
#include "config.h"
#include "form.h"
#include "log.h"
#include "checkpoint.h"
#include <axiom/syscall.h>

enum app {HOME,TERMINAL,FILES,EDITOR,SETTINGS};
static enum app app;
static bool active,dirty,unsaved,confirm_exit;
static int mx,my,width,height,wx,wy,ww,wh;
static uint8_t buttons;
static unsigned selected,first,count,total;
static char directory[256],status[96],document[4096];
static struct form_field editor;
static size_t editor_top;
static uint32_t wallpaper=0x152d46;
static struct {char name[48];bool directory;} entries[256];
static const char *titles[]={"Desktop","Terminal","Files","Text editor","Settings"};

static size_t length(const char*s){size_t n=0;while(s[n])n++;return n;}
static void copy(char*d,const char*s,size_t cap){size_t n=0;while(s[n]&&n+1<cap){d[n]=s[n];n++;}d[n]=0;}
static void number(char*out,unsigned n){char b[12];unsigned k=0;do{b[k++]=(char)('0'+n%10);n/=10;}while(n);unsigned i=0;while(k)out[i++]=b[--k];out[i]=0;}
static void message(const char*s){copy(status,s,sizeof(status));dirty=true;}
static bool inside(int x,int y,int w,int h){return mx>=x&&mx<x+w&&my>=y&&my<y+h;}
static void text(int x,int y,const char*s,uint32_t color){fb_text(x,y,s,color,2);}
static void button(int x,int y,int w,const char*s,bool chosen){fb_rect(x,y,w,32,chosen?0x315e97:0x293b50);text(x+10,y+9,s,0xf4f7fa);}
static unsigned page_rows(void){return (unsigned)(wh-156)/26;}
static void collect(const char*name,const struct vfs_stat*st,void*ctx){
    (void)ctx;total++;
    if(count<256){copy(entries[count].name,name,sizeof(entries[count].name));entries[count++].directory=st->directory;}
}
static void refresh(void){
    count=total=0;selected=first=0;
    int error=vfs_list(directory,collect,0);
    message(error?vfs_error_string(error):total>256?"Showing first 256 entries.":"Click an entry to open. UP / DOWN and ENTER also work.");
}
static void switch_app(enum app next){
    app=next;confirm_exit=false;console_visible(app==TERMINAL);
    if(app==FILES)refresh();
    dirty=true;log_write("DESKTOP",titles[app]);
}
static void home_path(void){const struct account_info*u=account_current();copy(directory,u?u->home:"/",sizeof(directory));}
static void parent_path(void){size_t n=length(directory);while(n>1&&directory[n-1]!='/')n--;if(n>1)n--;directory[n]=0;refresh();}
static bool joined(char*out,const char*dir,const char*name){
    size_t a=length(dir),b=length(name);
    if(a+b+2>256)return false;
    copy(out,dir,256);if(a>1)out[a++]='/';copy(out+a,name,256-a);return true;
}
static void open_entry(void){
    if(selected>=count)return;
    char path[256];if(!joined(path,directory,entries[selected].name)){message("Path is too long.");return;}
    if(entries[selected].directory){copy(directory,path,sizeof(directory));refresh();return;}
    if(unsaved){message("Save your editor draft before opening another file.");return;}
    struct vfs_stat st;int error=vfs_stat_path(path,&st);
    if(error){message(vfs_error_string(error));return;}
    if(st.size>=sizeof(document)){message("Editor supports text files smaller than 4096 bytes.");return;}
    int fd=vfs_open(path,VFS_READ);if(fd<0){message(vfs_error_string(fd));return;}
    char data[4096];long n=vfs_read(fd,data,sizeof(data)-1);vfs_close(fd);
    if(n<0){message(vfs_error_string((int)n));return;}
    for(long i=0;i<n;i++)if((data[i]<32&&data[i]!='\n')||data[i]>126){message("Only printable ASCII text and newlines are supported.");return;}
    for(long i=0;i<n;i++)document[i]=data[i];
    document[n]=0;
    editor.length=editor.cursor=(size_t)n;editor_top=0;unsaved=false;
    switch_app(EDITOR);message("Opened text. Save copy writes a new file in your home.");
}
static void save_copy(void){
    const struct account_info*u=account_current();if(!u)return;
    char name[48],digits[12],path[256];struct vfs_stat st;
    unsigned i;for(i=1;i<=999;i++){
        number(digits,i);copy(name,"note-",sizeof(name));copy(name+5,digits,sizeof(name)-5);
        copy(name+length(name),".txt",sizeof(name)-length(name));
        if(!joined(path,u->home,name)){message("Home path is too long.");return;}
        int result=vfs_stat_path(path,&st);
        if(result==VFS_ENOENT)break;
        if(result){message(vfs_error_string(result));return;}
    }
    if(i>999){message("No free note names. Rename old notes in Terminal.");return;}
    /* Save as a fresh file: vfs_rename cannot atomically replace an existing file. */
    int fd=vfs_open(path,VFS_WRITE|VFS_CREATE);
    if(fd<0){message(vfs_error_string(fd));return;}
    long written=vfs_write(fd,document,editor.length);int closed=vfs_close(fd);
    if(written!=(long)editor.length||closed){vfs_unlink(path,false);message("Save failed. Your draft is still here; original files unchanged.");return;}
    unsaved=false;copy(status,"Saved in home: ",sizeof(status));copy(status+15,name,sizeof(status)-15);dirty=true;
    if(!checkpoint_save())message("Saved in RAM only. Data disk checkpoint failed.");
    log_write("DESKTOP-SAVED",path);
}
static void draw_editor(void){
    button(wx+16,wy+44,152,"Save copy",false);
    text(wx+185,wy+54,unsaved?"Unsaved draft":"Saved / unchanged",unsaved?0xffcf7b:0x91d6b0);
    unsigned columns=(unsigned)(ww-40)/12,rows=(unsigned)(wh-140)/18;
    size_t caret_row=0,caret_col=0;
    for(size_t i=0;i<editor.cursor;i++){if(document[i]=='\n'||++caret_col>=columns){caret_row++;caret_col=0;}}
    if(caret_row<editor_top)editor_top=caret_row;
    if(caret_row>=editor_top+rows)editor_top=caret_row-rows+1;
    size_t row=0,col=0;
    for(size_t i=0;i<editor.length;i++){
        char c=document[i];
        if(c!='\n'&&row>=editor_top&&row<editor_top+rows){char s[]={c,0};text(wx+20+(int)col*12,wy+90+(int)(row-editor_top)*18,s,0xe8edf2);}
        if(c=='\n'||++col>=columns){row++;col=0;}
    }
    fb_rect(wx+20+caret_col*12,wy+88+(caret_row-editor_top)*18,2,18,0x8bc3ff);
}
static void draw(void){
    fb_pointer_hide();fb_clear(wallpaper);
    fb_rect(width*2/3,38,width/3,height-86,0x1b3954);
    fb_rect(0,0,width,38,0x101c2a);
    text(20,12,"AXIOM",0xf4f7fa);
    const struct account_info*u=account_current();text(140,12,u?u->name:"Session",0x9eb4ca);
    text(width-312,12,"F1: DESKTOP  F2: TERMINAL",0x9eb4ca);
    fb_rect(0,height-48,width,48,0x101c2a);
    const char*labels[]={"Home","Terminal","Files","Editor","Settings"};
    for(unsigned i=0;i<5;i++)button(12+(int)i*126,height-40,118,labels[i],app==(enum app)i);
    button(width-240,height-40,100,"Lock",false);button(width-128,height-40,116,"Log out",false);
    button(width-326,height-40,76,"Save",false);
    if(app==HOME){
        fb_text(64,130,"AXIOM",0xf4f7fa,8);
        text(68,210,"Your system. Your rules.",0xa9c5df);
        text(68,280,"Open an app from the panel below.",0xf4f7fa);
        text(68,316,"F2 Terminal   F3 Files   F4 Editor   F5 Settings",0xa9c5df);
        text(68,380,checkpoint_active()?"PERSISTENT DATA DISK":"LIVE SESSION",0xffcf7b);
        text(68,410,checkpoint_active()?"Save commits your files and accounts to disk.":"Files and accounts are RAM-backed in this build.",0xa9c5df);
        text(68,440,checkpoint_active()?"Use Save / F6 before powering off the VM.":"They do not survive a reboot.",0xa9c5df);
    }else{
        fb_rect(wx+6,wy+6,ww,wh,0x0b1826);fb_rect(wx,wy,ww,wh,0x101318);
        fb_rect(wx,wy,ww,34,0x293b50);text(wx+14,wy+10,titles[app],0xf4f7fa);
        button(wx+ww-42,wy+1,40,"X",false);
        if(app==TERMINAL)console_redraw();
        if(app==FILES){
            button(wx+16,wy+44,78,"Up",false);button(wx+104,wy+44,92,"Home",false);button(wx+206,wy+44,92,"Root",false);
            char shown[65];copy(shown,directory,sizeof(shown));text(wx+16,wy+90,shown,0x9eb4ca);
            for(unsigned i=first;i<count&&i<first+page_rows();i++){
                int y=wy+122+(int)(i-first)*26;
                if(i==selected)fb_rect(wx+12,y-4,ww-24,26,0x243f5b);
                text(wx+20,y,entries[i].directory?"DIR":"TXT",entries[i].directory?0x8bc3ff:0x91d6b0);
                text(wx+78,y,entries[i].name,0xf4f7fa);
            }
        }
        if(app==EDITOR)draw_editor();
        if(app==SETTINGS){
            text(wx+24,wy+65,"APPEARANCE",0x9eb4ca);
            button(wx+24,wy+98,150,"Ocean",wallpaper==0x152d46);button(wx+190,wy+98,150,"Slate",wallpaper==0x242936);
            text(wx+24,wy+170,"KEYBOARD LAYOUT",0x9eb4ca);
            button(wx+24,wy+204,150,"US",false);button(wx+190,wy+204,150,"Dvorak",false);
            text(wx+24,wy+260,keyboard_layout_name(),0x91d6b0);
            text(wx+24,wy+310,"Use Terminal > settings for all system settings.",0xf4f7fa);
            text(wx+24,wy+348,"Wallpaper applies to this session only.",0x9eb4ca);
        }
    }
    if(app!=TERMINAL&&app!=HOME){char shown[70];copy(shown,status,sizeof(shown));text(wx+16,wy+wh-24,shown,0xffcf7b);}
    if(confirm_exit){fb_rect(24,height-90,width-48,32,0x593845);text(36,height-81,"Unsaved editor draft. Save it, or click the session button again to discard.",0xffffff);}
    dirty=false;fb_pointer_show(mx,my);
}
void desktop_start(void){
    width=(int)fb_width();height=(int)fb_height();
    if(width<960||height<600){active=false;console_init();return;}
    active=true;wx=32;wy=64;ww=width-64;wh=height-144;
    mx=width/2;my=height/2;buttons=0;unsaved=confirm_exit=false;status[0]=document[0]=0;
    editor=(struct form_field){.text=document,.capacity=sizeof(document)};editor_top=0;
    home_path();console_viewport(wx+12,wy+44,ww-24,wh-56);console_clear();
    switch_app(HOME);draw();log_write("DESKTOP","session ready");
}
void desktop_stop(void){active=false;fb_pointer_hide();console_init();document[0]=0;unsaved=false;}
bool desktop_active(void){return active;}
static long session_action(long action){
    if(unsaved&&!confirm_exit){confirm_exit=true;dirty=true;return 0;}
    return action;
}
static long click(void){
    if(my>=height-48){
        if(inside(width-326,height-40,76,32)){message(account_save()&&checkpoint_save()?checkpoint_status():"Save failed. Keep the VM running and preserve your work.");if(app==HOME)switch_app(SETTINGS);return 0;}
        for(unsigned i=0;i<5;i++)if(inside(12+(int)i*126,height-40,118,32)){switch_app((enum app)i);return 0;}
        if(inside(width-240,height-40,100,32))return session_action(AXIOM_KEY_SESSION_LOCK);
        if(inside(width-128,height-40,116,32))return session_action(AXIOM_KEY_SESSION_LOGOUT);
    }
    if(app==HOME)return 0;
    if(inside(wx+ww-42,wy,42,34)){switch_app(HOME);return 0;}
    if(app==FILES){
        if(inside(wx+16,wy+44,78,32))parent_path();
        else if(inside(wx+104,wy+44,92,32)){home_path();refresh();}
        else if(inside(wx+206,wy+44,92,32)){copy(directory,"/",sizeof(directory));refresh();}
        else if(inside(wx+12,wy+118,ww-24,(int)page_rows()*26)){selected=first+(unsigned)(my-wy-118)/26;open_entry();}
    }else if(app==EDITOR&&inside(wx+16,wy+44,152,32))save_copy();
    else if(app==SETTINGS){
        if(inside(wx+24,wy+98,150,32)){wallpaper=0x152d46;dirty=true;}
        else if(inside(wx+190,wy+98,150,32)){wallpaper=0x242936;dirty=true;}
        else if(inside(wx+24,wy+204,150,32))message(config_user_set("keyboard","us")?"Keyboard set to US.":"Could not save keyboard layout.");
        else if(inside(wx+190,wy+204,150,32))message(config_user_set("keyboard","dvorak")?"Keyboard set to Dvorak.":"Could not save keyboard layout.");
    }
    return 0;
}
long desktop_poll(void){
    struct mouse_event mouse;unsigned budget=32;
    while(budget--&&mouse_poll_event(&mouse)){
        fb_pointer_hide();mx+=mouse.dx;my+=mouse.dy;
        if(mx<0)mx=0;
        if(my<0)my=0;
        if(mx>=width)mx=width-1;
        if(my>=height)my=height-1;
        long action=0;if((mouse.buttons&1)&&!(buttons&1))action=click();buttons=mouse.buttons;
        if(action)return action;
    }
    struct key_event e;long value=0;
    if(keyboard_poll_event(&e)&&e.pressed){
        if(e.scancode>=0x3b&&e.scancode<=0x3f)switch_app((enum app)(e.scancode-0x3b));
        else if(e.scancode==0x40){message(account_save()&&checkpoint_save()?checkpoint_status():"Save failed. Keep the VM running and preserve your work.");if(app==HOME)switch_app(SETTINGS);}
        else if(app==HOME&&e.character>=32&&e.character<=126){switch_app(TERMINAL);value=(unsigned char)e.character;}
        else if(app==TERMINAL)value=(unsigned char)e.character;
        else if(app==FILES){
            if(e.special==KEY_DOWN&&selected+1<count)selected++;
            if(e.special==KEY_UP&&selected)selected--;
            if(e.character=='\n')open_entry();
            if(e.character=='\b')parent_path();
            if(selected<first)first=selected;
            if(selected>=first+page_rows())first=selected-page_rows()+1;
            dirty=true;
        }else if(app==EDITOR){
            if(e.character=='\n'&&editor.length+1<editor.capacity){
                for(size_t i=editor.length+1;i>editor.cursor;i--)document[i]=document[i-1];
                document[editor.cursor++]='\n';editor.length++;unsaved=true;dirty=true;
            }else {size_t n=editor.length;char c=e.character;
                if(form_edit(&editor,&e)){if(editor.length!=n||(c>=32&&c<=126))unsaved=true;dirty=true;}
            }
            confirm_exit=false;
        }
    }
    if(dirty)draw();else fb_pointer_show(mx,my);
    return value;
}
