// yolo_ncnn_test.cpp
// PC 端（x86 Ubuntu）验证 yolov11n ncnn 模型是否正确
// 用法: ./yolo_ncnn_test <model.param> <model.bin> <test.jpg>
// 输出: 终端打印检测框 + 生成 result.png

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

static const char* class_names[] = {
    "no", "yes", "error"
};
const int num_classes = 3;

// ---------- 5x7 位图字体（已验证正确） ----------
// 每个字符 5 列 × 7 行，每列 1 字节，bit0=顶部行
static const unsigned char font5x7[][5] = {
    // 0: ' '
    {0x00, 0x00, 0x00, 0x00, 0x00},
    // 1: '.'
    {0x00, 0x00, 0x00, 0x00, 0x05},
    // 2: '0'
    {0x3E, 0x51, 0x49, 0x45, 0x3E},
    // 3: '1'
    {0x00, 0x42, 0x7F, 0x40, 0x00},
    // 4: '2'
    {0x42, 0x61, 0x51, 0x49, 0x46},
    // 5: '3'
    {0x21, 0x41, 0x45, 0x4B, 0x31},
    // 6: '4'
    {0x18, 0x14, 0x12, 0x7F, 0x10},
    // 7: '5'
    {0x27, 0x45, 0x45, 0x45, 0x39},
    // 8: '6'
    {0x3C, 0x4A, 0x49, 0x49, 0x30},
    // 9: '7'
    {0x01, 0x71, 0x09, 0x05, 0x03},
    // 10: '8'
    {0x36, 0x49, 0x49, 0x49, 0x36},
    // 11: '9'
    {0x06, 0x49, 0x49, 0x29, 0x1E},
    // 12: 'a'
    {0x00, 0x78, 0x54, 0x7C, 0x00},
    // 13: 'e'
    {0x00, 0x7C, 0x54, 0x54, 0x40},
    // 14: 'n'
    {0x00, 0x7C, 0x0C, 0x04, 0x00},
    // 15: 'o'
    {0x00, 0x7C, 0x44, 0x7C, 0x00},
    // 16: 's'
    {0x00, 0x4C, 0x74, 0x54, 0x00},
    // 17: 'y'
    {0x00, 0x1C, 0x7C, 0x1C, 0x00},
    // 18: 'r'
    {0x00, 0x7C, 0x08, 0x04, 0x08},
};

static int font_index(char c) {
    switch(c) {
        case ' ': return 0;
        case '.': return 1;
        case '0': return 2;
        case '1': return 3;
        case '2': return 4;
        case '3': return 5;
        case '4': return 6;
        case '5': return 7;
        case '6': return 8;
        case '7': return 9;
        case '8': return 10;
        case '9': return 11;
        case 'a': return 12;
        case 'e': return 13;
        case 'n': return 14;
        case 'o': return 15;
        case 's': return 16;
        case 'y': return 17;
        case 'r': return 18;
        default:  return 0;
    }
}

static void put_pixel(unsigned char* rgb, int img_w, int img_h, int x, int y,
                      int r, int g, int b) {
    if (x >= 0 && x < img_w && y >= 0 && y < img_h) {
        int idx = (y * img_w + x) * 3;
        rgb[idx]   = r;
        rgb[idx+1] = g;
        rgb[idx+2] = b;
    }
}

// 画文字（5x7 字体，scale=放大倍数），颜色 (r,g,b)
static void draw_text(unsigned char* rgb, int img_w, int img_h,
                      int x, int y, const char* text, int scale,
                      int r, int g, int b) {
    int cx = x;
    for (const char* p = text; *p; p++) {
        int idx = font_index(*p);
        for (int col = 0; col < 5; col++) {
            unsigned char bits = font5x7[idx][col];
            for (int row = 0; row < 7; row++) {
                if (bits & (1 << row)) {
                    for (int dy = 0; dy < scale; dy++) {
                        for (int dx = 0; dx < scale; dx++) {
                            put_pixel(rgb, img_w, img_h,
                                      cx + col*scale + dx, y + row*scale + dy,
                                      r, g, b);
                        }
                    }
                }
            }
        }
        cx += (5 + 1) * scale;
    }
}

struct Box {
    float x1, y1, x2, y2;
    float score;
    int cls;
};

