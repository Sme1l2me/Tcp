#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024
#define FILENAME_SIZE 512
#define TRIGGER_BUF_SIZE 8
#define RETRY_TIME_SEC 1
#define ACK_MESSAGE "ACK"
//#define TRIGGER "TRIGGER"

void cleanup(int sockfd, int clientSock, FILE *file){

    if (sockfd >= 0){
        close(sockfd);
    }
    if (clientSock >= 0){
        close(clientSock);
    }
    if (file != NULL){
        fclose(file);
    }
    file = NULL;
}

int main(int argc, char *argv[])
{
    int sockfd, clientSock;
    int ret;
    //int imageCounter = 1;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t clientAddrLen;
    char buffer[BUFFER_SIZE];
    char filename[FILENAME_SIZE];
    FILE *binFile = NULL; // 初始化为 NULL

    // 检查命令行参数数量
    if (argc < 3) {
        printf("Usage: %s <server_ip> <server_port> <save_dir>\n", argv[0]);
        return 1;
    }
    
    while(1){
        // 创建TCP套接字
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("Create socket failed!");
            return 1;
        }
        
        // 配置服务器地址
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        if (inet_pton(AF_INET, argv[1], &serverAddr.sin_addr) <= 0) {
            perror("Invalid address!");
            cleanup(sockfd, clientSock, binFile);
            sleep(RETRY_TIME_SEC);
            continue;
        }
        serverAddr.sin_port = htons(atoi(argv[2]));     

        // 设置地址复用
        int reuse = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            perror("Set socket option failed!");
            cleanup(sockfd, clientSock, binFile); // 关闭套接字
            return 1;
        }

        // 将套接字绑定到服务器地址
        ret = bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
        if (ret == 0) {
            printf("Bind successful!\n");
            break;
        } else {
            perror("Bind socket failed!");
            cleanup(sockfd, clientSock, binFile); // 关闭套接字
            sleep(RETRY_TIME_SEC);
        }
    }
    
    // 监听传入的连接
    if (listen(sockfd, 1) < 0) {
        perror("Listen failed!");
        cleanup(sockfd, clientSock, binFile); // 关闭套接字
        return 1;
    }

    printf("waiting connect...\n");

    // 接受客户端连接
    clientAddrLen = sizeof(clientAddr);
    clientSock = accept(sockfd, (struct sockaddr*)&clientAddr, &clientAddrLen);
    if (clientSock < 0) {
        perror("Accept failed!");
        cleanup(sockfd, clientSock, binFile); // 关闭套接字
    }

    printf("Successs connect.\n");

    // 切换到指定目录下
    if (chdir(argv[3]) != 0){
        perror("Change dir failed!");
        cleanup(sockfd, clientSock, binFile);
    }
    
    while (1) {
        // 接收输入数据元信息
        ssize_t receivedBytes = recv(clientSock, buffer, sizeof(buffer), 0);
        if (receivedBytes <= 0) {
            // 客户端断开连接或出错，结束循环
            break;
        }

        buffer[receivedBytes] = '\0';

        char* filenameStr = strtok(buffer, ":");
        char* filesizeStr = strtok(NULL, ":");
        if(filenameStr == NULL || filesizeStr == NULL){
            continue;
        }

        // 获取文件大小
        int fileSize = atoi(filesizeStr);
        
        if (fileSize <= 0) {
            // 文件大小无效，忽略
            fprintf(stderr, "File size invalid!");
            continue;
        }

        // 发送确认信息给客户端
        ssize_t sentBytes = send(clientSock, ACK_MESSAGE, strlen(ACK_MESSAGE), 0);
        if(sentBytes != strlen(ACK_MESSAGE)){
            perror("Send ack message failed!");
            cleanup(sockfd, clientSock, binFile);
            return 1;
        }

        // 创建输入数据文件
        snprintf(filename, sizeof(filename), "%s", filenameStr);
        binFile = fopen(filename, "wb");
        if (binFile == NULL) {
            perror("Create file failed!");
            cleanup(sockfd, clientSock, binFile);
            return 1;
        }

        // 接收数据并保存到文件
        size_t bytesReceived = 0;
        while (bytesReceived < fileSize) {
            ssize_t result = recv(clientSock, buffer, sizeof(buffer), 0);
            if (result <= 0) {
                // 客户端断开连接或出错，结束循环
                break;
            }

            size_t bytesToWrite = result;
            if (bytesReceived + bytesToWrite > fileSize) {
                bytesToWrite = fileSize - bytesReceived;
            }

            ssize_t writtenBytes = fwrite(buffer, 1, bytesToWrite, binFile);
            if (writtenBytes <= 0) {
                perror("Write data failed!");
                fclose(binFile);
                binFile = NULL;
                remove(filename);
                cleanup(sockfd, clientSock, binFile);
                return 1;
            }

            bytesReceived += writtenBytes;
        }

        // 传输完成，关闭文件
        fclose(binFile);
        binFile = NULL; // 将文件指针置为 NULL
        printf("Input file %s received and saved.\n", filename);

        // 发送确认信息给客户端
        sentBytes = send(clientSock, ACK_MESSAGE, strlen(ACK_MESSAGE), 0);
        if(sentBytes != strlen(ACK_MESSAGE)){
            perror("Send ack message failed!");
            cleanup(sockfd, clientSock, binFile);
            return 1;
        }

        memset(buffer, 0, sizeof(buffer));
        memset(filename, 0, sizeof(filename));
        //printf("Data transfor finished.\n");
    }

    // // TRIGGER
    // // 发送信息给客户端
    // ssize_t byte = send(clientSock, ACK_MESSAGE, strlen(ACK_MESSAGE), 0);
    // if(byte != strlen(ACK_MESSAGE)){
    //     perror("Send ack message failed!");
    //     cleanup(sockfd, clientSock, binFile);
    //     return 1;
    // }

    // char triggerBuf[TRIGGER_BUF_SIZE];
    // memset(triggerBuf, 0, sizeof(triggerBuf));
    // ssize_t recvBytes = recv(clientSock, triggerBuf, sizeof(triggerBuf), 0);
    // if(recvBytes <= 0 ){
    //     perror("Recv trigger failed!");
    //     cleanup(sockfd, clientSock, binFile);
    //     memset(triggerBuf, 0, sizeof(triggerBuf));
    //     return 1;
    // }

    // // 触发脚本流程
    // triggerBuf[recvBytes] = '\0';
    // printf("%s\n", triggerBuf);
    // memset(triggerBuf, 0, sizeof(triggerBuf));

    close(clientSock);
    cleanup(sockfd, clientSock, binFile);
    printf("Data transfer finished.\n");
    
    return 0;
}
