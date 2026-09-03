#include "framebuffer.h"
#include <stddef.h>

static struct limine_framebuffer *fb;

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
        case '/': return (uint8_t)(1u << (4u - (row * 5u / 7u)));
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
    fb = info;
    return true;
}

void fb_rect(uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint32_t rgb) {
    if (!fb || x >= fb->width || y >= fb->height) return;
    if (x + w > fb->width) w = fb->width - x;
    if (y + h > fb->height) h = fb->height - y;
    uint32_t colour = native_colour(rgb);
    for (uint64_t py = y; py < y + h; py++) {
        volatile uint32_t *line = (volatile uint32_t *)((uintptr_t)fb->address + py * fb->pitch);
        for (uint64_t px = x; px < x + w; px++) line[px] = colour;
    }
}

void fb_clear(uint32_t rgb) { fb_rect(0, 0, fb->width, fb->height, rgb); }

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
