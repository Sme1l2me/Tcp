#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>

#define BUFFER_SIZE 1024
#define FILEPATH_SIZE 512
//#define TRIGGER_BUF_SIZE 24

int is_input_file(const char *filename) {
    // 检查文件名是否以 ".bin"或 ".txt" 结尾，来判断是否是输入文件
    return strstr(filename, ".bin") || strstr(filename, ".txt") ;
}

int main(int argc, char const *argv[]) {
    DIR *dir;
    struct dirent *ent;
    char buffer[BUFFER_SIZE];
    char confirm[BUFFER_SIZE];
    int sockfd;
    int ret = 0;
    struct sockaddr_in serverAddr;
    //const char *trigger = "TRIGGER";

    if (argc < 3) {
        printf("Usage: %s <server_ip> <server_port> <input_dir>\n", argv[0]);
        return 1;
    }

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
        return 1;
    }
    serverAddr.sin_port = htons(atoi(argv[2]));

    //Connect
    ret = connect(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (ret < 0){
        perror("Connect failed!");
        return 1;
    }

    // 打开目录并遍历文件
    dir = opendir(argv[3]);
    if (dir == NULL) {
        perror("Open directory failed!");
        return 1;
    }

    while ((ent = readdir(dir)) != NULL) {
        // 忽略当前目录和上级目录
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        // 处理输入文件
        if (is_input_file(ent->d_name)) {
            // 构造完整文件路径
            char filepath[FILEPATH_SIZE];
            snprintf(filepath, sizeof(filepath), "%s/%s", argv[3], ent->d_name);

            // 打开输入文件
            FILE *binFile = fopen(filepath, "rb");
            if (binFile == NULL) {
                perror("bin open failed!");
                closedir(dir);
                return 1;
            }

            // 获取输入数据的大小
            fseek(binFile, 0, SEEK_END);
            long binSize = ftell(binFile);
            fseek(binFile, 0, SEEK_SET);

            // 构造输入文件元信息
            char binInfo[BUFFER_SIZE];
            snprintf(binInfo, sizeof(binInfo), "%s:%ld", ent->d_name, binSize);

            // 发送输入文件元信息
            ssize_t sentBytes = send(sockfd, binInfo, strlen(binInfo), 0);
            if (sentBytes != strlen(binInfo)) {
                perror("Send bin info failed!");
                memset(binInfo, 0, sizeof(binInfo));
                close(sockfd);
                closedir(dir);
                return 1;
            }

            // 等待服务端确认接受元信息
            ssize_t recvBytes = recv(sockfd, confirm, sizeof(confirm), 0);
            if(recvBytes <= 0){
                perror("Recv confirmation failed!");
                memset(binInfo, 0, sizeof(binInfo));
                memset(confirm, 0, sizeof(confirm));
                close(sockfd);
                closedir(dir);
                return 1;
            }

            // 发送输入文件数据
            size_t bytesRead;
            ssize_t sentDataBytes;
            while ((bytesRead = fread(buffer, 1, sizeof(buffer), binFile)) > 0) {
                sentDataBytes = send(sockfd, buffer, bytesRead, 0);
                if (sentDataBytes != bytesRead) {
                    perror("Send data failed!");
                    memset(binInfo, 0, sizeof(binInfo));
                    memset(confirm, 0, sizeof(confirm));
                    memset(buffer, 0, sizeof(buffer));
                    fclose(binFile);
                    close(sockfd);
                    closedir(dir);
                    return 1;
                }
            }

            // 等待服务端确认接受数据
            recvBytes = recv(sockfd, confirm, sizeof(confirm), 0);
            if(recvBytes <= 0){
                perror("Recv data failed!");
                memset(binInfo, 0, sizeof(binInfo));
                memset(confirm, 0, sizeof(confirm));
                memset(buffer, 0, sizeof(buffer));
                fclose(binFile);
                close(sockfd);
                closedir(dir);
                return 1;
            }

            // 关闭当前输入文件
            fclose(binFile);
            printf("File %s sent.\n", filepath);

            memset(binInfo, 0, sizeof(binInfo));
            memset(confirm, 0, sizeof(confirm));
            memset(buffer, 0, sizeof(buffer));
        }
    }

    // // TRIGGER
    // // 接受服务端信息
    // ssize_t recvBytes = recv(sockfd, confirm, sizeof(confirm), 0);
    // if(recvBytes <= 0){
    //     perror("Recv confirmation failed!");
    //     memset(confirm, 0, sizeof(confirm));
    //     close(sockfd);
    //     closedir(dir);
    //     return 1;
    // }
    // memset(confirm, 0, sizeof(confirm));
    // // 发送触发
    // ssize_t sendBytes = send(sockfd, trigger, strlen(trigger), 0);
    // if(sendBytes != strlen(trigger)){
    //     perror("Send trigger failed!");
    //     //memset(trigger, 0, sizeof(trigger));
    //     closedir(dir);
    //     close(sockfd);
    //     return 1;
    // }
    //memset(trigger, 0, sizeof(trigger));
    // 关闭目录
    closedir(dir);
    // 关闭套接字
    close(sockfd);

    printf("Data transfer finished.\n");
    return 0;
}
