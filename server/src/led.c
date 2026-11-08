#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "led.h"

#define LED_BRIGHT "/sys/class/leds/cpu/brightness"
#define LED_TRIGGER "/sys/class/leds/cpu/trigger"

static int fd1 = -1;
static int fd2 = -1;

int led_init(void)
{
    fd1 = open(LED_BRIGHT, O_WRONLY);
    if (fd1 < 0) 
    { 
        perror("open /sys/class/leds/cpu/brightness error"); 
        return -1; 
    }

    fd2 = open(LED_TRIGGER,O_WRONLY);
    if(fd2 < 0)
    {
        perror("open /sys/class/leds/cpu/trigger error");
        return -1;
    }
    return 0;
}

void led_on(void) 
{ 
    write(fd2,"none",4);
    write(fd1, "1", 1); 
}  
void led_off(void)
{ 
    write(fd2,"none",4);
    write(fd1, "0", 1); 
}
void led_trigger(const char *string)
{
    write(fd2,string,strlen(string));
}
