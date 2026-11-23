#include "framebuffer.h"
#include <stddef.h>

static struct limine_framebuffer *fb;
static uint32_t *surface;
static uint64_t clip_left,clip_top,clip_right,clip_bottom;
static uint64_t damage_left,damage_top,damage_right,damage_bottom;
static bool damaged;

/* Compact 5x7 glyph rows. Lowercase uses readable small-cap forms. */
static const uint8_t letters[26][7] = {
    {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
    {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
    {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
    {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}
};

static const uint8_t digits[10][7] = {
    {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14}
};

static const uint8_t lowercase[26][7] = {
    {0,0,14,1,15,17,15},{16,16,22,25,17,17,30},{0,0,14,16,16,17,14},
    {1,1,13,19,17,17,15},{0,0,14,17,31,16,14},{6,9,8,28,8,8,8},
    {0,0,15,17,15,1,14},{16,16,22,25,17,17,17},{4,0,12,4,4,4,14},
    {2,0,6,2,2,18,12},{16,16,18,20,24,20,18},{12,4,4,4,4,4,14},
    {0,0,26,21,21,17,17},{0,0,22,25,17,17,17},{0,0,14,17,17,17,14},
    {0,0,30,17,30,16,16},{0,0,15,17,15,1,1},{0,0,22,25,16,16,16},
    {0,0,15,16,14,1,30},{8,8,28,8,8,9,6},{0,0,17,17,17,19,13},
    {0,0,17,17,17,10,4},{0,0,17,17,21,21,10},{0,0,17,10,4,10,17},
    {0,0,17,17,15,1,14},{0,0,31,2,4,8,31}
};

static uint8_t glyph_row(char c, unsigned row) {
    if (c >= 'a' && c <= 'z') return lowercase[(unsigned)(c - 'a')][row];
    if (c >= 'A' && c <= 'Z') return letters[(unsigned)(c - 'A')][row];
    if (c >= '0' && c <= '9') return digits[(unsigned)(c - '0')][row];
    switch (c) {
        case '.': return row == 6 ? 4 : 0;
        case ':': return (row == 2 || row == 5) ? 4 : 0;
        case '>': return row == 2 ? 16 : row == 3 ? 8 : row == 4 ? 4 : 0;
        case '<': return row == 2 ? 1 : row == 3 ? 2 : row == 4 ? 4 : 0;
        case '-': return row == 3 ? 14 : 0;
        case '_': return row == 6 ? 31 : 0;
        case '*': { static const uint8_t star[7]={0,21,14,31,14,21,0}; return star[row]; }
        case '/': return (uint8_t)(1u << (row * 5u / 7u));
        case '\\': return (uint8_t)(1u << (4u - (row * 5u / 7u)));
        case '+': return row==3?31:(row>=1&&row<=5?4:0);
        case '=': return (row == 2 || row == 4) ? 31 : 0;
        case '?': { static const uint8_t q[7]={14,17,1,2,4,0,4}; return q[row]; }
        case '!': return row < 5 || row == 6 ? 4 : 0;
        case ',': return row == 5 ? 4 : row == 6 ? 8 : 0;
        case '[': return (row == 0 || row == 6) ? 14 : 8;
        case ']': return (row == 0 || row == 6) ? 14 : 2;
        case '(': return row == 0 || row == 6 ? 2 : row == 1 || row == 5 ? 4 : 8;
        case ')': return row == 0 || row == 6 ? 8 : row == 1 || row == 5 ? 4 : 2;
        case ' ': default: return 0;
    }
}

static uint32_t native_colour(uint32_t rgb) {
    uint32_t r = (rgb >> 16) & 0xff, g = (rgb >> 8) & 0xff, b = rgb & 0xff;
    return ((r >> (8 - fb->red_mask_size)) << fb->red_mask_shift) |
           ((g >> (8 - fb->green_mask_size)) << fb->green_mask_shift) |
           ((b >> (8 - fb->blue_mask_size)) << fb->blue_mask_shift);
}

bool fb_init(struct limine_framebuffer *info) {
    if (!info || !info->address || info->bpp != 32 || info->memory_model != LIMINE_FRAMEBUFFER_RGB) return false;
    fb_pointer_hide();fb = info;surface=0;fb_clip_reset();
    return true;
}

void fb_clip(uint64_t x,uint64_t y,uint64_t w,uint64_t h){
    clip_left=x<fb_width()?x:fb_width();clip_top=y<fb_height()?y:fb_height();
    clip_right=clip_left+(w<fb_width()-clip_left?w:fb_width()-clip_left);
    clip_bottom=clip_top+(h<fb_height()-clip_top?h:fb_height()-clip_top);
}
void fb_clip_reset(void){fb_clip(0,0,fb_width(),fb_height());}
bool fb_begin_frame(uint32_t*pixels,size_t count){
    if(!fb||surface||!pixels||!fb->height||fb->width>count/fb->height)return false;
    fb_pointer_hide();fb_clip_reset();surface=pixels;damaged=false;
    for(uint64_t y=0;y<fb->height;y++){
        const volatile uint32_t*row=(void*)((uintptr_t)fb->address+y*fb->pitch);
        for(uint64_t x=0;x<fb->width;x++)surface[y*fb->width+x]=row[x];
    }
    return true;
}
void fb_present(void){
    if(surface&&damaged)for(uint64_t y=damage_top;y<damage_bottom;y++){
        volatile uint32_t*row=(void*)((uintptr_t)fb->address+y*fb->pitch);
        for(uint64_t x=damage_left;x<damage_right;x++)row[x]=surface[y*fb->width+x];
    }
    surface=0;damaged=false;fb_clip_reset();
}

void fb_rect(uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint32_t rgb) {
    if (!fb || x >= fb->width || y >= fb->height) return;
    uint64_t right=x+(w<fb->width-x?w:fb->width-x),bottom=y+(h<fb->height-y?h:fb->height-y);
    if(x<clip_left)x=clip_left;
    if(y<clip_top)y=clip_top;
    if(right>clip_right)right=clip_right;
    if(bottom>clip_bottom)bottom=clip_bottom;
    if(x>=right||y>=bottom)return;
    if(surface){
        if(!damaged){damage_left=x;damage_top=y;damage_right=right;damage_bottom=bottom;damaged=true;}
        else{if(x<damage_left)damage_left=x;if(y<damage_top)damage_top=y;if(right>damage_right)damage_right=right;if(bottom>damage_bottom)damage_bottom=bottom;}
    }
    uint32_t colour = native_colour(rgb);
    for (uint64_t py = y; py < bottom; py++) {
        volatile uint32_t *line = surface?surface+py*fb->width:(volatile uint32_t *)((uintptr_t)fb->address + py * fb->pitch);
        for (uint64_t px = x; px < right; px++) line[px] = colour;
    }
}

void fb_clear(uint32_t rgb) { fb_rect(0, 0, fb_width(), fb_height(), rgb); }

static uint32_t pointer_saved[18][12];
static uint64_t pointer_x,pointer_y;
static bool pointer_visible;
void fb_pointer_hide(void){
    if(!pointer_visible||!fb)return;
    for(unsigned y=0;y<18&&pointer_y+y<fb->height;y++){
        volatile uint32_t*row=(void*)((uintptr_t)fb->address+(pointer_y+y)*fb->pitch);
        for(unsigned x=0;x<12&&pointer_x+x<fb->width;x++)row[pointer_x+x]=pointer_saved[y][x];
    }
    pointer_visible=false;
}
void fb_pointer_show(uint64_t x,uint64_t y){
    if(surface)return;
    fb_pointer_hide();if(!fb||x>=fb->width||y>=fb->height)return;
    pointer_x=x;pointer_y=y;
    for(unsigned dy=0;dy<18&&y+dy<fb->height;dy++){
        volatile uint32_t*row=(void*)((uintptr_t)fb->address+(y+dy)*fb->pitch);
        for(unsigned dx=0;dx<12&&x+dx<fb->width;dx++){
            pointer_saved[dy][dx]=row[x+dx];
            bool arrow=dy<13&&dx<=dy/2;
            bool stem=dy>=10&&dy<18&&dx>=3&&dx<=5;
            if(arrow||stem)row[x+dx]=native_colour(dx==0||dx==dy/2||dy==17?0x101318:0xffffff);
        }
    }
    pointer_visible=true;
}

void fb_text(uint64_t x, uint64_t y, const char *text, uint32_t rgb, unsigned scale) {
    uint64_t origin = x;
    for (; *text; text++) {
        if (*text == '\n') { x = origin; y += 9u * scale; continue; }
        for (unsigned row = 0; row < 7; row++) {
            uint8_t bits = glyph_row(*text, row);
            for (unsigned col = 0; col < 5; col++)
                if (bits & (1u << (4u - col))) fb_rect(x + col * scale, y + row * scale, scale, scale, rgb);
        }
        x += 6u * scale;
    }
}

uint64_t fb_width(void) { return fb ? fb->width : 0; }
uint64_t fb_height(void) { return fb ? fb->height : 0; }
struct limine_framebuffer *fb_info(void) { return fb; }
