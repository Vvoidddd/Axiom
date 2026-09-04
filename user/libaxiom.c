#include "libaxiom.h"
#include <axiom/syscall.h>
long ax_syscall4(long number,long a,long b,long c,long d){register long r10 __asm__("r10")=d;register long rax __asm__("rax")=number;register long rdi __asm__("rdi")=a;register long rsi __asm__("rsi")=b;register long rdx __asm__("rdx")=c;__asm__ volatile("int $0x80":"+a"(rax):"D"(rdi),"S"(rsi),"d"(rdx),"r"(r10):"memory","cc");return rax;}
size_t ax_strlen(const char*s){size_t n=0;while(s[n])n++;return n;}
int ax_streq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return!*a&&!*b;}
long ax_write(const char*t,size_t n){return ax_syscall4(SYS_WRITE,(long)t,(long)n,0,0);}
void ax_puts(const char*t){ax_write(t,ax_strlen(t));}
long ax_key_poll(void){return ax_syscall4(SYS_KEY_POLL,0,0,0,0);}
long ax_command(const char*t){return ax_syscall4(SYS_COMMAND,(long)t,0,0,0);}
long ax_spawn(const char*p){return ax_syscall4(SYS_SPAWN,(long)p,0,0,0);}
long ax_service_heartbeat(void){return ax_syscall4(SYS_SERVICE_HEARTBEAT,0,0,0,0);}
void ax_sleep(uint64_t ms){ax_syscall4(SYS_SLEEP,(long)ms,0,0,0);}
