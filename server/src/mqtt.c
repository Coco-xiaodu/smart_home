#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/stat.h> 
#include <sys/ioctl.h>
#include <fcntl.h> 
#include "MQTTClient.h"     //包含MQTT客户端库头文件
#include "mqtt.h"
#include "led.h"       /* 网络层调用硬件层，靠 led.h 的声明 */
#include "http.h"
#include "dht11.h"
#include "pir.h"

#define SERVER_ADDRESS  "192.168.68.129"
#define CLIENTID        "智能家居"

static void delivered(void *context, MQTTClient_deliveryToken dt) 
{ 
    printf("Message with token value %d delivery confirmed\n", dt); 
} 

static int msgarrvd(void *context, char *topicName, int topicLen, 
            MQTTClient_message *message) 
{ 
    if(strcmp("灯光控制",topicName) == 0)
    {
        if(strcmp("heartbeat",message->payload) == 0)
        {
            led_trigger("heartbeat");
        }
        else if(strcmp("on",message->payload) == 0)
        {
            led_on();
        }
        else if(strcmp("off",message->payload) == 0)
        {
            led_off();
        }
        else
        {
            printf("i don't no this command\n");
        }
    }
    else if(strcmp("温湿度检测",topicName) == 0)
    {
        dht11_read();
    }
    else if(strcmp("人体感应",topicName) == 0)
    {
        pir_read();
    }
    else
    {
        printf("未知主题\n");
    }
    MQTTClient_freeMessage(&message);   //释放内存 
    MQTTClient_free(topicName);         //释放内存 
    return 1; 
} 

static void connlost(void *context, char *cause) 
{ 
    printf("\nConnection lost\n"); 
    printf("    cause: %s\n", cause); 
} 

int mqtt_start(void)
{
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer; 
    MQTTClient_willOptions will_opts = MQTTClient_willOptions_initializer; 
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient client; 
    int ret;

    //1,创建mqtt客户端对象
    ret = MQTTClient_create(&client,SERVER_ADDRESS,CLIENTID,MQTTCLIENT_PERSISTENCE_DEFAULT,NULL);     
    if(ret != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to create client, return code %d\n", ret); 
        return EXIT_FAILURE; 
    }

    //2,设置回调函数(**函数名本身，就代表这个函数的地址**。)
    ret = MQTTClient_setCallbacks(client,NULL,connlost,msgarrvd,delivered);
    if(ret != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to set callbacks, return code %d\n", ret); 
        return EXIT_FAILURE; 
    }

    //3,连接服务端
    will_opts.topicName = "遗嘱";  
    will_opts.message = "goodbye!!!"; 
    will_opts.retained = 1;       //遗嘱保留消息 
    will_opts.qos = 0;    

    conn_opts.will = &will_opts; 
    conn_opts.keepAliveInterval = 30;      //客户端keepAlive间隔时间 
    conn_opts.cleansession = 1;            //客户端cleanSession标志 
    conn_opts.username = "lxd";     //用户名 
    conn_opts.password = "040312";      //密码 

    ret = MQTTClient_connect(client, &conn_opts);
    if(ret != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", ret); 
        return EXIT_FAILURE; 
    }

    printf("MQTT服务器连接成功!\n");

    //4,订阅主题
    ret = MQTTClient_subscribe(client, "灯光控制", 0);
    if(ret != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to subscribe1, return code %d\n", ret);
        return EXIT_FAILURE;    
    }
    ret = MQTTClient_subscribe(client, "温湿度检测", 0);
    if(ret != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to subscribe2, return code %d\n", ret);
        return EXIT_FAILURE;    
    }
    ret = MQTTClient_subscribe(client, "人体感应", 0);
    if(ret != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to subscribe3, return code %d\n", ret);
        return EXIT_FAILURE;    
    }

    //5,发布上线消息
    MQTTClient_deliveryToken token; 
    pubmsg.payload = "欢迎使用智能家居系统";  //消息内容 
    pubmsg.payloadlen = 30;             //消息的长度 
    pubmsg.qos = 0;                     //QoS等级 
    pubmsg.retained = 0;                //消息的保留标志 
    
    ret = MQTTClient_publishMessage(client, CLIENTID, &pubmsg, &token);
    if(ret != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to publish message, return code %d\n", ret); 
        return EXIT_FAILURE; 
    }

    //6,定时发布温度信息
    while(1)
    {
        //读取温度信息
        MQTTClient_message temppubmsg = MQTTClient_message_initializer;
        char tempbuf[20] = {0};
        int fd = open("/sys/class/thermal/thermal_zone0/temp",O_RDONLY);
        if(fd < 0)
        {
            perror("open thermal error");
            return -1;
        }
        read(fd,tempbuf,sizeof(tempbuf));
        close(fd);

        temppubmsg.payload = tempbuf;                  //消息内容
        temppubmsg.payloadlen = strlen(tempbuf);       //消息的长度
        temppubmsg.qos = 0;                            //QoS等级
        temppubmsg.retained = 0;                       //消息的保留标志 

        ret = MQTTClient_publishMessage(client, "开发板温度信息", &temppubmsg, &token);
        if(ret != MQTTCLIENT_SUCCESS)
        {
            printf("Failed to publish temperature message, return code %d\n", ret);
            return EXIT_FAILURE;
        }

        //读取环境温湿度信息
        MQTTClient_message humipubmsg = MQTTClient_message_initializer;
        char humibuf[2] = {0};
        char buf[64]={0};
        fd = open("/dev/dht11", O_RDWR);
        ioctl(fd, 0xc0046402, humibuf);
        snprintf(buf, sizeof(buf), "环境温度: %d，环境湿度: %d", (unsigned char)humibuf[1], (unsigned char)humibuf[0]);
        humipubmsg.payload = buf;                      //消息内容
        humipubmsg.payloadlen = strlen(buf);           //消息的长度
        humipubmsg.qos = 0;                            //QoS等级
        humipubmsg.retained = 0;                       //消息的保留标志

        ret = MQTTClient_publishMessage(client, "环境温湿度信息", &humipubmsg, &token);
        if(ret != MQTTCLIENT_SUCCESS)
        {
            printf("Failed to publish humidity message, return code %d\n", ret);
            return EXIT_FAILURE;
        }   

        //读取人体感应信息
        MQTTClient_message pirpubmsg = MQTTClient_message_initializer;
        char pirbuf[1] = {0};
        fd = open("/sys/class/gpio/gpio116/value",O_RDONLY);
        if(fd < 0)
        {
            perror("open pir error");
            return -1;
        }
        read(fd,pirbuf,sizeof(pirbuf));
        close(fd);

        if(pirbuf[0] == '1')
        {
            pirpubmsg.payload = "有人经过";            //消息内容
        }
        else
        {
            pirpubmsg.payload = "无人经过";            //消息内容
        }
        pirpubmsg.payloadlen = 12;                    //消息的长度
        pirpubmsg.qos = 0;                            //QoS等级
        pirpubmsg.retained = 0;                       //消息的保留标志
        ret = MQTTClient_publishMessage(client, "人体感应信息", &pirpubmsg, &token);
        if(ret != MQTTCLIENT_SUCCESS)
        {
            printf("Failed to publish pir message, return code %d\n", ret);
            return EXIT_FAILURE;
        }

        sleep(15);
    }

    MQTTClient_disconnect(client, 1000);
    MQTTClient_destroy(&client);
    return 0;

}