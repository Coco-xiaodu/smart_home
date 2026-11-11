// yolo_camera.cpp
// imx6ull 摄像头实时 YOLO 检测
// 用法: ./yolo_camera <model.param> <model.bin> [设备] [输出jpg]
// 默认: /dev/video1, result_camera.jpg

#include <ncnn/net.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

static const char* class_names[] = { "no", "yes", "error" };
const int num_classes = 3;

// 5x7 位图字体
static const unsigned char font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x05},
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E},
    {0x00,0x78,0x54,0x7C,0x00}, {0x00,0x7C,0x54,0x54,0x40},
    {0x00,0x7C,0x0C,0x04,0x00}, {0x00,0x7C,0x44,0x7C,0x00},
    {0x00,0x4C,0x74,0x54,0x00}, {0x00,0x1C,0x7C,0x1C,0x00},
    {0x00,0x7C,0x08,0x04,0x08},
};
static int font_index(char c) {
    switch(c) {
        case ' ': return 0; case '.': return 1;
        case '0': return 2; case '1': return 3; case '2': return 4;
        case '3': return 5; case '4': return 6; case '5': return 7;
        case '6': return 8; case '7': return 9; case '8': return 10;
        case '9': return 11; case 'a': return 12; case 'e': return 13;
        case 'n': return 14; case 'o': return 15; case 's': return 16;
        case 'y': return 17; case 'r': return 18;
        default: return 0;
    }
}
static void put_pixel(unsigned char* rgb, int w, int h, int x, int y, int r, int g, int b) {
    if (x>=0&&x<w&&y>=0&&y<h) { int i=(y*w+x)*3; rgb[i]=r; rgb[i+1]=g; rgb[i+2]=b; }
}
static void draw_text(unsigned char* rgb, int w, int h, int x, int y, const char* t, int s, int r, int g, int b) {
    int cx=x;
    for (const char* p=t; *p; p++) {
        int idx=font_index(*p);
        for (int col=0; col<5; col++) {
            unsigned char bits=font5x7[idx][col];
            for (int row=0; row<7; row++) {
                if (bits&(1<<row)) {
                    for (int dy=0; dy<s; dy++)
                        for (int dx=0; dx<s; dx++)
                            put_pixel(rgb,w,h,cx+col*s+dx,y+row*s+dy,r,g,b);
                }
            }
        }
        cx+=(5+1)*s;
    }
}

struct Box { float x1,y1,x2,y2,score; int cls; };
static float iou(const Box&a,const Box&b) {
    float x1=std::max(a.x1,b.x1),y1=std::max(a.y1,b.y1);
    float x2=std::min(a.x2,b.x2),y2=std::min(a.y2,b.y2);
    float w=std::max(0.f,x2-x1),h=std::max(0.f,y2-y1),i=w*h;
    float u=(a.x2-a.x1)*(a.y2-a.y1)+(b.x2-b.x1)*(b.y2-b.y1)-i;
    return u>0?i/u:0.f;
}

// ---------- V4L2 摄像头 ----------
struct CamBuffer { void* start; size_t length; };

int cam_open(const char* dev, int w, int h, CamBuffer** bufs_out, int* nbufs_out) {
    int fd = open(dev, O_RDWR);
    if (fd < 0) { perror("open cam"); return -1; }

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = w;
    fmt.fmt.pix.height = h;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) { perror("S_FMT"); return -1; }
    printf("cam: %dx%d\n", fmt.fmt.pix.width, fmt.fmt.pix.height);

    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = 4; req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) { perror("REQBUFS"); return -1; }

    CamBuffer* bufs = (CamBuffer*)calloc(req.count, sizeof(CamBuffer));
    for (int i = 0; i < req.count; i++) {
        struct v4l2_buffer b;
        memset(&b, 0, sizeof(b));
        b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; b.memory = V4L2_MEMORY_MMAP; b.index = i;
        ioctl(fd, VIDIOC_QUERYBUF, &b);
        bufs[i].length = b.length;
        bufs[i].start = mmap(NULL, b.length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, b.m.offset);
        ioctl(fd, VIDIOC_QBUF, &b);
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd, VIDIOC_STREAMON, &type);

    *bufs_out = bufs;
    *nbufs_out = req.count;
    return fd;
}

