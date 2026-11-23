#ifndef AXIOM_FRAMEBUFFER_H
#define AXIOM_FRAMEBUFFER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <limine.h>

bool fb_init(struct limine_framebuffer *info);
void fb_clear(uint32_t rgb);
void fb_rect(uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint32_t rgb);
void fb_text(uint64_t x, uint64_t y, const char *text, uint32_t rgb, unsigned scale);
uint64_t fb_width(void);
uint64_t fb_height(void);
struct limine_framebuffer *fb_info(void);
void fb_pointer_hide(void);
void fb_pointer_show(uint64_t x,uint64_t y);
/* Native-pixel software surface; the caller owns width*height pixels. */
bool fb_begin_frame(uint32_t *pixels,size_t count);
void fb_present(void);
void fb_clip(uint64_t x,uint64_t y,uint64_t width,uint64_t height);
void fb_clip_reset(void);

#endif

