#ifndef AXIOM_USER_LIB_H
#define AXIOM_USER_LIB_H
#include <stddef.h>
#include <stdint.h>
long ax_syscall4(long number,long a,long b,long c,long d);
long ax_write(const char*text,size_t size);
void ax_puts(const char*text);
long ax_key_poll(void);
long ax_command(const char*line);
long ax_spawn(const char*path);
void ax_sleep(uint64_t milliseconds);
size_t ax_strlen(const char*text);
int ax_streq(const char*a,const char*b);
#endif
