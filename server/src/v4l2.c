#include "v4l2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

static int enum_formats(int fd)
{
    struct v4l2_fmtdesc fmtdesc = {0};
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmtdesc.index = 0;
    printf("==== Camera supported formats ====\n");
    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        printf("fmt:0x%08X %s\n", fmtdesc.pixelformat, fmtdesc.description);
        fmtdesc.index++;
    }
    return 0;
}

int cam_open(CamDev *dev, const char *dev_name, int req_w, int req_h)
{
    memset(dev, 0, sizeof(CamDev));
    dev->fd = open(dev_name, O_RDWR);
    if (dev->fd < 0) {
        perror("cam open");
        return -1;
    }
    enum_formats(dev->fd);

    struct v4l2_capability cap = {0};
    if (ioctl(dev->fd, VIDIOC_QUERYCAP, &cap) < 0) {
        perror("QUERYCAP");
        close(dev->fd);
        return -1;
    }
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        printf("not capture dev\n");
        close(dev->fd);
        return -1;
    }

    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = req_w;
    fmt.fmt.pix.height = req_h;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    if (ioctl(dev->fd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("S_FMT");
        close(dev->fd);
        return -1;
    }
    printf("cam actual: %dx%d, bpl:%u, sizeimg:%u\n",
           fmt.fmt.pix.width, fmt.fmt.pix.height,
           fmt.fmt.pix.bytesperline, fmt.fmt.pix.sizeimage);
    dev->width = fmt.fmt.pix.width;
    dev->height = fmt.fmt.pix.height;
    dev->bytesperline = fmt.fmt.pix.bytesperline;

    struct v4l2_requestbuffers req = {0};
    req.count = CAM_BUF_CNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(dev->fd, VIDIOC_REQBUFS, &req) < 0) {
        perror("REQBUFS");
        close(dev->fd);
        return -1;
    }

    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    for (int i = 0; i < CAM_BUF_CNT; i++) {
        buf.index = i;
        if (ioctl(dev->fd, VIDIOC_QUERYBUF, &buf) < 0) {
            perror("QUERYBUF");
            close(dev->fd);
            return -1;
        }
        dev->bufs[i].len = buf.length;
        dev->bufs[i].start = mmap(NULL, buf.length, PROT_READ, MAP_SHARED, dev->fd, buf.m.offset);
        if (dev->bufs[i].start == MAP_FAILED) {
            perror("mmap cam");
            close(dev->fd);
            return -1;
        }
        if (ioctl(dev->fd, VIDIOC_QBUF, &buf) < 0) {
            perror("QBUF");
            close(dev->fd);
            return -1;
        }
    }
    return 0;
}

int cam_start_stream(CamDev *dev)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(dev->fd, VIDIOC_STREAMON, &type) < 0) {
        perror("STREAMON");
        return -1;
    }
    return 0;
}

int cam_get_frame(CamDev *dev, uint8_t **data, uint32_t *datalen)
{
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(dev->fd, VIDIOC_DQBUF, &buf) < 0) {
        perror("DQBUF");
        return -1;
    }
    *data = dev->bufs[buf.index].start;
    *datalen = buf.bytesused;
    return buf.index;
}

int cam_put_frame(CamDev *dev)
{
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(dev->fd, VIDIOC_QBUF, &buf) < 0) {
        perror("QBUF");
        return -1;
    }
    return 0;
}

void cam_close(CamDev *dev)
{
    if (dev->fd <= 0) return;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(dev->fd, VIDIOC_STREAMOFF, &type);
    for (int i = 0; i < CAM_BUF_CNT; i++) {
        if (dev->bufs[i].start) munmap(dev->bufs[i].start, dev->bufs[i].len);
    }
    close(dev->fd);
    memset(dev, 0, sizeof(CamDev));
}

void yuyv_to_xrgb8888(uint8_t *yuyv_src, uint32_t *dst, int w, int h, uint32_t bpl)
{
    int y, x;
    for (y = 0; y < h; y++) {
        uint8_t *line = yuyv_src + y * bpl;
        for (x = 0; x < w; x += 2) {
            int y0 = line[x*2 + 0];
            int u  = line[x*2 + 1] - 128;
            int y1 = line[x*2 + 2];
            int v  = line[x*2 + 3] - 128;

            int r = y0 + 1402 * v / 1000;
            int g = y0 - 344 * u / 1000 - 714 * v / 1000;
            int b = y0 + 1772 * u / 1000;
            r = r < 0 ? 0 : (r > 255 ? 255 : r);
            g = g < 0 ? 0 : (g > 255 ? 255 : g);
            b = b < 0 ? 0 : (b > 255 ? 255 : b);
            dst[y * w + x] = (r << 16) | (g << 8) | b;

            r = y1 + 1402 * v / 1000;
            g = y1 - 344 * u / 1000 - 714 * v / 1000;
            b = y1 + 1772 * u / 1000;
            r = r < 0 ? 0 : (r > 255 ? 255 : r);
            g = g < 0 ? 0 : (g > 255 ? 255 : g);
            b = b < 0 ? 0 : (b > 255 ? 255 : b);
            dst[y * w + x + 1] = (r << 16) | (g << 8) | b;
        }
    }
}
