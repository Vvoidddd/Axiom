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
#include "process.h"
#include "memory.h"
#include "security.h"
#include <axiom/syscall.h>

enum app {HOME,TERMINAL,FILES,EDITOR,SETTINGS,TASKS};
static enum app app;
static bool active,dirty,unsaved,confirm_exit;
static int mx,my,width,height,wx,wy,ww,wh;
static struct window {int x,y,w,h;bool open,minimized,maximized;} windows[6];
static enum app order[5],dragging;
static unsigned window_count;
static int drag_x,drag_y;
static uint32_t *backbuffer;
static uint64_t terminal_revision;
static uint8_t buttons;
static unsigned selected,first,count,total;
static unsigned icon_focus,task_first,task_count;
static uint64_t task_refresh;
static char directory[256],app_status[6][96],document[4096];
#define status app_status[app]
static struct form_field editor;
static bool save_dialog,replace_confirm;
static char save_path[256];
static struct form_field filename;
static size_t editor_top;
static uint32_t wallpaper=0x152d46;
static struct {char name[48];bool directory;} entries[256];
static const char *titles[]={"Desktop","Terminal","Files","Text editor","Settings","Task Manager"};

static size_t length(const char*s){size_t n=0;while(s[n])n++;return n;}
static void copy(char*d,const char*s,size_t cap){size_t n=0;while(s[n]&&n+1<cap){d[n]=s[n];n++;}d[n]=0;}
static void number(char*out,uint64_t n){char b[20];unsigned k=0;do{b[k++]=(char)('0'+n%10);n/=10;}while(n);unsigned i=0;while(k)out[i++]=b[--k];out[i]=0;}
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
static void geometry(enum app target){
    struct window*w=&windows[target];
    wx=w->maximized?8:w->x;wy=w->maximized?42:w->y;
    ww=w->maximized?width-16:w->w;wh=w->maximized?height-98:w->h;
    if(target==TERMINAL)console_viewport(wx+12,wy+44,ww-24,wh-56);
}
static void raise_window(enum app target){
    unsigned at=0;while(at<window_count&&order[at]!=target)at++;
    if(at<window_count)for(unsigned i=at;i+1<window_count;i++)order[i]=order[i+1];
    else window_count++;
    order[window_count-1]=target;windows[target].open=true;windows[target].minimized=false;
}
static void front_window(void){
    app=HOME;
    for(unsigned i=window_count;i;i--)if(windows[order[i-1]].open&&!windows[order[i-1]].minimized){app=order[i-1];break;}
    if(app!=HOME)geometry(app);
    console_visible(app==TERMINAL);dirty=true;dragging=HOME;
}
static void minimize(void){if(app!=HOME){windows[app].minimized=true;front_window();}}
static void maximize(void){if(app!=HOME){windows[app].maximized=!windows[app].maximized;geometry(app);dirty=true;}}
static void switch_app(enum app next){
    app=next;confirm_exit=false;save_dialog=replace_confirm=false;console_visible(app==TERMINAL);
    dragging=HOME;
    if(app==HOME){for(unsigned i=1;i<=TASKS;i++)windows[i].minimized=true;}
    else{raise_window(app);geometry(app);}
    if(app==FILES)refresh();
    else if(app==TASKS){task_first=0;task_refresh=0;message("Read-only monitor. UP / DOWN scroll. CPU time is scheduler ticks.");}
    else status[0]=0;
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
    switch_app(EDITOR);message("F8: Save as. F9: Save a new copy in your home.");
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
    int error=vfs_replace_file(path,document,editor.length,false);
    if(error){message(vfs_error_string(error));return;}
    unsaved=false;copy(status,"Saved in home: ",sizeof(status));copy(status+15,name,sizeof(status)-15);dirty=true;
    if(!checkpoint_save())message("Saved in RAM only. Data disk checkpoint failed.");
    log_write("DESKTOP-SAVED",path);
}
static void begin_save_as(void){
    const struct account_info*u=account_current();if(!u)return;
    if(!joined(save_path,u->home,"document.txt")){message("Home path is too long.");return;}
    filename=(struct form_field){.text=save_path,.capacity=sizeof(save_path),.length=length(save_path),.cursor=length(save_path)};
    save_dialog=true;replace_confirm=false;message("Enter an absolute file path. ENTER saves; ESC cancels.");
}
static void save_as(void){
    if(save_path[0]!='/'){message("Use an absolute path starting with /.");return;}
    int error=vfs_replace_file(save_path,document,editor.length,replace_confirm);
    if(error==VFS_EEXIST){replace_confirm=true;message("File exists. ENTER or Replace confirms; ESC cancels.");return;}
    replace_confirm=false;
    if(error){message(vfs_error_string(error));return;}
    save_dialog=false;unsaved=false;message("Saved file. Original retained if saving to a different path.");
    if(!checkpoint_save())message("Saved in RAM only. Data disk checkpoint failed.");
    log_write("DESKTOP-SAVED",save_path);
}
static void draw_editor(void){
    button(wx+16,wy+44,152,"Save copy",false);
    button(wx+180,wy+44,152,"Save as",save_dialog);
    text(wx+350,wy+54,unsaved?"Unsaved draft":"Saved / unchanged",unsaved?0xffcf7b:0x91d6b0);
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
    if(save_dialog){
        fb_rect(wx+12,wy+84,ww-24,158,0x293b50);
        text(wx+24,wy+100,"SAVE AS - FULL PATH",0xf4f7fa);
        unsigned columns=(unsigned)(ww-72)/12;
        if(filename.cursor<filename.offset)filename.offset=filename.cursor;
        if(filename.cursor>=filename.offset+columns)filename.offset=filename.cursor-columns+1;
        char shown[256];copy(shown,save_path+filename.offset,columns+1);
        fb_rect(wx+24,wy+126,ww-48,38,0x161b22);text(wx+32,wy+138,shown,0xf4f7fa);
        fb_rect(wx+32+(filename.cursor-filename.offset)*12,wy+134,2,22,0x8bc3ff);
        button(wx+24,wy+184,152,replace_confirm?"Replace":"Save",true);
        button(wx+188,wy+184,152,"Cancel",false);
    }
}
static int icon_step(void){int step=(height-128)/5;return step<118?step:118;}
static void draw_icon(unsigned i){
    int x=48,y=64+(int)i*icon_step();enum app target=(enum app)(i+1);
    fb_rect(x-12,y-8,156,icon_step()-4,app==HOME&&icon_focus==i?0x294963:wallpaper);
    uint32_t ink=0x8bc3ff;
    if(target==TERMINAL){fb_rect(x+28,y,64,46,0x101318);fb_rect(x+28,y,64,6,ink);fb_text(x+36,y+16,">_",0x91d6b0,2);}
    else if(target==FILES){fb_rect(x+28,y+8,26,10,0xffcf7b);fb_rect(x+24,y+16,72,38,0xe8b55d);fb_rect(x+28,y+22,64,28,0xffcf7b);}
    else if(target==EDITOR){fb_rect(x+36,y,48,56,0xe8edf2);for(int j=0;j<4;j++)fb_rect(x+44,y+12+j*10,30,3,0x53758b);}
    else if(target==SETTINGS){for(int j=0;j<3;j++){fb_rect(x+28,y+10+j*16,64,3,ink);fb_rect(x+40+j*14,y+5+j*16,8,13,0xf4f7fa);}}
    else {fb_rect(x+26,y,68,54,0x101318);for(int j=0;j<4;j++)fb_rect(x+34+j*14,y+40-j*8,8,8+j*8,0x91d6b0);}
    text(x,y+68,target==EDITOR?"Editor":target==TASKS?"Task Manager":titles[target],0xf4f7fa);
}
static void metric(int x,int y,const char*label,uint64_t value){
    char digits[21];number(digits,value);
    text(x,y,label,0x9eb4ca);text(x,y+24,digits,0xf4f7fa);
}
static void task_cell(int x,int y,const char*s,int available,uint32_t color){fb_text(x,y,s,color,length(s)*12<=(unsigned)available?2:1);}
static unsigned task_rows(void){return (unsigned)(wh-214)/26;}
static void draw_tasks(void){
    metric(wx+20,wy+52,"Free RAM MiB",pmm_free_pages()/256);
    metric(wx+248,wy+52,"Heap KiB",heap_bytes_used()/1024);
    metric(wx+476,wy+52,"Uptime seconds",hardware_uptime_ms()/1000);
    text(wx+20,wy+132,"PID",0x8bc3ff);text(wx+100,wy+132,"TID",0x8bc3ff);
    text(wx+180,wy+132,"UID",0x8bc3ff);text(wx+280,wy+132,"STATE",0x8bc3ff);
    text(wx+430,wy+132,"TICKS",0x8bc3ff);text(wx+590,wy+132,"NAME",0x8bc3ff);
    uint32_t own;if(!process_current_identity(&own,0,0))return;
    bool audit=process_current_has_capability(CAP_AUDIT,hardware_uptime_ms());
    struct process_info visible[PROCESS_MAX];task_count=0;
    for(unsigned i=0;i<process_count()&&i<PROCESS_MAX;i++){
        struct process_info p;if(process_info_at(i,&p)&&(audit||p.real_uid==own))visible[task_count++]=p;
    }
    unsigned rows=task_rows();if(task_first>=task_count)task_first=task_count?task_count-1:0;
    for(unsigned i=task_first;i<task_count&&i<task_first+rows;i++){
        const struct process_info*p=&visible[i];int y=wy+166+(int)(i-task_first)*26;char b[33];
        number(b,p->pid);task_cell(wx+20,y,b,76,0xf4f7fa);
        number(b,p->tid);task_cell(wx+100,y,b,76,0xf4f7fa);
        number(b,p->real_uid);task_cell(wx+180,y,b,96,0xf4f7fa);
        task_cell(wx+280,y,process_state_name(p->state),146,p->state==PROCESS_ZOMBIE?0x8995a3:0x91d6b0);
        number(b,p->runtime_ticks);task_cell(wx+430,y,b,156,0xf4f7fa);
        copy(b,p->name,sizeof(b));task_cell(wx+590,y,b,ww-610,0xf4f7fa);
    }
    if(!task_count)text(wx+20,wy+166,"No visible processes.",0x8995a3);
    task_refresh=hardware_uptime_ms();
}
static void draw(void){
    enum app focused=app;
    fb_pointer_hide();fb_begin_frame(backbuffer,(size_t)width*height);fb_clear(wallpaper);
    fb_rect(width*2/3,38,width/3,height-86,0x1b3954);
    fb_rect(0,0,width,38,0x101c2a);
    text(20,12,"AXIOM",0xf4f7fa);
    const struct account_info*u=account_current();text(140,12,u?u->name:"Session",0x9eb4ca);
    text(width-312,12,"F1: DESKTOP  F7: TASKS",0x9eb4ca);
    fb_rect(0,height-48,width,48,0x101c2a);
    const char*labels[]={"Home","Terminal","Files","Editor","Settings"};
    for(unsigned i=0;i<5;i++)button(12+(int)i*126,height-40,118,labels[i],app==(enum app)i);
    for(unsigned i=1;i<5;i++)if(windows[i].open)fb_rect(20+i*126,height-7,102,2,windows[i].minimized?0x53758b:0x8bc3ff);
    button(width-240,height-40,100,"Lock",false);button(width-128,height-40,116,"Log out",false);
    button(width-326,height-40,76,"Tasks",app==TASKS);
    if(windows[TASKS].open)fb_rect(width-318,height-7,60,2,windows[TASKS].minimized?0x53758b:0x8bc3ff);
    {
        for(unsigned i=0;i<5;i++)draw_icon(i);
        fb_text(width/2-60,230,"AXIOM",0xf4f7fa,8);
        text(width/2-56,310,"Your system. Your rules.",0xa9c5df);
        text(240,height-100,"Click an icon or use TAB / arrows and ENTER.",0xa9c5df);
    }
    for(unsigned zi=0;zi<window_count;zi++){
        app=order[zi];if(!windows[app].open||windows[app].minimized)continue;
        geometry(app);fb_clip(0,38,width,height-86);
        fb_rect(wx+6,wy+6,ww,wh,0x0b1826);fb_rect(wx,wy,ww,wh,0x101318);
        fb_clip(wx,wy,ww,wh);
        fb_rect(wx,wy,ww,34,app==focused?0x293b50:0x25303c);text(wx+14,wy+10,titles[app],0xf4f7fa);
        button(wx+ww-126,wy+1,40,"_",false);button(wx+ww-84,wy+1,40,windows[app].maximized?"-":"+",false);
        button(wx+ww-42,wy+1,40,"X",false);
        if(app==TERMINAL){console_visible(true);console_redraw();console_visible(false);}
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
        if(app==TASKS)draw_tasks();
        if(app==SETTINGS){
            text(wx+24,wy+65,"APPEARANCE",0x9eb4ca);
            button(wx+24,wy+98,150,"Ocean",wallpaper==0x152d46);button(wx+190,wy+98,150,"Slate",wallpaper==0x242936);
            text(wx+24,wy+170,"KEYBOARD LAYOUT",0x9eb4ca);
            button(wx+24,wy+204,150,"US",false);button(wx+190,wy+204,150,"Dvorak",false);
            text(wx+24,wy+260,keyboard_layout_name(),0x91d6b0);
            text(wx+24,wy+310,"Use Terminal > settings for all system settings.",0xf4f7fa);
            text(wx+24,wy+348,"Wallpaper applies to this session only.",0x9eb4ca);
        }
        if(app!=TERMINAL){char shown[70];copy(shown,status,sizeof(shown));text(wx+16,wy+wh-24,shown,0xffcf7b);}
        fb_clip_reset();
    }
    app=focused;if(app!=HOME)geometry(app);console_visible(app==TERMINAL);
    if(confirm_exit){fb_rect(24,height-90,width-48,32,0x593845);text(36,height-81,"Unsaved editor draft. Save it, or click the session button again to discard.",0xffffff);}
    dirty=false;terminal_revision=console_revision();fb_present();fb_pointer_show(mx,my);
}
void desktop_start(void){
    width=(int)fb_width();height=(int)fb_height();
    if(width<960||height<600){active=false;console_init();return;}
    active=true;wx=32;wy=64;ww=width-160;wh=height-192;
    window_count=0;dragging=HOME;
    for(unsigned i=0;i<=TASKS;i++){windows[i]=(struct window){.x=wx,.y=wy,.w=ww,.h=wh};app_status[i][0]=0;}
    /* ponytail: cap the software surface at 16 MiB; larger modes draw directly. */
    backbuffer=(uint64_t)width*height<=4194304?kmalloc((size_t)width*height*4):0;
    mx=width/2;my=height/2;buttons=0;icon_focus=0;unsaved=confirm_exit=false;status[0]=document[0]=0;
    editor=(struct form_field){.text=document,.capacity=sizeof(document)};editor_top=0;
    home_path();console_viewport(wx+12,wy+44,ww-24,wh-56);console_clear();
    switch_app(HOME);draw();log_write("DESKTOP","session ready");
}
void desktop_stop(void){active=false;fb_pointer_hide();fb_present();kfree(backbuffer);backbuffer=0;console_init();document[0]=0;unsaved=false;}
bool desktop_active(void){return active;}
static long session_action(long action){
    if(unsaved&&!confirm_exit){confirm_exit=true;dirty=true;return 0;}
    return action;
}
static long click(void){
    if(save_dialog){
        if(inside(wx+24,wy+184,152,32))save_as();
        else if(inside(wx+188,wy+184,152,32)){save_dialog=replace_confirm=false;message("Save cancelled. Draft retained.");}
        return 0;
    }
    if(my>=height-48){
        if(inside(width-326,height-40,76,32)){if(app==TASKS)minimize();else switch_app(TASKS);return 0;}
        for(unsigned i=0;i<5;i++)if(inside(12+(int)i*126,height-40,118,32)){if(i&&app==(enum app)i)minimize();else switch_app((enum app)i);return 0;}
        if(inside(width-240,height-40,100,32))return session_action(AXIOM_KEY_SESSION_LOCK);
        if(inside(width-128,height-40,116,32))return session_action(AXIOM_KEY_SESSION_LOGOUT);
    }
    enum app hit=HOME;
    for(unsigned i=window_count;i;i--){enum app candidate=order[i-1];if(!windows[candidate].open||windows[candidate].minimized)continue;geometry(candidate);if(inside(wx,wy,ww,wh)){hit=candidate;break;}}
    if(hit==HOME){app=HOME;console_visible(false);dirty=true;for(unsigned i=0;i<5;i++)if(inside(36,56+(int)i*icon_step(),156,icon_step()-4)){icon_focus=i;switch_app((enum app)(i+1));break;}return 0;}
    bool was_focused=app==hit;
    if(!was_focused)switch_app(hit);else geometry(hit);
    if(inside(wx+ww-42,wy,42,34)){windows[app].open=false;front_window();return 0;}
    if(inside(wx+ww-84,wy,42,34)){maximize();return 0;}
    if(inside(wx+ww-126,wy,42,34)){minimize();return 0;}
    if(inside(wx,wy,ww-126,34)){
        if(!windows[app].maximized){dragging=app;drag_x=mx-wx;drag_y=my-wy;}return 0;
    }
    if(!was_focused)return 0;
    if(app==FILES){
        if(inside(wx+16,wy+44,78,32))parent_path();
        else if(inside(wx+104,wy+44,92,32)){home_path();refresh();}
        else if(inside(wx+206,wy+44,92,32)){copy(directory,"/",sizeof(directory));refresh();}
        else if(inside(wx+12,wy+118,ww-24,(int)page_rows()*26)){selected=first+(unsigned)(my-wy-118)/26;open_entry();}
    }else if(app==EDITOR){
        if(inside(wx+16,wy+44,152,32))save_copy();
        else if(inside(wx+180,wy+44,152,32))begin_save_as();
    }
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
        if(dragging!=HOME&&(mouse.buttons&1)){
            struct window*w=&windows[dragging];w->x=mx-drag_x;w->y=my-drag_y;
            if(w->x<8)w->x=8;
            if(w->x>width-w->w-8)w->x=width-w->w-8;
            if(w->y<42)w->y=42;
            if(w->y>height-w->h-56)w->y=height-w->h-56;
            geometry(dragging);dirty=true;
        }
        if(!(mouse.buttons&1))dragging=HOME;
        long action=0;if((mouse.buttons&1)&&!(buttons&1))action=click();buttons=mouse.buttons;
        if(action)return action;
    }
    struct key_event e;long value=0;
    if(keyboard_poll_event(&e)&&e.pressed){
        if(save_dialog){
            if(e.scancode==1){save_dialog=replace_confirm=false;message("Save cancelled. Draft retained.");}
            else if(e.character=='\n')save_as();
            else if(form_edit(&filename,&e)){replace_confirm=false;message("ENTER saves; ESC cancels.");}
        }
        else if(app==EDITOR&&e.scancode==0x42)begin_save_as();
        else if(app==EDITOR&&e.scancode==0x43)save_copy();
        else if(e.scancode==0x44)maximize();
        else if(e.scancode==0x57)minimize();
        else if(e.scancode==0x58){for(unsigned i=1;i<=TASKS;i++){enum app next=(enum app)(1+(app+i-1)%TASKS);if(windows[next].open){switch_app(next);break;}}}
        else if(e.scancode>=0x3b&&e.scancode<=0x3f)switch_app((enum app)(e.scancode-0x3b));
        else if(e.scancode==0x41)switch_app(TASKS);
        else if(e.scancode==0x40){if(app==HOME||app==TERMINAL)switch_app(SETTINGS);message(account_save()&&checkpoint_save()?checkpoint_status():"Save failed. Keep the VM running and preserve your work.");}
        else if(app==HOME&&(e.special==KEY_HOME||e.special==KEY_END)){icon_focus=e.special==KEY_HOME?0:4;dirty=true;}
        else if(app==HOME&&(e.character=='\t'||e.special==KEY_DOWN||e.special==KEY_UP||e.special==KEY_LEFT||e.special==KEY_RIGHT)){
            bool back=e.special==KEY_UP||e.special==KEY_LEFT||(e.character=='\t'&&e.shift);icon_focus=(icon_focus+(back?4:1))%5;dirty=true;
        }
        else if(app==HOME&&(e.character=='\n'||e.character==' '))switch_app((enum app)(icon_focus+1));
        else if(app==HOME&&e.character>=32&&e.character<=126){switch_app(TERMINAL);value=(unsigned char)e.character;}
        else if(app==TERMINAL)value=(unsigned char)e.character;
        else if(app==TASKS){if(e.special==KEY_UP&&task_first)task_first--;if(e.special==KEY_DOWN&&task_first+task_rows()<task_count)task_first++;dirty=true;}
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
    if(windows[TASKS].open&&!windows[TASKS].minimized&&hardware_uptime_ms()-task_refresh>=1000)dirty=true;
    if(windows[TERMINAL].open&&!windows[TERMINAL].minimized&&console_revision()!=terminal_revision)dirty=true;
    if(dirty)draw();else fb_pointer_show(mx,my);
    return value;
}
