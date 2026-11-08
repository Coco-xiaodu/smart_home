#include "pir.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

static int fd = -1;
static char gpio_path[64] = "/sys/class/gpio/gpio116";
static char path[64] = "/sys/class/gpio/gpio116/value";

int pir_init(void)
{
    if(access(gpio_path, F_OK) == -1)  //判断gpio116是否已经导出
    {
        fd = open("/sys/class/gpio/export", O_WRONLY);
        if(fd < 0)
        {
            perror("open /sys/class/gpio/export error");
            return -1;
        }
        write(fd, "116", 3);
        close(fd);
    }

    fd = open(path, O_RDONLY);
    if(fd < 0)
    {
        perror("open /sys/class/gpio/gpio116/value error");
        return -1;
    }
    return 0;
}

int pir_read(void)
{
    char buf[2] = {0};
    int pir_status = 0;
    lseek(fd, 0, SEEK_SET);
    read(fd, buf, 1);
    pir_status = atoi(buf);
    if(pir_status == 1)
    {
        printf("有人经过\n");
    }
    else
    {
        printf("无人经过\n");
    }
    return pir_status;
}

void pir_exit(void)
{
    close(fd);
    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if(fd < 0)
    {
        perror("open /sys/class/gpio/unexport error");
        return;
    }
    write(fd, "116", 3);
    close(fd);
}