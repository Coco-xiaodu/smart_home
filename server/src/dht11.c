#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "dht11.h"

#define DHT11_DEV     "/dev/dht11"    /* 板子设备节点 */
#define DHT11_IOCINIT 0x40046400      /* 初始化命令（arg = &gpio 指针！） */
#define DHT11_IOCREAD 0xc0046402      /* 读命令（arg = buf 指针） */

#define DHT11_GPIO    115             /* GPIO4_IO19 */

static int fd = -1;

int dht11_init(void)
{
    int gpio = DHT11_GPIO;            /* 先放变量里 */

    if (fd >= 0)
        close(fd);

    fd = open(DHT11_DEV, O_RDWR);
    if (fd < 0)
    {
        perror("open /dev/dht11");
        return -1;
    }

    /* ★ 关键：传 &gpio（指针），不是 115（数值）！
     * 驱动用 copy_from_user 从该地址读 GPIO 号 */
    if (ioctl(fd, DHT11_IOCINIT, &gpio) < 0)
    {
        perror("ioctl DHT11_IOCINIT");
        return -1;                    /* 驱动 bug：此时退出必崩内核，需重启板子 */
    }

    printf("DHT11 初始化成功 (GPIO%d)\n", gpio);
    return 0;
}

int dht11_read(void)
{
    char buf[2] = {0};
    int ret;

    if (fd < 0)
    {
        printf("DHT11 未初始化\n");
        return -1;
    }

    ret = ioctl(fd, DHT11_IOCREAD, buf);   /* buf 数组名即指针，正确 */
    if (ret < 0)
    {
        perror("ioctl DHT11_IOCREAD");
        return -1;
    }

    printf("温度: %d°C  湿度: %d%%\n",
           (unsigned char)buf[1], (unsigned char)buf[0]);   /* [湿度, 温度] */
    return 0;
}
