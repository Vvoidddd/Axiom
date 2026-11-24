#include "recovery.h"
#include "account.h"
#include "console.h"
#include "hardware.h"
#include "log.h"
#include "io.h"
#include "vfs.h"
#include "security.h"
#include <stddef.h>
extern void kernel_execute_user_command(char *line);
static bool equal(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static void erase(void*p,size_t n){volatile unsigned char*b=p;while(n--)*b++=0;}
static void line(const char*prompt,char*out,size_t capacity,bool secret){
    static bool previous_cr;
    console_write(prompt);size_t used=0;bool overflow=false;
    for(;;){
        char c=0;struct key_event event;
        if(inb(0x3fd)&1)c=(char)inb(0x3f8);
        else if(keyboard_poll_event(&event)&&event.pressed)c=event.character;
        if(c=='\n'&&previous_cr){previous_cr=false;continue;}
        if(c){previous_cr=c=='\r';if(previous_cr)c='\n';}
        if(c==127)c='\b';
        if(c=='\n'){out[overflow?0:used]=0;console_putc('\n');if(overflow)console_write("Input too long; discarded.\n");return;}
        if(c=='\b'){if(used){used--;if(!secret)console_backspace();}continue;}
        if(c>=32&&c<127){if(used+1<capacity){out[used++]=c;if(!secret)console_putc(c);}else overflow=true;}
        if(!c)__asm__ volatile("hlt");
    }
}
static void start(void){console_set_output(log_console_char);console_init();}
void recovery_setup(void){
    start();
    if(!account_has_admin()){
        console_write("AXIOM TEXT SETUP\nCreate the first administrator. Password input is hidden.\n");
        for(;;){
            char name[32],password[73],confirmation[73];uint32_t uid;
            line("Username: ",name,sizeof(name),false);
            line("Password: ",password,sizeof(password),true);
            line("Confirm password: ",confirmation,sizeof(confirmation),true);
            bool ok=equal(password,confirmation)&&!account_create(name,password,ACCOUNT_ADMIN,&uid);
            erase(password,sizeof(password));erase(confirmation,sizeof(confirmation));
            if(ok){if(!account_save()){console_write("Account save failed. Recovery stopped to preserve data.\n");machine_halt();}break;}
            console_write("Setup failed: check name, password (8-72 characters), confirmation and storage.\n");
        }
    }
    char key[33];
    if(account_take_recovery_key(key,sizeof(key))){
        console_write("SAVE YOUR RECOVERY KEY OFFLINE: ");console_write(key);console_putc('\n');erase(key,sizeof(key));
        char ignored[2];line("Press Enter after saving it: ",ignored,sizeof(ignored),false);
    }
}
void recovery_shell(void){
    start();account_end_session();
    for(;;){
        console_write("AXIOM TEXT RECOVERY\nAdministrator or recovery account required. Password input is hidden.\n");
        char name[32],password[73];
        line("Login: ",name,sizeof(name),false);line("Password: ",password,sizeof(password),true);
        int uid=account_authenticate(name,password,hardware_uptime_ms());erase(password,sizeof(password));
        const struct account_info *user=account_named(name);
        if(uid<0||!user||(!account_is_admin((uint32_t)uid)&&user->type!=ACCOUNT_RECOVERY)||!account_begin_session((uint32_t)uid)){
            account_audit("recovery-denied",name);
            console_write("Recovery login rejected.\n");pit_wait_ms(1000);continue;
        }
        /* Authenticated maintenance mode deliberately grants root file access. */
        vfs_set_security_context(0,0,0,0,0077,CAP_ALL);vfs_chdir("/");
        account_audit("recovery-console",name);
        console_write("Authenticated maintenance console. HELP lists commands; LOGOUT locks it.\n");
        for(;;){
            char command[128];line("recovery> ",command,sizeof(command),false);
            bool leave=equal(command,"logout")||equal(command,"lock")||equal(command,"exit");
            if(!leave)kernel_execute_user_command(command);
            erase(command,sizeof(command));if(leave)break;
        }
        account_end_session();
    }
}
