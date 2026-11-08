#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "server.h"
#include "led.h"       /* 网络层调用硬件层，靠 led.h 的声明 */
#include "http.h"
#include "dht11.h"


#define BACKLOG 10

void *client_thread(void *arg)
{
    int cfd = *(int *)arg;
    char recvbuf[128] = {0};
    int ret;

    while (1) {
        memset(recvbuf, 0, sizeof(recvbuf));
        ret = recv(cfd, recvbuf, sizeof(recvbuf) - 1, 0);/* 客户端断开 → 线程结束 */
        if (ret <= 0) 
        {
            break; 
        }                
        recvbuf[ret] = '\0';
        printf("客户端发送: %s\n", recvbuf);

        if (strncmp(recvbuf, "GET ", 4) == 0)
        {
            http_handle(cfd, recvbuf);
        }
        else if (strncmp(recvbuf, "on", 2) == 0)
        {
            led_on();
        }
        else if (strncmp(recvbuf, "off", 3) == 0)
        {
            led_off();
        }  
        else if (strncmp(recvbuf, "dht11", 5) == 0)
        {
            dht11_read();
        }
        else if (strncmp(recvbuf, "trigger", 7) == 0)
        {
            led_trigger("heatbeat");
        }
        else if (strncmp(recvbuf, "exit", 4) == 0)
        {
            break;
        }
        else 
        {
            printf("unknown cmd\n");
        }
    }
    close(cfd);
    return NULL;
}

int server_start(int port)
{
    char buf[128]={0};
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
    { 
        perror("socket"); 
        exit(-1); 
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    int sockret;
    sockret = bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    if(sockret < 0)
    {
        perror("bind error");
        close(sockfd);
        return -1;
    }

    sockret = listen(sockfd,BACKLOG);
    if(sockret < 0)
    {
        perror("listen error");
        close(sockfd);
        return -1;
    }
    printf("listening on port %d\n", port);

    while (1) 
    {
        struct sockaddr_in cli={0};
        socklen_t len = sizeof(cli);
        int cfd = accept(sockfd, (struct sockaddr *)&cli, &len);
        printf("有客户端接入...\n"); 
        inet_ntop(AF_INET, &cli.sin_addr.s_addr, buf, sizeof(buf)); 
        printf("客户端主机的IP地址: %s\n", buf); 
        printf("客户端进程的端口号: %d\n", ntohs(cli.sin_port));  

        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, &cfd);
        pthread_detach(tid);   /* 线程结束自动回收，不阻塞主线程 */
    }

    close(sockfd);
    return 0;
}
