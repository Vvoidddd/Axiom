#ifndef AXIOM_CONSOLE_H
#define AXIOM_CONSOLE_H

#include <stdint.h>
#include <stdbool.h>

void console_init(void);
void console_prepare(void);
void console_clear(void);
void console_putc(char c);
void console_write(const char *text);
void console_write_u64(uint64_t value);
void console_write_hex(uint64_t value);
void console_backspace(void);
void console_viewport(uint64_t x,uint64_t y,uint64_t width,uint64_t height);
void console_visible(bool visible);
void console_redraw(void);
uint64_t console_revision(void);

#endif
