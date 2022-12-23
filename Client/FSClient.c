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
#include <pthread.h>

#define INVALID_SOCKET -1
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;

int P2Pfd = -1; //socket P2P
int checkPORT = 0;

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
void* FileShareThread(void* arg){

}
void* P2PThread(void* arg){
    char* port = (char*)arg;

    int p2p_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    SOCKADDR_IN p2paddr,caddr;
    int clen = sizeof(caddr);
    p2paddr.sin_family = AF_INET;
    p2paddr.sin_port = htons(atoi(port));
    p2paddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(p2p_socket, (SOCKADDR*)&p2paddr, sizeof(p2paddr)) == 0){
        checkPORT = 1;
        listen(p2p_socket, 10);
        while (0 == 0){
            int cfd = accept(p2p_socket, (SOCKADDR*)&caddr, &clen);
            if (cfd != INVALID_SOCKET)
            {
                //TODO: tạo luồng kết nối để truyền file
                pthread_t tid = 0;
                int* arg = (int*)calloc(1, sizeof(int));
                *arg = cfd;
                pthread_create(&tid, NULL, FileShareThread, (void*)arg);
            }
        }
    }else{
        printf("\nPORT not available\n");
        exit(0);
    }
    close(p2p_socket);
}
int main(int argc, char *argv[]){
    if(argc != 2){
        printf("Usage: ./FSClient <port>\n");
        return 1;
    }
    //Tạo 1 luồng để nhận kết nối TCP
    pthread_t tid = 0;
    char* arg = argv[1];
    pthread_create(&tid, NULL, P2PThread, (void*)arg);

    //Đợi kiểm tra cổng kết nối
    while(!checkPORT){
        sleep(1);
    }

    //TODO: Thêm share tất cả file trong thư mục
    int cfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    SOCKADDR_IN saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(8889);
    saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    int error = connect(cfd, (SOCKADDR*)&saddr, sizeof(saddr));
    if (error != -1)
    {
        SendData(cfd, argv[1], strlen(argv[1]));

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
                FILE* f = fopen(command+9, "rb");
                if(f == NULL){
                    printf("File không tồn tại! Hãy kiểm tra lại đường dẫn\n");
                    goto NHAPLENH;
                } else{
                    fclose(f);
                }
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
    } else{
        printf("Không thể kết nối tới P2P Server\n");
    }
    return 0;
}
