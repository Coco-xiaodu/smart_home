#include "screen.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#define FB_DEV_NAME "/dev/fb0"

int fb_open(FbDev *dev)
{
    memset(dev, 0, sizeof(FbDev));
    dev->fd = open(FB_DEV_NAME, O_RDWR);
    if (dev->fd < 0) {
        perror("fb open");
        return -1;
    }
    struct fb_var_screeninfo vinfo = {0};
    struct fb_fix_screeninfo finfo = {0};
    if (ioctl(dev->fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        perror("FBIOGET_VSCREENINFO");
        close(dev->fd);
        return -1;
    }
    if (ioctl(dev->fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        perror("FBIOGET_FSCREENINFO");
        close(dev->fd);
        return -1;
    }
    dev->width = vinfo.xres;
    dev->height = vinfo.yres;
    dev->line_len = finfo.line_length;
    uint32_t screen_size = finfo.line_length * vinfo.yres;
    dev->vmem = mmap(NULL, screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, dev->fd, 0);
    if (dev->vmem == MAP_FAILED) {
        perror("mmap fb");
        close(dev->fd);
        return -1;
    }
    printf("fb: %dx%d, bpp:%d\n", dev->width, dev->height, vinfo.bits_per_pixel);
    return 0;
}

void fb_close(FbDev *dev)
{
    if (dev->fd <= 0) return;
    munmap(dev->vmem, dev->line_len * dev->height);
    close(dev->fd);
    memset(dev, 0, sizeof(FbDev));
}

void scale_nearest(uint32_t *src, uint32_t *dst, int src_w, int src_h, int dst_w, int dst_h)
{
    int y, x;
    for (y = 0; y < dst_h; y++) {
        int sy = y * src_h / dst_h;
        if (sy >= src_h) sy = src_h - 1;
        for (x = 0; x < dst_w; x++) {
            int sx = x * src_w / dst_w;
            if (sx >= src_w) sx = src_w - 1;
            dst[y * dst_w + x] = src[sy * src_w + sx];
        }
    }
}