unsigned char* cam_read_frame(int fd, CamBuffer* bufs, int nbufs, int* out_buf_idx) {
    struct v4l2_buffer b;
    memset(&b, 0, sizeof(b));
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; b.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_DQBUF, &b) < 0) { perror("DQBUF"); return NULL; }
    unsigned char* frame = (unsigned char*)bufs[b.index].start;
    *out_buf_idx = b.index;
    return frame;
}

void cam_release_frame(int fd, int buf_idx) {
    struct v4l2_buffer b;
    memset(&b, 0, sizeof(b));
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; b.memory = V4L2_MEMORY_MMAP; b.index = buf_idx;
    ioctl(fd, VIDIOC_QBUF, &b);
}

void cam_close(int fd, CamBuffer* bufs, int nbufs) {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd, VIDIOC_STREAMOFF, &type);
    for (int i = 0; i < nbufs; i++) munmap(bufs[i].start, bufs[i].length);
    free(bufs);
    close(fd);
}

// YUYV 转 RGB
void yuyv_to_rgb(const unsigned char* yuyv, unsigned char* rgb, int w, int h) {
    for (int i = 0; i < w * h / 2; i++) {
        int y0 = yuyv[i*4+0], u = yuyv[i*4+1]-128, y1 = yuyv[i*4+2], v = yuyv[i*4+3]-128;
        int r, g, b;
        r = y0 + 1.402*v; g = y0 - 0.344*u - 0.714*v; b = y0 + 1.772*u;
        rgb[i*6+0]=std::max(0,std::min(255,r)); rgb[i*6+1]=std::max(0,std::min(255,g)); rgb[i*6+2]=std::max(0,std::min(255,b));
        r = y1 + 1.402*v; g = y1 - 0.344*u - 0.714*v; b = y1 + 1.772*u;
        rgb[i*6+3]=std::max(0,std::min(255,r)); rgb[i*6+4]=std::max(0,std::min(255,g)); rgb[i*6+5]=std::max(0,std::min(255,b));
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("usage: %s <model.param> <model.bin> [/dev/video1] [帧数]\n", argv[0]);
        printf("  默认: /dev/video1, 拍 10 帧\n");
        return 1;
    }
    const char* dev = argc > 3 ? argv[3] : "/dev/video1";
    int nframes = argc > 4 ? atoi(argv[4]) : 10;

    const int CAM_W = 640, CAM_H = 480;
    const int TARGET = 320;

    // 打开摄像头
    CamBuffer* bufs; int nbufs;
    int camfd = cam_open(dev, CAM_W, CAM_H, &bufs, &nbufs);
    if (camfd < 0) return 1;

    // 加载模型
    ncnn::Net net;
    net.opt.use_vulkan_compute = false;
    net.opt.num_threads = 1;  // 单核
    net.opt.use_fp16_packed = false;
    net.opt.use_fp16_storage = false;
    if (net.load_param(argv[1]) != 0) { printf("load_param failed\n"); return 1; }
    if (net.load_model(argv[2]) != 0)   { printf("load_model failed\n"); return 1; }

    unsigned char* rgb = (unsigned char*)malloc(CAM_W * CAM_H * 3);

    // letterbox 参数（固定）
    float scale = std::min((float)TARGET/CAM_W, (float)TARGET/CAM_H);
    int new_w = (int)roundf(CAM_W * scale);
    int new_h = (int)roundf(CAM_H * scale);
    int pad_x = (TARGET - new_w) / 2;
    int pad_y = (TARGET - new_h) / 2;
    const float norm[3] = {1/255.f, 1/255.f, 1/255.f};

    for (int frame = 0; frame < nframes; frame++) {
        printf("--- frame %d/%d ---\n", frame+1, nframes);

        // 读一帧
        int buf_idx;
        unsigned char* yuyv = cam_read_frame(camfd, bufs, nbufs, &buf_idx);
        if (!yuyv) break;

        // YUYV -> RGB
        yuyv_to_rgb(yuyv, rgb, CAM_W, CAM_H);
        cam_release_frame(camfd, buf_idx);

        // letterbox 到 320x320
        ncnn::Mat in_resized = ncnn::Mat::from_pixels_resize(
            rgb, ncnn::Mat::PIXEL_RGB, CAM_W, CAM_H, new_w, new_h);

        ncnn::Mat canvas(TARGET, TARGET, 3);
        canvas.fill(114.f);
        for (int ch = 0; ch < 3; ch++) {
            const float* src = in_resized.channel(ch);
            float* dst = canvas.channel(ch);
            for (int y = 0; y < new_h; y++)
                memcpy(dst+(y+pad_y)*TARGET+pad_x, src+y*new_w, new_w*sizeof(float));
        }
        canvas.substract_mean_normalize(0, norm);

        // 推理
        ncnn::Extractor ex = net.create_extractor();
        ex.input("in0", canvas);
        ncnn::Mat out;
        ex.extract("out0", out);

        // 后处理
        std::vector<Box> dets;
        for (int i = 0; i < out.w; i++) {
            float cx=out.row(0)[i], cy=out.row(1)[i], bw=out.row(2)[i], bh=out.row(3)[i];
            float ms=0; int bc=-1;
            for (int k=0; k<num_classes; k++) {
                float s = out.row(4+k)[i];
                if (s>ms) { ms=s; bc=k; }
            }
            if (ms < 0.5) continue;
            cx=(cx-pad_x)/scale; cy=(cy-pad_y)/scale; bw/=scale; bh/=scale;
            Box b; b.x1=cx-bw/2; b.y1=cy-bh/2; b.x2=cx+bw/2; b.y2=cy+bh/2; b.score=ms; b.cls=bc;
            dets.push_back(b);
        }
        std::sort(dets.begin(), dets.end(), [](const Box&a,const Box&b){return a.score>b.score;});
        std::vector<Box> keep;
        for (auto& b : dets) {
            bool drop=false;
            for (auto& k : keep) if (k.cls==b.cls && iou(k,b)>0.45) { drop=true; break; }
            if (!drop) keep.push_back(b);
        }

        printf("  detections: %d\n", (int)keep.size());
        int fs = 2;
        for (auto& b : keep) {
            char label[64];
            snprintf(label, sizeof(label), "%s %.2f", class_names[b.cls], b.score);
            int x1=std::max(0,(int)b.x1), y1=std::max(0,(int)b.y1);
            int x2=std::min(CAM_W-1,(int)b.x2), y2=std::min(CAM_H-1,(int)b.y2);
            int r_box, g_box, b_box;
            if (b.cls==0)      { r_box=0;   g_box=128; b_box=255; }
            else if (b.cls==1) { r_box=0;   g_box=200; b_box=0;   }
            else               { r_box=255; g_box=0;   b_box=0;   }
            for (int t=0; t<4; t++) {
                for (int x=x1-t; x<=x2+t; x++) {
                    put_pixel(rgb,CAM_W,CAM_H,x,y1-t,r_box,g_box,b_box);
                    put_pixel(rgb,CAM_W,CAM_H,x,y2+t,r_box,g_box,b_box);
                }
                for (int y=y1-t; y<=y2+t; y++) {
                    put_pixel(rgb,CAM_W,CAM_H,x1-t,y,r_box,g_box,b_box);
                    put_pixel(rgb,CAM_W,CAM_H,x2+t,y,r_box,g_box,b_box);
                }
            }
            int tx=x1;
            int tw=(int)strlen(label)*6*fs;
            int ty;
            if (y1 - 8*fs - 4 > 0) ty = y1 - 8*fs - 4;
            else ty = y2 + 2;
            for (int by=ty-1; by<ty+8*fs+1; by++)
                for (int bx=tx-1; bx<tx+tw+1; bx++)
                    put_pixel(rgb,CAM_W,CAM_H,bx,by,0,0,0);
            draw_text(rgb,CAM_W,CAM_H,tx,ty,label,fs,255,255,255);
        }

        // 保存帧
        char fname[64];
        snprintf(fname, sizeof(fname), "frame_%02d.jpg", frame);
        stbi_write_jpg(fname, CAM_W, CAM_H, 3, rgb, 80);
        printf("  saved %s\n", fname);

        sleep(1); // 间隔 1 秒
    }

    free(rgb);
    cam_close(camfd, bufs, nbufs);
    printf("done, %d frames saved\n", nframes);
    return 0;
}
