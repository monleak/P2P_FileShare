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
int countFile = 0;
char* dirDownload = "";

typedef struct _file{
    char name[200]; //Đường dẫn đầy đủ của file
    char pass[50]; //Mật khẩu của file
} file;
file *files = NULL;

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
void processSendFile(char* filename, int addr, int port){
    int sendFile_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    SOCKADDR_IN saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = addr;
    int error = connect(sendFile_socket, (SOCKADDR*)&saddr, sizeof(saddr));
    if(error!=-1){
        FILE* f = fopen(filename, "rb");
        if (f != NULL)
        {
            fseek(f, 0, SEEK_END);
            int size = ftell(f);
            fseek(f, 0, SEEK_SET);
            char* data = (char*)calloc(size, 1);
            fread(data, 1, size, f);
            fclose(f);

            char header[1024];
            char* alias = (char*)calloc(1024, 1);
            if(strrchr(filename,'/') != NULL){
                strcpy(alias,strrchr(filename,'/')+1);
            } else{
                strcpy(alias,filename);
            }
            sprintf(header, "FILE %s %d", alias, size);

            send(sendFile_socket, header, strlen(header), 0);
            int sent = 0;
            while (sent < size)
            {
                int tmp = send(sendFile_socket, data + sent, size - sent, 0);
                sent += tmp;
            }
            free(data);
            data = NULL;
        }
    }
}
void processRecvFile(int cfd, char* filename, int size){
    int countFileSameName = 0;
    char* nameFile = (char*) calloc(1024,1);
    strcpy(nameFile,filename);

    while (access(nameFile, F_OK) == 0) {
        // file tồn tại
        countFileSameName++;
        sprintf(nameFile,"%s_(%d)",filename,countFileSameName);
    }

    FILE* f = fopen(nameFile,"w");
    char* data = (char *) calloc(size,1);
    int receive = 0;
    while (receive < size)
    {
        int tmp = recv(cfd, data + receive, size - receive, 0);
        receive += tmp;
    }
    fwrite(data,1,size,f);
    fclose(f);
    free(data);data=NULL;
}
void* FileShareThread(void* arg){
    int cfd = *((int*)arg);
    free(arg);
    arg = NULL;

    char buffer[1024] = { 0 };
    int r = RecvData(cfd,buffer, sizeof(buffer));
    if (r > 0){
        /* Format các gói tin gửi đến
         * Yêu cầu truyền file đến địa chỉ: SENDTO <filename> <pass> <addr> <port>
         * Header của file gửi đến: FILE <filename> <size>
         */
        if(strncmp(buffer,"SENDTO",6)==0){
            //TODO: kiểm tra lại hàm strtok (https://www.educative.io/answers/splitting-a-string-using-strtok-in-c)

            char* filename = (char*) calloc(200,1);
            char* pass = (char*) calloc(50,1);
            char* addr = (char*) calloc(100,1);
            char* port = (char*) calloc(100,1);
            strtok(buffer+6+1, " ");
            sprintf(filename,"%s",buffer+6+1);
            strtok(buffer+6+1+ strlen(filename)+1, " ");
            sprintf(pass,"%s",buffer+6+1+ strlen(filename)+1);
            strtok(buffer+6+1+ strlen(filename)+1+ strlen(pass)+1, " ");
            sprintf(addr,"%s",buffer+6+1+ strlen(filename)+1+ strlen(pass)+1);
            strtok(buffer+6+1+ strlen(filename)+1+ strlen(pass)+1+ strlen(addr)+1, " ");
            sprintf(port,"%s",buffer+6+1+ strlen(filename)+1+ strlen(pass)+1+ strlen(addr)+1);
            int check = 0;
            for(int i=0;i<countFile;i++){
                if(strcmp(files[i].name,filename) == 0 && strcmp(files[i].pass,pass) == 0){
                    check = 1;
                    break;
                }
            }
            if(check==1){
                processSendFile(filename,atoi(addr),atoi(port));
            }
            free(filename);filename=NULL;
            free(pass);pass=NULL;
            free(addr);addr=NULL;
            free(port);port=NULL;
        } else if(strncmp(buffer,"FILE",4)==0){
            char* filename = (char*) calloc(200,1);
            char* size = (char*) calloc(50,1);
            strtok(buffer+4+1, " ");
            sprintf(filename,"%s",buffer+4+1);
            strtok(buffer+4+1+ strlen(filename)+1, " ");
            sprintf(size,"%s",buffer+4+1+ strlen(filename)+1);
            processRecvFile(cfd,filename, atoi(size));
            free(filename);filename=NULL;
            free(size);size=NULL;
        }
    }
    close(cfd);
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
                //TODO: tạo luồng để nhận yêu cầu truyền file từ server hoặc nhận file gửi đến từ client khác
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
    //TODO: thiết kế luồng nhận file kiểm tra xem file truyền đến có được yêu cầu hay không
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
                char* filename = NULL;
                char* pass = NULL;
                if(strstr(command,"-p")!=NULL){
                    filename = strtok(command+8, " ");
                    pass = strtok(command+8+1+ strlen(filename)+3, " ");
                } else{
                    filename = strtok(command+8, " ");
                    pass = "****";
                }

                if (access(filename, F_OK) == 0) {
                    // file exists
                } else {
                    printf("File không tồn tại! Hãy kiểm tra lại đường dẫn\n");
                    goto NHAPLENH;
                }
                int oldSize = sizeof(file)*countFile;
                files = (file*) realloc(files,oldSize + sizeof(file));
                strcpy(files[countFile].name,filename);
                strcpy(files[countFile].pass,pass);
                countFile++;
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
