#include "libaxiom.h"
#include <axiom/syscall.h>
/* Deterministic ring-3 fuzz corpus. Never submit random text to SYS_COMMAND. */
int main(void){
    unsigned long random=0x4158494f4d;
    long uid=ax_syscall4(SYS_GETUID,0,0,0,0);
    char unterminated[128];for(unsigned i=0;i<sizeof(unterminated);i++)unterminated[i]='x';
    if(ax_command(unterminated)!=-14||ax_spawn(unterminated)!=-14)return 1;
    if(ax_service_heartbeat()!=-13)return 2;
    const unsigned long invalid[]={0,1,0xfffffffffffffffful,0xffff800000000000ul,0x800000000000ul};
    for(unsigned i=0;i<4096;i++){
        random=random*6364136223846793005ul+1;
        long pointer=(long)invalid[i%5];
        if(ax_syscall4((long)(random|0x8000000000000000ul),random,random>>1,random>>2,random>>3)!=-38)return 3;
        if(ax_syscall4(SYS_WRITE,pointer,1+(random&4095),0,0)!=-14)return 4;
        if(ax_syscall4(SYS_COMMAND,pointer,0,0,0)!=-14)return 5;
        if(ax_syscall4(SYS_SPAWN,pointer,0,0,0)!=-14)return 6;
        if(ax_syscall4(SYS_SLEEP,10001+(random&65535),0,0,0)!=-22)return 7;
    }
    if(ax_syscall4(SYS_WRITE,(long)unterminated,4097,0,0)!=-14)return 8;
    return ax_syscall4(SYS_GETUID,0,0,0,0)==uid?0:9;
}
