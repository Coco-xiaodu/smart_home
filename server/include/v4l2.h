#ifndef V4L2_H
#define V4L2_H

#include <stdint.h>
#include <linux/videodev2.h>

#define CAM_BUF_CNT 3

typedef struct {
    uint8_t *start;
    uint32_t len;
} CamBuf;

typedef struct {
    int fd;
    int width;
    int height;
    uint32_t bytesperline;
    CamBuf bufs[CAM_BUF_CNT];
} CamDev;

int cam_open(CamDev *dev, const char *dev_name, int req_w, int req_h);
int cam_start_stream(CamDev *dev);
int cam_get_frame(CamDev *dev, uint8_t **data, uint32_t *datalen);
int cam_put_frame(CamDev *dev);
void cam_close(CamDev *dev);
void yuyv_to_xrgb8888(uint8_t *yuyv_src, uint32_t *dst, int w, int h, uint32_t bpl);

#endif
