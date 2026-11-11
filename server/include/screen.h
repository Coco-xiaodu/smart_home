#ifndef SCREEN_H
#define SCREEN_H

#include <stdint.h>

typedef struct {
    int fd;
    int width;
    int height;
    uint32_t line_len;
    uint32_t *vmem;
} FbDev;

int fb_open(FbDev *dev);
void fb_close(FbDev *dev);
void scale_nearest(uint32_t *src, uint32_t *dst, int src_w, int src_h, int dst_w, int dst_h);

#endif
