#include "console.h"
#include "framebuffer.h"

#define BG 0x101318u
#define FG 0xe8edf2u
#define ACCENT 0x66aaffu
#define CELL_W 12u
#define CELL_H 18u

static uint64_t col, row, cols, rows;
static uint64_t origin_x,origin_y;
static bool visible=true;
static uint64_t revision;
uint64_t console_revision(void){return revision;}
/* ponytail: bounded character backing store; replace with surfaces for a compositor. */
static char cells[80][160];

static void redraw_cell(uint64_t x, uint64_t y, char c, uint32_t colour) {
    if(!visible)return;
    fb_pointer_hide();
    fb_rect(origin_x+x * CELL_W, origin_y+y * CELL_H, CELL_W, CELL_H, BG);
    char s[2] = {c, 0};
    fb_text(origin_x+x * CELL_W, origin_y+y * CELL_H + 2, s, colour, 2);
}

static void scroll(void) {
    for(uint64_t y=1;y<rows;y++)for(uint64_t x=0;x<cols;x++)cells[y-1][x]=cells[y][x];
    for(uint64_t x=0;x<cols;x++)cells[rows-1][x]=0;
    console_redraw();
    row = rows - 1;
}

void console_init(void) {
    console_prepare();
    console_clear();
}

void console_prepare(void) {
    visible=true;
    console_viewport(0,0,fb_width(),fb_height());
}
void console_viewport(uint64_t x,uint64_t y,uint64_t width,uint64_t height){
    origin_x=x;origin_y=y;
    cols=width/CELL_W;rows=height/CELL_H;
    if(cols>160)cols=160;
    if(rows>80)rows=80;
    if(!cols)cols=1;
    if(!rows)rows=1;
    if(col>=cols)col=0;
    if(row>=rows)row=rows-1;
}

void console_visible(bool value){visible=value;}
void console_redraw(void){
    if(!visible)return;
    fb_pointer_hide();fb_rect(origin_x,origin_y,cols*CELL_W,rows*CELL_H,BG);
    for(uint64_t y=0;y<rows;y++)for(uint64_t x=0;x<cols;x++)if(cells[y][x])redraw_cell(x,y,cells[y][x],cells[y][x]=='>'?ACCENT:FG);
}
void console_clear(void) {
    revision++;
    for(unsigned y=0;y<80;y++)for(unsigned x=0;x<160;x++)cells[y][x]=0;
    col=0;row=0;console_redraw();
}

void console_putc(char c) {
    revision++;
    if(c=='\b'){console_backspace();return;}
    if(c=='\r'){col=0;return;}
    if (c == '\n') { col = 0; row++; }
    else {
        if((unsigned char)c<32)return;
        cells[row][col]=c;
        redraw_cell(col, row, c, c == '>' ? ACCENT : FG);
        if (++col >= cols) { col = 0; row++; }
    }
    if (row >= rows) scroll();
}

void console_write(const char *text) { while (*text) console_putc(*text++); }

void console_backspace(void) {
    revision++;
    if (col == 0) { if(row==0)return;row--;col=cols-1; }
    else col--;
    cells[row][col]=0;
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
