#include "console.h"
#include "framebuffer.h"

#define BG 0x101318u
#define FG 0xe8edf2u
#define ACCENT 0x66aaffu
#define CELL_W 12u
#define CELL_H 18u

static uint64_t col, row, cols, rows;

static void redraw_cell(uint64_t x, uint64_t y, char c, uint32_t colour) {
    fb_rect(x * CELL_W, y * CELL_H, CELL_W, CELL_H, BG);
    char s[2] = {c, 0};
    fb_text(x * CELL_W, y * CELL_H + 2, s, colour, 2);
}

static void scroll(void) {
    struct limine_framebuffer *info = fb_info();
    uint64_t bytes = (info->height - CELL_H) * info->pitch;
    unsigned char *base = info->address;
    for (uint64_t i = 0; i < bytes; i++) base[i] = base[i + CELL_H * info->pitch];
    fb_rect(0, info->height - CELL_H, info->width, CELL_H, BG);
    row = rows - 1;
}

void console_init(void) {
    cols = fb_width() / CELL_W;
    rows = fb_height() / CELL_H;
    console_clear();
}

void console_clear(void) { fb_clear(BG); col = 0; row = 0; }

void console_putc(char c) {
    if (c == '\n') { col = 0; row++; }
    else {
        redraw_cell(col, row, c, c == '>' ? ACCENT : FG);
        if (++col >= cols) { col = 0; row++; }
    }
    if (row >= rows) scroll();
}

void console_write(const char *text) { while (*text) console_putc(*text++); }

void console_backspace(void) {
    if (col == 0) { if(row==0)return;row--;col=cols-1; }
    else col--;
    redraw_cell(col, row, ' ', FG);
}

void console_write_u64(uint64_t value) {
    char buf[21]; unsigned i = sizeof(buf); buf[--i] = 0;
    do { buf[--i] = (char)('0' + value % 10); value /= 10; } while (value);
    console_write(&buf[i]);
}

void console_write_hex(uint64_t value) {
    static const char hex[] = "0123456789ABCDEF";
    char buf[19]; buf[0]='0'; buf[1]='x'; buf[18]=0;
    for (int i=17; i>=2; i--) { buf[i]=hex[value & 15]; value >>= 4; }
    console_write(buf);
}
