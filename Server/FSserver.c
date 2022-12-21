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
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>

#define MAX_CONN_NUM 1024
#define INVALID_SOCKET -1
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
int countFile;

char* helpMESS = NULL;
char* formatWelcome = "Welcome to my tracking server (ID Client: %d)\n";
char* errorMess= "Something is wrong! Please contact admin.\n";
char* invalidCmd = "INVALID COMMAND!\n";

pthread_mutex_t* mutex = NULL;

//cấu trúc client để lưu trữ danh sách các tệp được chia sẻ bởi mỗi client
typedef struct _file{
    int id;
    char name[1024];
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
void processListFile(int cfd){
    char *listFile = NULL;
//    pthread_mutex_lock(mutex);
    if(files!=NULL && countFile > 0){
        char *topbotListFile = "=============================================================================\n";
        listFile = (char *) realloc(listFile, strlen(topbotListFile)+sizeof(char));
        strcat(listFile,topbotListFile);

        for(int i=0;i< countFile;i++){
//            if(files[i].id != -1){
                int oldLen = listFile == NULL ? 0 : sizeof(listFile);
                char* clientId = (char*)calloc(1, sizeof(char)*50);
                sprintf(clientId,"(Client ID %d) ",files[i].id);
                listFile = (char*)realloc(listFile, oldLen + 1* sizeof(files[i].name)+10*sizeof(char)+ strlen(clientId));
                strcat(listFile,clientId);
                strcat(listFile,"\t\t");
                strcat(listFile,files[i].name);
                strcat(listFile,"\n");
                free(clientId);clientId=NULL;
//            }
        }
        int oldLen = listFile == NULL ? 0 : sizeof(listFile);
        listFile = (char *) realloc(listFile,oldLen + strlen(topbotListFile)+sizeof(char));
        strcat(listFile,topbotListFile);
    }
//    pthread_mutex_unlock(mutex);
    if(listFile != NULL && strlen(listFile)>0){
        SendData(cfd,listFile, strlen(listFile));
        free(listFile);
        listFile=NULL;
    }else{
        SendData(cfd,"Không có file nào để hiển thị!", strlen("Không có file nào để hiển thị!"));
    }
}
void processShareFile(int cfd,char* filename){
    pthread_mutex_lock(mutex);

    int oldLen = sizeof(file)*countFile;
    files = (file*)realloc(files, oldLen + 1*sizeof(file));
    files[countFile].id = cfd;
    strcpy(files[countFile].name,filename);
    countFile++;

    pthread_mutex_unlock(mutex);

    SendData(cfd,"Share file thành công!", strlen("Share file thành công!"));
}
void* ClientThread(void* arg)
{
    int cfd = *((int*)arg);
    free(arg);
    arg = NULL;

    char* welcome = (char*)calloc(1, sizeof(char)*1024);
    sprintf(welcome,formatWelcome,cfd);
    SendData(cfd,welcome, strlen(welcome));
    free(welcome);welcome=NULL;

    if(helpMESS!=NULL){
        SendData(cfd,helpMESS, strlen(helpMESS));
    }else{
        SendData(cfd,errorMess, strlen(errorMess));
    }
    while (1){
        char buffer[1024] = { 0 };
        int r = RecvData(cfd,buffer, sizeof(buffer));
        if (r > 0){
            while(buffer[strlen(buffer)-1] == '\n' || buffer[strlen(buffer)-1] == '\r'){
                buffer[strlen(buffer)-1] = 0;
            }
            printf("Command from Client %d: %s\n",cfd,buffer);
            if(strncmp(buffer,"fs",2) != 0){
                SendData(cfd,invalidCmd, strlen(invalidCmd));
                continue;
            }
            if(strncmp(buffer,"fs help",7) == 0){
                if(helpMESS!=NULL){
                    SendData(cfd,helpMESS, strlen(helpMESS));
                } else{
                    SendData(cfd,errorMess, strlen(errorMess));
                }
            } else if(strncmp(buffer,"fs list",7) == 0){
                processListFile(cfd);
            } else if(strncmp(buffer,"fs share",8) == 0){
                processShareFile(cfd,buffer+9);
            }
            else{
                SendData(cfd,invalidCmd, strlen(invalidCmd));
            }
        }else{
            printf("A client has disconnected (ID: %d)\n",cfd);
            //TODO: fix core dump
            if(files!=NULL){
                for(int i=0;i<countFile;i++){
                    if(files[i].id == cfd){
                        memcpy(files+i*sizeof(file),files+(i+1)*sizeof(file),sizeof(file)*(countFile-i-1));
                        countFile--;
                        i--;
                    }
                }
            }
            break;
        }
    }
    close(cfd);
}

int main() {
    mutex = (pthread_mutex_t*)calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_init(mutex, NULL);

    FILE* f = fopen("./helpCommand", "rb");
    if(f!=NULL){
        fseek(f, 0, SEEK_END);
        int size = ftell(f);
        fseek(f, 0, SEEK_SET);
        helpMESS = (char*)calloc(size, 1);
        fread(helpMESS, 1, size, f);
        fclose(f);
    } else{
        printf("Thiếu file helpCommand\n");
    }

    int sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    SOCKADDR_IN saddr, caddr;
    int clen = sizeof(caddr);
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(8888);
    saddr.sin_addr.s_addr = inet_addr("127.0.0.1");;

    if (bind(sfd, (SOCKADDR*)&saddr, sizeof(saddr)) == 0)
    {
        listen(sfd, 10);
        printf("Waiting for clients ..\n\n");
        while (0 == 0){
            int cfd = accept(sfd, (SOCKADDR*)&caddr, &clen);
            if (cfd != INVALID_SOCKET)
            {
                printf("New client connected! (ID: %d)\n",cfd);
                pthread_t tid = 0;
                int* arg = (int*)calloc(1, sizeof(int));
                *arg = cfd;
                pthread_create(&tid, NULL, ClientThread, (void*)arg);
            }
        }
    }else
        printf("PORT not available\n");
    if(files!=NULL){
        free(files);
        files = NULL;
    }

    pthread_mutex_destroy(mutex);
    free(mutex);
    return 0;
}
