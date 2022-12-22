#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <malloc.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>

#define INVALID_SOCKET -1
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;

int SendData(int fd, char* data, int len)
{
    int sent = 0;
    int tmp = 0;
    do
    {
        tmp = send(fd, data + sent, len - sent, 0);
        sent += tmp;
    } while (tmp >= 0 && sent < len);
    return sent;
}

int RecvData(int fd, char* data, int maxlen)
{
    int received = 0;
    int blocksize = 1024;
    int tmp = 0;
    do
    {
        tmp = recv(fd, data + received, blocksize, 0);
        received += tmp;
    } while (tmp >= 0 && received < maxlen && tmp == blocksize);
    return received;
}

int main(){
    int cfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    SOCKADDR_IN saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(8889);
    saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    int error = connect(cfd, (SOCKADDR*)&saddr, sizeof(saddr));
    if (error != -1)
    {
        while (0 == 0){
            char buffer[1000] = { 0 };
            RecvData(cfd, buffer, sizeof(buffer));
            printf("%s\n", buffer);
            NHAPLENH:
            printf(">");
            char command[1000] = { 0 };
            fgets(command, sizeof(command), stdin);
            while(command[strlen(command)-1] == '\n' || command[strlen(command)-1] == '\r'){
                command[strlen(command)-1] = 0;
            }
            if (strncmp(command, "fs quit", 7) == 0)
            {
                printf("Disconnected!\n");
                break;
            } else if (strncmp(command, "fs share", 8) == 0)
            {
//                FILE* f = fopen(command+9, "rb");
//                if(f == NULL){
//                    printf("File không tồn tại! Hãy kiểm tra lại đường dẫn\n");
//                    goto NHAPLENH;
//                } else{
//                    fclose(f);
//                }
            } else if(strncmp(command, "fs test", 7) == 0){
                FILE* f = fopen("/home/monleak/Code/P2P-FileShare/TestCommand/test1","r");
                if(f!=NULL){
                    while(1){
                        if(!fgets(command, sizeof(command),f)){
                            break;
                        }
                        SendData(cfd,command, strlen(command));
                        RecvData(cfd, buffer, sizeof(buffer));
                    }
                    fclose(f);
                    goto NHAPLENH;
                }
            }
            SendData(cfd,command, strlen(command));
        }
        close(cfd);
    }
    return 0;
}
