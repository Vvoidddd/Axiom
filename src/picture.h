#ifndef AXIOM_PICTURE_H
#define AXIOM_PICTURE_H
#include <stdint.h>
struct picture {uint32_t width,height,*pixels;};
/* Zero-initialize p before first use. Classic 24-bit BMP; errors preserve p. */
int picture_load(const char *path,struct picture *picture);
void picture_close(struct picture *picture);
#endif
