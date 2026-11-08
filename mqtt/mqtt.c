#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h> 
#include "MQTTClient.h"     //包含MQTT客户端库头文件 

#define SERVER_ADDRESS  "192.168.68.129"
#define CLIENTID        "smart_home"

static void delivered(void *context, MQTTClient_deliveryToken dt) 
{ 
    printf("Message with token value %d delivery confirmed\n", dt); 
} 

static int msgarrvd(void *context, char *topicName, int topicLen, 
            MQTTClient_message *message) 
{ 
    int fd1,fd2;
    fd1 = open("/sys/class/leds/cpu/brightness",O_WRONLY);
    fd2 = open("/sys/class/leds/cpu/trigger",O_WRONLY);
    if(strcmp("led_contral",topicName) == 0)
    {
        if(strcmp("heartbeat",message->payload) == 0)
        {
            write(fd2,"heartbeat",9);
        }
        else if(strcmp("on",message->payload) == 0)
        {
            write(fd2,"0",1);
            write(fd1,"1",1);
        }
        else if(strcmp("off",message->payload) == 0)
        {
            write(fd2,"0",1);
            write(fd1,"0",1);
        }
        else
        {
            printf("i don't no this command\n");
        }
    }
    close(fd1);
    close(fd2);
    MQTTClient_freeMessage(&message);   //释放内存 
    MQTTClient_free(topicName);         //释放内存 
    return 1; 
} 

static void connlost(void *context, char *cause) 
{ 
    printf("\nConnection lost\n"); 
    printf("    cause: %s\n", cause); 
} 

int main()
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
    will_opts.topicName = "willTopic";  
    will_opts.message = "goodbye!!!"; 
    will_opts.retained = 1;       //遗嘱保留消息 
    will_opts.qos = 0;    

    conn_opts.will = &will_opts; 
    conn_opts.keepAliveInterval = 30;      //客户端keepAlive间隔时间 
    conn_opts.cleansession = 0;            //客户端cleanSession标志 
    conn_opts.username = "lxd";     //用户名 
    conn_opts.password = "040312";      //密码 

    ret = MQTTClient_connect(client, &conn_opts);
    if(ret != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", ret); 
        return EXIT_FAILURE; 
    }

    printf("MQTT服务器连接成功!\n"); 

    //4,发布上线消息
    MQTTClient_deliveryToken token; 
    pubmsg.payload = "my name is lxd";  //消息内容 
    pubmsg.payloadlen = 14;             //消息的长度 
    pubmsg.qos = 0;                     //QoS等级 
    pubmsg.retained = 1;                //消息的保留标志 

    ret = MQTTClient_publishMessage(client, "smart_home", &pubmsg, &token);
    if(ret != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to publish message, return code %d\n", ret); 
        return EXIT_FAILURE; 
    }

    //5,订阅主题
    ret = MQTTClient_subscribe(client, "led_contral", 0);
    if(ret != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to subscribe, return code %d\n", ret); 
        return EXIT_FAILURE; 
    }

    //6,定时发布温度信息
    while(1)
    {
        MQTTClient_message temppubmsg = MQTTClient_message_initializer;

        int fd;
        char buf[64]={0};
        fd = open("/sys/class/thermal/thermal_zone0/temp",O_RDONLY);
        if(fd < 0)
        {
            perror("open thermal error");
            return -1;
        }

        read(fd,buf,sizeof(buf));
        close(fd);

        temppubmsg.payload = buf;                  //消息内容 
        temppubmsg.payloadlen = strlen(buf);       //消息的长度 
        temppubmsg.qos = 0;                        //QoS等级 
        temppubmsg.retained = 0;                   //消息的保留标志 

        ret = MQTTClient_publishMessage(client, "temperature", &temppubmsg, NULL);
        if(ret != MQTTCLIENT_SUCCESS)
        {
            printf("Failed to publish message, return code %d\n", ret); 
            return EXIT_FAILURE; 
        }
        sleep(5);
    }

}