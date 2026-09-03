#ifndef AXIOM_CONSOLE_H
#define AXIOM_CONSOLE_H

#include <stdint.h>

void console_init(void);
void console_clear(void);
void console_putc(char c);
void console_write(const char *text);
void console_write_u64(uint64_t value);
void console_write_hex(uint64_t value);
void console_backspace(void);

#endif