static float iou(const Box& a, const Box& b) {
    float x1 = std::max(a.x1, b.x1);
    float y1 = std::max(a.y1, b.y1);
    float x2 = std::min(a.x2, b.x2);
    float y2 = std::min(a.y2, b.y2);
    float w = std::max(0.f, x2 - x1);
    float h = std::max(0.f, y2 - y1);
    float inter = w * h;
    float uni = (a.x2 - a.x1) * (a.y2 - a.y1) + (b.x2 - b.x1) * (b.y2 - b.y1) - inter;
    return uni > 0 ? inter / uni : 0.f;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        printf("usage: %s <model.param> <model.bin> <test.jpg>\n", argv[0]);
        return 1;
    }
    const char* param_path = argv[1];
    const char* bin_path   = argv[2];
    const char* img_path   = argv[3];

    int img_w = 0, img_h = 0, c = 0;
    unsigned char* rgb = stbi_load(img_path, &img_w, &img_h, &c, 3);
    if (!rgb) { printf("load image failed: %s\n", img_path); return 1; }
    printf("image: %dx%d\n", img_w, img_h);

    // letterbox 预处理到 320x320
    const int target = 320;
    const float scale = std::min((float)target / img_w, (float)target / img_h);
    const int new_w = (int)roundf(img_w * scale);
    const int new_h = (int)roundf(img_h * scale);
    const int pad_x = (target - new_w) / 2;
    const int pad_y = (target - new_h) / 2;

    ncnn::Mat in_resized = ncnn::Mat::from_pixels_resize(
        rgb, ncnn::Mat::PIXEL_RGB, img_w, img_h, new_w, new_h);

    ncnn::Mat canvas(target, target, 3);
    canvas.fill(114.f);

    for (int ch = 0; ch < 3; ch++) {
        const float* src = in_resized.channel(ch);
        float* dst = canvas.channel(ch);
        for (int y = 0; y < new_h; y++) {
            memcpy(dst + (y + pad_y) * target + pad_x,
                   src + y * new_w,
                   new_w * sizeof(float));
        }
    }

    const float norm_vals[3] = { 1.f / 255.f, 1.f / 255.f, 1.f / 255.f };
    canvas.substract_mean_normalize(0, norm_vals);

    // 加载模型
    ncnn::Net net;
    net.opt.use_vulkan_compute = false;
    net.opt.num_threads = 4;
    net.opt.use_fp16_packed = false;
    net.opt.use_fp16_storage = false;

    if (net.load_param(param_path) != 0) { printf("load_param failed\n"); return 1; }
    if (net.load_model(bin_path) != 0)   { printf("load_model failed\n"); return 1; }

    ncnn::Extractor ex = net.create_extractor();
    ex.input("in0", canvas);

    ncnn::Mat out;
    if (ex.extract("out0", out) != 0) { printf("extract out0 failed\n"); return 1; }
    printf("out shape: dims=%d c=%d h=%d w=%d\n", out.dims, out.c, out.h, out.w);

    // 后处理
    std::vector<Box> dets;
    const int num_anchors = out.w;
    for (int i = 0; i < num_anchors; i++) {
        float cx = out.row(0)[i];
        float cy = out.row(1)[i];
        float bw = out.row(2)[i];
        float bh = out.row(3)[i];

        float max_score = 0;
        int best_cls = -1;
        for (int k = 0; k < num_classes; k++) {
            float s = out.row(4 + k)[i];
            if (s > max_score) { max_score = s; best_cls = k; }
        }
        if (max_score < 0.25) continue;

        cx = (cx - pad_x) / scale;
        cy = (cy - pad_y) / scale;
        bw /= scale;
        bh /= scale;

        Box b;
        b.x1 = cx - bw / 2; b.y1 = cy - bh / 2;
        b.x2 = cx + bw / 2; b.y2 = cy + bh / 2;
        b.score = max_score;
        b.cls = best_cls;
        dets.push_back(b);
    }

    // NMS
    std::sort(dets.begin(), dets.end(),
              [](const Box& a, const Box& b) { return a.score > b.score; });
    std::vector<Box> keep;
    for (auto& b : dets) {
        bool drop = false;
        for (auto& k : keep) {
            if (k.cls == b.cls && iou(k, b) > 0.45) { drop = true; break; }
        }
        if (!drop) keep.push_back(b);
    }

    // 画框 + 标签
    printf("detections: %d\n", (int)keep.size());

    int fscale = 4; // 字体放大 4 倍

    for (auto& b : keep) {
        char label[64];
        snprintf(label, sizeof(label), "%s %.2f", class_names[b.cls], b.score);
        printf("  %-6s %.2f  [%.0f,%.0f,%.0f,%.0f]\n",
               class_names[b.cls], b.score, b.x1, b.y1, b.x2, b.y2);

        int x1 = std::max(0, (int)b.x1);
        int y1 = std::max(0, (int)b.y1);
        int x2 = std::min(img_w - 1, (int)b.x2);
        int y2 = std::min(img_h - 1, (int)b.y2);

        // 类别颜色：no=蓝色，yes=绿色，error=红色
        int r_box, g_box, b_box;
        if (b.cls == 0)      { r_box = 0;   g_box = 128; b_box = 255; }  // 蓝
        else if (b.cls == 1) { r_box = 0;   g_box = 200; b_box = 0;   }  // 绿
        else                 { r_box = 255; g_box = 0;   b_box = 0;   }  // 红

        // 画框（粗 4px，明显）
        int bw = 4;
        for (int t = 0; t < bw; t++) {
            for (int x = x1-t; x <= x2+t; x++) {
                put_pixel(rgb, img_w, img_h, x, y1-t, r_box, g_box, b_box);
                put_pixel(rgb, img_w, img_h, x, y2+t, r_box, g_box, b_box);
            }
            for (int y = y1-t; y <= y2+t; y++) {
                put_pixel(rgb, img_w, img_h, x1-t, y, r_box, g_box, b_box);
                put_pixel(rgb, img_w, img_h, x2+t, y, r_box, g_box, b_box);
            }
        }

        // 标签位置
        int tx = x1;
        int ty = y1 - 8 * fscale - 2;
        if (ty < 0) ty = y1 + 3;

        // 标签背景条：纯黑色（对比度最高）
        int tw = (int)strlen(label) * 6 * fscale;
        for (int by = ty - 1; by < ty + 7 * fscale + 1; by++) {
            for (int bx = tx - 2; bx < tx + tw + 2; bx++) {
                put_pixel(rgb, img_w, img_h, bx, by, 0, 0, 0);
            }
        }

        // 白色文字
        draw_text(rgb, img_w, img_h, tx, ty, label, fscale, 255, 255, 255);
    }

    stbi_write_png("result.png", img_w, img_h, 3, rgb, img_w * 3);
    printf("saved result.png\n");

    stbi_image_free(rgb);
    return 0;
}
