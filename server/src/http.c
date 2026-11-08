#include "http.h"
#include "led.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>

void http_handle(int cfd, const char *req)
{
    /* 1. 判断路径：/on 点灯，/off 灭灯 */
    if (strncmp(req, "GET /on", 7) == 0) {
        led_on();
        write(cfd, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK", 42);
    }
    else if (strncmp(req, "GET /off", 8) == 0) {
        led_off();
        write(cfd, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK", 42);
    }
    /* 2. 其它请求：返回控制页面（HTML） */
    else {
        const char *html =
            "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
            "<title>开发板 LED 控制</title></head>"
            "<body style=\"text-align:center;padding:40px;"
            "font-family:sans-serif;\">"
            "<h1>开发板 LED 控制</h1>"
            "<p>浏览器按钮 → HTTP → 开发板 → 点灯</p>"
            "<button onclick=\"fetch('/on')\" style=\"font-size:28px;"
            "padding:24px 48px;margin:20px;border:none;border-radius:12px;"
            "background:#52C41A;color:#fff;\">开灯</button>"
            "<button onclick=\"fetch('/off')\" style=\"font-size:28px;"
            "padding:24px 48px;margin:20px;border:none;border-radius:12px;"
            "background:#EA6668;color:#fff;\">关灯</button>"
            "</body></html>";

        char head[128];
        int len = strlen(html);
        snprintf(head, sizeof(head),
                 "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %d\r\n\r\n",
                 len);
        write(cfd, head, strlen(head));
        write(cfd, html, len);
    }
}
