#include "form.h"
#include "framebuffer.h"
#include "mouse.h"
#include "log.h"
#include "account.h"

enum form_error form_validate_setup(const char*name,const char*password,const char*confirmation){
    if(!account_name_valid(name))return FORM_NAME;
    size_t n=0;while(password&&password[n])n++;
    if(n<8||n>72)return FORM_PASSWORD;
    if(!confirmation)return FORM_CONFIRM;
    for(size_t i=0;i<=n;i++)if(password[i]!=confirmation[i])return FORM_CONFIRM;
    return FORM_VALID;
}
void form_message(const char*message){
    fb_pointer_hide();fb_rect(250,520,524,24,0x101318);
    fb_text(250,524,message,0xff667b,2);
}

bool form_edit(struct form_field*f,const struct key_event*e){
    if(!f||!e||!f->text||!f->capacity||!e->pressed)return false;
    if(e->special==KEY_LEFT){if(f->cursor)f->cursor--;}
    else if(e->special==KEY_RIGHT){if(f->cursor<f->length)f->cursor++;}
    else if(e->special==KEY_HOME)f->cursor=0;
    else if(e->special==KEY_END)f->cursor=f->length;
    else if(e->special==KEY_DELETE){
        if(f->cursor<f->length){for(size_t i=f->cursor;i<f->length;i++)f->text[i]=f->text[i+1];f->length--;}
    }else if(e->character=='\b'){
        if(f->cursor){for(size_t i=f->cursor-1;i<f->length;i++)f->text[i]=f->text[i+1];f->cursor--;f->length--;}
    }else if(e->character>=32&&e->character<=126){
        if(f->length+1>=f->capacity)return false;
        for(size_t i=f->length+1;i>f->cursor;i--)f->text[i]=f->text[i-1];
        f->text[f->cursor++]=e->character;f->length++;
    }else return false;
    f->text[f->length]=0;return true;
}
static void draw_field(struct form_field*f,unsigned y,bool active,bool caret){
    if(f->cursor<f->offset)f->offset=f->cursor;
    if(f->cursor>=f->offset+40)f->offset=f->cursor-39;
    char shown[41];size_t n=0;
    while(f->offset+n<f->length&&n<40){shown[n]=f->secret?'*':f->text[f->offset+n];n++;}shown[n]=0;
    fb_text(250,y,f->label,active?0xf4f7fa:0x8995a3,2);
    fb_rect(250,y+28,524,42,active?0x4f8fe8:0x252b34);
    fb_rect(253,y+31,518,36,0x161b22);
    fb_text(265,y+41,shown,0xf4f7fa,2);
    if(active&&caret)fb_rect(265+(f->cursor-f->offset)*12,y+38,2,21,0xf4f7fa);
}
static bool inside(int x,int y,int top,int height){return x>=250&&x<774&&y>=top&&y<top+height;}
static void move_pointer(int*x,int*y,const struct mouse_event*event){
    *x+=event->dx;*y+=event->dy;
    if(*x<0)*x=0;
    if(*y<0)*y=0;
    if(*x>=(int)fb_width())*x=(int)fb_width()-1;
    if(*y>=(int)fb_height())*y=(int)fb_height()-1;
}
void account_form(bool setup,char name[32],char password[73],char confirmation[73]){
    struct form_field fields[]={
        {.text=name,.capacity=32,.label="USERNAME"},
        {.text=password,.capacity=73,.label=setup?"PASSWORD (8-72 CHARACTERS)":"PASSWORD",.secret=true},
        {.text=confirmation,.capacity=73,.label="CONFIRM PASSWORD",.secret=true}
    };
    unsigned count=setup?3:2,active=0,first_y=setup?165:210,button_y=setup?475:430;
    for(unsigned i=0;i<count;i++)fields[i].text[0]=0;
    int mouse_x=(int)fb_width()/2,mouse_y=(int)fb_height()/2;
    uint8_t buttons=0;uint64_t blink_start=hardware_uptime_ms();bool dirty=true,last_caret=false;
    fb_pointer_hide();fb_clear(0x101318);
    fb_text(250,setup?70:90,setup?"WELCOME TO AXIOM":"AXIOM LOGIN",0xf4f7fa,setup?4:5);
    fb_text(250,setup?115:145,setup?"Create the first administrator account":"Your system. Your rules.",0x8995a3,2);
    fb_rect(250,button_y,524,42,0x315e97);
    fb_text(275,button_y+14,setup?"CREATE ACCOUNT":"SIGN IN",0xffffff,2);
    fb_text(250,550,"Click a field or use TAB / SHIFT+TAB",0x8995a3,2);
    fb_text(250,575,"ENTER: next field / submit",0x8995a3,2);
    log_write("FORM",setup?"account setup ready":"login ready");
    for(;;){
        bool submit=false,moved=false;
        struct mouse_event mouse;
        while(mouse_poll_event(&mouse)){
            move_pointer(&mouse_x,&mouse_y,&mouse);moved=true;
            if((mouse.buttons&1)&&!(buttons&1)){
                for(unsigned i=0;i<count;i++)if(inside(mouse_x,mouse_y,(int)(first_y+100*i+28),42)){
                    active=i;struct form_field*f=&fields[i];
                    size_t column=mouse_x>265?(size_t)(mouse_x-265)/12:0;
                    f->cursor=f->offset+column;if(f->cursor>f->length)f->cursor=f->length;
                    dirty=true;blink_start=hardware_uptime_ms();log_write("FORM",f->label);
                }
                if(inside(mouse_x,mouse_y,(int)button_y,42))submit=true;
            }
            buttons=mouse.buttons;
        }
        struct key_event key;
        while(keyboard_poll_event(&key)){
            if(!key.pressed)continue;
            if(key.character=='\t'||key.special==KEY_UP||key.special==KEY_DOWN){
                bool back=key.special==KEY_UP||(key.character=='\t'&&key.shift);
                active=back?(active+count-1)%count:(active+1)%count;dirty=true;
            }else if(key.character=='\n'){
                if(active+1<count){active++;dirty=true;}else submit=true;
            }else if(form_edit(&fields[active],&key))dirty=true;
            blink_start=hardware_uptime_ms();
        }
        if(submit){
            bool complete=true;
            for(unsigned i=0;i<count;i++)if(!fields[i].length){active=i;complete=false;dirty=true;break;}
            if(!complete)form_message("Fill in every field.");
            if(complete&&setup){
                enum form_error error=form_validate_setup(name,password,confirmation);
                if(error!=FORM_VALID){
                    static const char*messages[]={"","Name: 2-31 letters, hyphens, then digits.","Password must be 8-72 characters.","Passwords do not match."};
                    form_message(messages[error]);active=(unsigned)error-1;complete=false;dirty=true;
                }
            }
            if(complete){fb_pointer_hide();return;}
        }
        bool caret=((hardware_uptime_ms()-blink_start)/500)%2==0;
        if(dirty||caret!=last_caret){
            fb_pointer_hide();
            for(unsigned i=0;i<count;i++)draw_field(&fields[i],first_y+100*i,i==active,caret);
            last_caret=caret;dirty=false;moved=true;
        }
        if(moved&&mouse_available())fb_pointer_show((uint64_t)mouse_x,(uint64_t)mouse_y);
        __asm__ volatile("hlt");
    }
}
void form_wait_continue(void){
    int x=(int)fb_width()/2,y=(int)fb_height()/2;uint8_t buttons=0;
    fb_rect(250,430,524,42,0x315e97);fb_text(275,444,"I SAVED MY KEY - CONTINUE",0xffffff,2);
    if(mouse_available())fb_pointer_show((uint64_t)x,(uint64_t)y);
    for(;;){
        struct key_event key;
        while(keyboard_poll_event(&key))if(key.pressed&&key.character=='\n'){fb_pointer_hide();return;}
        struct mouse_event mouse;
        while(mouse_poll_event(&mouse)){
            move_pointer(&x,&y,&mouse);
            if((mouse.buttons&1)&&!(buttons&1)&&inside(x,y,430,42)){fb_pointer_hide();return;}
            buttons=mouse.buttons;if(mouse_available())fb_pointer_show((uint64_t)x,(uint64_t)y);
        }
        __asm__ volatile("hlt");
    }
}
