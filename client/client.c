#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_IP "192.168.68.130"  /* 开发板 IP */
#define PORT 8888

int main(int argc, char *argv[])
{
    char buf[512];

    /* 1. 建 socket */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    /* 2. 填服务端地址 */
    struct sockaddr_in server = {0};
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server.sin_addr);

    /* 3. 连接（板子没开机/没联网会在这里报错） */
    int ret;
    ret = connect(sockfd, (struct sockaddr *)&server, sizeof(server));
    if(ret < 0)
    {
        perror("connect error");
        return -1;
    }
    printf("服务器连接成功...\n\n"); 

    /* 4. 发命令 */
    while(1) 
    { 
        // 清理缓冲区 
        memset(buf, 0x0, sizeof(buf)); 
 
        // 接收用户输入的字符串数据 
        printf("Please enter a string: "); 
        fgets(buf, sizeof(buf), stdin); 
 
        // 将用户输入的数据发送给服务器 
        ret = send(sockfd, buf, strlen(buf), 0); 
        if(0 > ret){ 
            perror("send error"); 
            break; 
        } 
 
        //输入了"exit"，退出循环 
        if(0 == strncmp(buf, "exit", 4)) 
            break; 
    } 
    close(sockfd);
    return 0;
}
