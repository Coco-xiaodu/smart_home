#include <stdio.h>
#include "led.h"
#include "server.h"
#include "dht11.h"
#include "pir.h"
#include "mqtt.h"


int main(void)
{

    int temp;
    temp = led_init();               //先初始化硬件层（LED）
    if(temp < 0) 
    {
        return -1;   
    }

    temp = dht11_init(); 
    if(temp < 0)
    {
        printf("初始化DHT11失败\n");
        return -1;
    } 

    temp = pir_init();
    if(temp < 0)
    {
        printf("初始化PIR失败\n");
        return -1;
    }


    //server_start(8888);              //再启动网络服务（阻塞）
    mqtt_start();
    return 0;
}
